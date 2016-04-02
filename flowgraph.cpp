#include "flowgraph.h"
#include <map>

namespace Optimize {

const std::vector< IR::VirtualRegister* >& FlowGraphNode::usedRegisters() const
{
	return instruction->inputs;
}

const std::vector< IR::VirtualRegister* >& FlowGraphNode::assignedRegisters() const
{
	return instruction->outputs;
}

FlowGraph::FlowGraph(const Asm::Instructions &code)
{
	std::map<std::string, FlowGraphNode *> label_positions;
	nodecount = 0;
	FlowGraphNode *newnode = NULL;
	
	for (Asm::Instructions::const_iterator inst = code.begin();
			inst != code.end(); inst++) {
		nodes.push_back(FlowGraphNode(nodecount, &(*inst)));
		if ((*inst).label != NULL)
			label_positions[(*inst).label->getName()] = & nodes.back();;
		nodecount++;
	}
	
	last_instruction = newnode;
	
	for (std::list<FlowGraphNode>::iterator node = nodes.begin();
			node != nodes.end(); node++) {
		FlowGraphNode *current = &(*node);
		for (int i = 0; i < current->instruction->destinations.size(); i++)  {
			const IR::Label *label = current->instruction->destinations[i];
		
			FlowGraphNode *nextnode = NULL;
			if (label == NULL) {
				std::list<FlowGraphNode>::iterator next_iter = node;
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