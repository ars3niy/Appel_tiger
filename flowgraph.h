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
	
	/*enum {N_FIXED = 5};
	FlowGraphNode *fixed_prev[N_FIXED];//, *fixed_next[N_FIXED];
	std::vector<FlowGraphNode *> many_prev;//, many_next;
	int nprev;//, nnext;
	bool overflow_prev;//, overflow_next;*/
	
public:
	int index;
	std::list<FlowGraphNode *> previous;
	//FlowGraphNode *__prev;
	
	FlowGraphNode(int _index, const Asm::Instruction *_instruction,
		const IR::VirtualRegister *ignored_register);
	
	/*void addprev(FlowGraphNode *node)
	{
		if (nprev == N_FIXED) {
			overflow_prev = true;
			many_prev.reserve(2*N_FIXED);
			many_prev.resize(N_FIXED);
			memmove(many_prev.data(), fixed_prev, N_FIXED*sizeof(fixed_prev[0]));
		}
		
		if (! overflow_prev)
			fixed_prev[nprev] = node;
		else
			many_prev.push_back(node);
		nprev++;
	}*/
	
	/*void addnext(FlowGraphNode *node)
	{
		if (nnext == N_FIXED) {
			overflow_next = true;
			many_next.reserve(2*N_FIXED);
			many_next.resize(N_FIXED);
			memmove(many_next.data(), fixed_next, N_FIXED*sizeof(fixed_next[0]));
		}
		
		if (! overflow_next)
			fixed_next[nnext] = node;
		else
			many_next.push_back(node);
		nnext++;
	}*/
	
	//int nextCount() const {return nnext;}
	/*int prevCount() const {return nprev;}
	
	FlowGraphNode *const* previous() const
	{
		if (overflow_prev)
			return many_prev.data();
		else
			return &fixed_prev[0];
	}*/
	
	/*FlowGraphNode *const* next() const
	{
		if (overflow_next)
			return many_next.data();
		else
			return &fixed_next[0];
	}*/
	
	const std::vector<IR::VirtualRegister *> &usedRegisters() const;
	const std::vector<IR::VirtualRegister *> &assignedRegisters() const;
	bool isRegToRegAssignment() const {return is_reg_to_reg_assign;}
};

class FlowGraph {
public:
	typedef std::list<FlowGraphNode> NodeList;
private:
	NodeList nodes;
	int nodecount;
public:
	/**
	 * Frame pointer should be ignored_register because it's not
	 * handled by the register allocator
	 */
	FlowGraph(const Asm::Instructions &code,
		const IR::VirtualRegister *ignored_register);
	int nodeCount() const {return nodecount;}
	const NodeList &getNodes() const {return nodes;}
};

};

#endif // FLOWGRAPH_H
