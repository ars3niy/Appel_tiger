#include "flowgraph.h"
#include <map>

namespace Optimize {

FlowGraphNode::FlowGraphNode(int _index, const Asm::Instruction* _instruction,
	const IR::VirtualRegister *ignored_register)
{
	index = _index;
	instruction = _instruction;
	//nprev = 0;
	//overflow_prev = false;
	//nnext = 0;
	//overflow_next = false;
	//sequential_next = NULL;
	//__prev = NULL;
	is_reg_to_reg_assign = instruction->is_reg_to_reg_assign;
	for (IR::VirtualRegister *output: instruction->outputs)
		assert(output->getIndex() != ignored_register->getIndex());
	used.resize(instruction->inputs.size());
	int n_used = 0;
	for (IR::VirtualRegister *input: instruction->inputs)
		if ((ignored_register == NULL) ||
				input->getIndex() != ignored_register->getIndex())
			used[n_used++] = input;
		else
			is_reg_to_reg_assign = false;
	used.resize(n_used);
}

const std::vector< IR::VirtualRegister* >& FlowGraphNode::usedRegisters() const
{
	return used;
}

const std::vector< IR::VirtualRegister* >& FlowGraphNode::assignedRegisters() const
{
	return instruction->outputs;
}

struct NodeLink {
	int labelid;
	FlowGraphNode *who_is_prev;
	
	NodeLink(int _id, FlowGraphNode *_who_prev) :
		labelid(_id), who_is_prev(_who_prev) {}
};

FlowGraph::FlowGraph(const Asm::Instructions &code,
	const IR::VirtualRegister *ignored_register)
{
	std::map<int, FlowGraphNode *> label_positions;
	nodecount = 0;
	FlowGraphNode *prev = NULL;
	std::list<NodeLink> links;
	
	for (const Asm::Instruction &inst: code) {
		nodes.push_back(FlowGraphNode(nodecount, &inst, ignored_register));
		FlowGraphNode *current = &nodes.back();
		if (inst.label != NULL)
			label_positions[inst.label->getIndex()] = & nodes.back();
		
		if (prev != NULL) {
			if (prev->instruction->jumps_to_next)
				//current->__prev = prev;
				current->previous.push_back(prev);
			for (IR::Label *label: prev->instruction->extra_destinations) {
				auto labelpos = label_positions.find(label->getIndex());
				if (labelpos != label_positions.end())
					//(*labelpos).second->__prev = prev;
					(*labelpos).second->previous.push_back(prev);
				else
					links.push_back(NodeLink(label->getIndex(), prev));
			}
		}
		
		prev = current;
		nodecount++;
	}
	
	if (prev != NULL) {
		for (IR::Label *label: prev->instruction->extra_destinations) {
			auto labelpos = label_positions.find(label->getIndex());
			if (labelpos != label_positions.end())
				(*labelpos).second->previous.push_back(prev);
			else
				links.push_back(NodeLink(label->getIndex(), prev));
		}
	}
		
	
	for (NodeLink &link: links) {
		label_positions[link.labelid]->previous.push_back(link.who_is_prev);
	}
	
	/*for (auto node = nodes.begin();
			node != nodes.end(); ++node) {
		FlowGraphNode *current = &(*node);
		for (IR::Label *label: current->instruction->destinations)  {
			FlowGraphNode *nextnode = NULL;
			if (label != NULL)
				nextnode = label_positions[label->getIndex()];
			else
				nextnode = current->sequential_next;
			
			if (nextnode != NULL) { // happens at the last node
				//current->next.push_back(nextnode);
				nextnode->previous.push_back(current);
				//current->addnext(nextnode);
				//nextnode->addprev(current);
			}
		}
	}*/
}

}
