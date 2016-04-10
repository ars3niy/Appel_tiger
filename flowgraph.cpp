#include "flowgraph.h"
#include <map>

namespace Optimize {

FlowGraphNode::FlowGraphNode(int _index, const Asm::Instruction* _instruction,
	IR::VirtualRegister *ignored_register)
	: index(_index), instruction(_instruction)
{
	is_reg_to_reg_assign = instruction->is_reg_to_reg_assign;
	for (IR::VirtualRegister *output: instruction->outputs)
		assert(output->getIndex() != ignored_register->getIndex());
	for (IR::VirtualRegister *input: instruction->inputs)
		if ((ignored_register == NULL) ||
				input->getIndex() != ignored_register->getIndex())
			used.push_back(input);
		else
			is_reg_to_reg_assign = false;
}

const std::vector< IR::VirtualRegister* >& FlowGraphNode::usedRegisters() const
{
	return used;
}

const std::vector< IR::VirtualRegister* >& FlowGraphNode::assignedRegisters() const
{
	return instruction->outputs;
}

FlowGraph::FlowGraph(const Asm::Instructions &code,
	IR::VirtualRegister *ignored_register)
{
	std::map<std::string, FlowGraphNode *> label_positions;
	nodecount = 0;
	FlowGraphNode *newnode = NULL;
	
	for (const Asm::Instruction &inst: code) {
		nodes.push_back(FlowGraphNode(nodecount, &inst, ignored_register));
		if (inst.label != NULL)
			label_positions[inst.label->getName()] = & nodes.back();;
		nodecount++;
	}
	
	last_instruction = newnode;
	
	for (auto node = nodes.begin();
			node != nodes.end(); ++node) {
		FlowGraphNode *current = &(*node);
		for (IR::Label *label: current->instruction->destinations)  {
			FlowGraphNode *nextnode = NULL;
			if (label == NULL) {
				auto next_iter = node;
				next_iter++;
				if (next_iter != nodes.end())
					nextnode = &(*next_iter);
			} else
				nextnode = label_positions.at(label->getName());
			
			if (nextnode != NULL) {
				current->next.push_back(& *nextnode);
				(*nextnode).previous.push_back(current);
			}
		}
	}
}

}