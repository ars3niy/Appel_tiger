#ifndef _X86_64ASSEMBLER_H
#define _X86_64ASSEMBLER_H

#include "assembler.h"

namespace Asm {

class X86_64Assembler: public Assembler {
private:
	std::vector<IR::Expression *> memory_exp;
	IR::Expression *exp_int, *exp_label, *exp_register;
	IR::Expression *exp_mul_index[8], *exp_mul_index_plus[8];
	
	void make_arithmetic(int asm_code, IR::BinaryOp ir_code);
	void make_comparison(int asm_code, IR::ComparisonOp ir_code);
	void make_assignment();
	std::string translateOperand(IR::Expression *expr);
protected:
	virtual void translateExpressionTemplate(IR::Expression *templ,
		IR::VirtualRegister *value_storage,
		const std::list<TemplateChildInfo> &children, Instructions &result);
	virtual void translateStatementTemplate(IR::Statement *templ,
		const std::list<TemplateChildInfo> &children, Instructions &result);
public:
	X86_64Assembler(IR::IREnvironment *ir_env);
};

/*
enum ExpressionKind {
	IR_INTEGER,  (put value)
	IR_LABELADDR,(put label name)
	IR_REGISTER, (put register)
	IR_BINARYOP,
	IR_MEMORY,   mov [...], register
	IR_FUN_CALL, mov (arg1), %rdi
	             mov (arg2), %rsi
	             mov (arg3), %rdx
	             mov (arg4), %rcx
	             mov (arg5), %r8
	             mov (arg6), %r9
	             push (arg N)
	             push (arg N-1)
	             ...
	             push (arg7)
	             mov fp, %r10
	             call (dest)
	IR_STAT_EXP_SEQ, (statements)
	                 (expression)
};

enum StatementKind {
	IR_MOVE,    mov reg, reg
	          | mov const, reg
	          | mov mem, reg
	          | mov reg, mem
	          | mov const, mem
	IR_EXP_IGNORE_RESULT (expression) unless constant or register
	IR_JUMP,     | jmp constant
	             | jmp register
	IR_COND_JUMP,  cmp reg,reg
	               jXX true
	             | cmp const,reg
	               jXX true
	             | cmp mem,reg
	               jXX true
	             | cmp reg, mem
	               jXX true
	             | cmp const, mem
	               jXX true
	IR_STAT_SEQ, 
	IR_LABEL     (label name):
};

enum BinaryOp {
	OP_PLUS, OP_MINUS: add/sub 
	OP_MUL   mov op1, %eax
	         mul op2
	OP_DIV   mov op1, %eax
	         div %op2
	
	OP_AND   I
	OP_OR    G
	OP_XOR   N
	OP_SHL   O
	OP_SHR   R
	OP_SHAR  E
};

enum ComparisonOp {
	OP_EQUAL,
	OP_NONEQUAL,
	OP_LESS,
	OP_LESSEQUAL,
	OP_GREATER,
	OP_GREATEQUAL,
	OP_ULESS,
	OP_ULESSEQUAL,
	OP_UGREATER,
	OP_UGREATEQUAL
};
*/

}

#endif
