#ifndef _FLOWGRAPH_H
#define _FLOWGRAPH_H

#include "assembler.h"

namespace Optimize {

class FlowGraphNode {
friend class FlowGraph;
private:
	const Asm::Instruction *instruction;
public:
	int index;
	std::list<FlowGraphNode *> previous, next;
	
	FlowGraphNode(int _index, const Asm::Instruction *_instruction)
		: index(_index), instruction(_instruction) {}
	const std::vector<IR::VirtualRegister *> &usedRegisters() const;
	const std::vector<IR::VirtualRegister *> &assignedRegisters() const;
	bool isRegToRegAssignment() const {return instruction->is_reg_to_reg_assign;}
};

class FlowGraph {
public:
	typedef std::list<FlowGraphNode> NodeList;
private:
	NodeList nodes;
	int nodecount;
public:
	FlowGraphNode *last_instruction;
	
	FlowGraph(const Asm::Instructions &code);
	int nodeCount() const {return nodecount;}
	const NodeList &getNodes() const {return nodes;}
};

};

#endif // FLOWGRAPH_H
