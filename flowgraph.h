#ifndef _FLOWGRAPH_H
#define _FLOWGRAPH_H

#include "assembler.h"

namespace Optimize {

class FlowGraphNode {
friend class FlowGraph;
private:
	const Asm::Instruction *instruction;
	
	/**
	 * Instruction outputs without the ignored node
	 */
	std::vector<IR::VirtualRegister *> used;
	
	/**
	 * Same as instruction->is_reg_to_reg_assign, except if assignment
	 * source is the ignored register this will be false instead
	 */
	bool is_reg_to_reg_assign;
public:
	int index;
	std::list<FlowGraphNode *> previous, next;
	
	void init(int _index, const Asm::Instruction *_instruction,
		IR::VirtualRegister *ignored_register);
	
	const std::vector<IR::VirtualRegister *> &usedRegisters() const;
	const std::vector<IR::VirtualRegister *> &assignedRegisters() const;
	bool isRegToRegAssignment() const {return is_reg_to_reg_assign;}
};

class FlowGraph {
public:
	typedef std::vector<FlowGraphNode> NodeList;
private:
	NodeList nodes;
	int nodecount;
public:
	FlowGraphNode *last_instruction;
	
	/**
	 * Frame pointer should be ignored_register because it's not
	 * handled by the register allocator
	 */
	FlowGraph(const Asm::Instructions &code,
		IR::VirtualRegister *ignored_register);
	int nodeCount() const {return nodecount;}
	const NodeList &getNodes() const {return nodes;}
};

};

#endif // FLOWGRAPH_H
