#ifndef _INTERMEDIATE_H
#define _INTERMEDIATE_H

#include <list>
#include <assert.h>
#include <string>
#include <stdio.h>
#include <vector>
#include <memory>
#include "debugprint.h"

namespace IR { // Intermediate Representation

class Label {
private:
	int index;
	std::string name;
public:
	Label(int _index);
	
	Label(int _index, const std::string &_name) : index(_index), name(_name) {}
	
	/**
	 * Sequential number, from 0 to total number of labels - 1
	 */
	int getIndex() {return index;}
	
	const std::string &getName() const {return name;}
	
	void appendToName(const std::string &s)
	{
		name += s;
	}
};

class LabelFactory {
private:
	std::list<Label> labels;
public:
	Label *addLabel();
	Label *addLabel(const std::string &name);
};

class AbstractVarLocation;

class VirtualRegister {
private:
	int index;
	std::string name;
	AbstractVarLocation *prespilled_location;
public:
	VirtualRegister(int _index, const std::string &_name) :
		index(_index), name(_name), prespilled_location(NULL) {}
	VirtualRegister(int _index);
	
	void prespill(AbstractVarLocation *location) {prespilled_location = location;}
	
	bool isPrespilled() {return prespilled_location != NULL;}
	AbstractVarLocation *getPrespilledLocation() {return prespilled_location;}
	
	/**
	 * Sequential number, from 0 to total number of virtual registers - 1
	 */
	int getIndex() {return index;}
	
	const std::string &getName() {return name;}
};

typedef std::vector<IR::VirtualRegister *> RegisterMap;

class RegisterFactory: public DebugPrinter {
private:
	std::list<VirtualRegister> registers;
public:
	RegisterFactory() : DebugPrinter("registers.log") {}
	VirtualRegister *addRegister();
	VirtualRegister *addRegister(const std::string &name);
};

enum ExpressionKind {
	// Expressions with value
	IR_INTEGER,  // int value
	IR_LABELADDR,// label from list
	IR_REGISTER, // pseudo-register from list
	IR_BINARYOP, // two expressions and an operation
	IR_MEMORY,   // expression for address
	IR_FUN_CALL, // expression for function, list of expressions for arguments
	IR_STAT_EXP_SEQ, // statement, expression
	IR_EXPR_MAX
};

enum StatementKind {
	// Statements - no value
	IR_MOVE,     // REGISTER or MEMORY expression, expression
	IR_EXP_IGNORE_RESULT, // expression
	IR_JUMP,     // expression, list of possible destination labels
	IR_COND_JUMP,// two expressions, comparisen operation, true label, false label
	IR_STAT_SEQ, // list of statementsn
	IR_LABEL,     // label, places it
	IR_STAT_MAX
};

enum BinaryOp {
	OP_PLUS,
	OP_MINUS,
	OP_MUL,
	OP_DIV,
	OP_AND,
	OP_OR,
	OP_XOR,
	OP_SHL,
	OP_SHR,
	OP_SHAR,
	BINOP_MAX
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
	OP_UGREATEQUAL,
	COMPOP_MAX
};

class Statement;

class Expression {
public:
	ExpressionKind kind;
	
	Expression(ExpressionKind _kind) : kind(_kind) {}
};

class IntegerExpression: public Expression {
public:
	int value;
	
	IntegerExpression(int _value) : Expression(IR_INTEGER), value(_value) {}
};

class LabelAddressExpression: public Expression {
public:
	Label *label;
	
	LabelAddressExpression(Label *_label) : Expression(IR_LABELADDR), label(_label) {}
};

class RegisterExpression: public Expression {
public:
	VirtualRegister *reg;
	
	RegisterExpression(VirtualRegister *_reg) : Expression(IR_REGISTER), reg(_reg) {}
};

class BinaryOpExpression: public Expression {
public:
	BinaryOp operation;
	Expression *left, *right;
	
    BinaryOpExpression(BinaryOp _op, Expression *_left, Expression *_right) :
		Expression(IR_BINARYOP), operation(_op), left(_left), right(_right) {}
};

class MemoryExpression: public Expression {
public:
	Expression *address;
	
    MemoryExpression(Expression *_address) : Expression(IR_MEMORY),
		address(_address) {}
};

class CallExpression: public Expression {
public:
	Expression *function;
	std::list<Expression *> arguments;
	Expression *callee_parentfp;
	
	CallExpression(Expression *_func, Expression *_callee_parentfp) :
		Expression(IR_FUN_CALL), function(_func), callee_parentfp(_callee_parentfp)
	{}
	
	void addArgument(Expression *arg)
	{
		arguments.push_back(arg);
	}
};

class StatExpSequence: public Expression {
public:
	Statement *stat;
	Expression *exp;
	
    StatExpSequence(Statement *_stat, Expression *_exp) :
		Expression(IR_STAT_EXP_SEQ), stat(_stat), exp(_exp) {}
};

#define DECLARE_EXPRESSION_CONVERSION(type, code) \
static inline type *To ## type(Expression *e) \
{ \
	assert(e->kind == code); \
	return (type *)e; \
}

DECLARE_EXPRESSION_CONVERSION(IntegerExpression, IR_INTEGER);
DECLARE_EXPRESSION_CONVERSION(LabelAddressExpression, IR_LABELADDR);
DECLARE_EXPRESSION_CONVERSION(RegisterExpression, IR_REGISTER);
DECLARE_EXPRESSION_CONVERSION(BinaryOpExpression, IR_BINARYOP);
DECLARE_EXPRESSION_CONVERSION(MemoryExpression, IR_MEMORY);
DECLARE_EXPRESSION_CONVERSION(CallExpression, IR_FUN_CALL);
DECLARE_EXPRESSION_CONVERSION(StatExpSequence, IR_STAT_EXP_SEQ);

#undef DECLARE_EXPRESSION_CONVERSION

class Statement {
public:
	StatementKind kind;
	
	Statement(StatementKind _kind) : kind(_kind) {}
};

class MoveStatement: public Statement {
public:
	Expression *to, *from;
	
    MoveStatement(Expression *_to, Expression *_from) :
		Statement(IR_MOVE), to(_to), from(_from)
	{}
};

class ExpressionStatement: public Statement {
public:
	Expression *exp;
	
    ExpressionStatement(Expression *_exp) : Statement(IR_EXP_IGNORE_RESULT),
		exp(_exp) {}
};

class JumpStatement: public Statement {
public:
	Expression *dest;
	
	std::list<Label *> possible_results;
	
    JumpStatement(Expression *_dest) : Statement(IR_JUMP), dest(_dest) {}
    JumpStatement(Expression *_dest, Label *only_option) :
		Statement(IR_JUMP), dest(_dest)
	{
		possible_results.push_back(only_option);
	}
    void addPossibleResult(Label *result)
	{
		possible_results.push_back(result);
	}
};

class CondJumpStatement: public Statement {
public:
	ComparisonOp comparison;
	Expression *left, *right;
	Label *true_dest, *false_dest;
	
	CondJumpStatement(ComparisonOp _comparison, Expression *_left,
			Expression *_right, Label *_true_dest, Label *_false_dest) :
		Statement(IR_COND_JUMP), comparison(_comparison), left(_left),
		right(_right), true_dest(_true_dest), false_dest(_false_dest) {}
};

void FlipComparison(CondJumpStatement *jump);

class StatementSequence: public Statement {
public:
	std::list<Statement *>statements;
	
    StatementSequence() : Statement(IR_STAT_SEQ) {}
    void addStatement(Statement *stat)
	{
		statements.push_back(stat);
	}
};

class LabelPlacementStatement: public Statement {
public:
	Label *label;
	
    LabelPlacementStatement(Label *_label) : Statement(IR_LABEL), label(_label) {}
};

#define DECLARE_STATEMENT_CONVERSION(type, code) \
static inline type *To ## type(Statement *s) \
{ \
	assert(s->kind == code); \
	return (type *)s; \
}

DECLARE_STATEMENT_CONVERSION(MoveStatement, IR_MOVE);
DECLARE_STATEMENT_CONVERSION(ExpressionStatement, IR_EXP_IGNORE_RESULT);
DECLARE_STATEMENT_CONVERSION(JumpStatement, IR_JUMP);
DECLARE_STATEMENT_CONVERSION(CondJumpStatement, IR_COND_JUMP);
DECLARE_STATEMENT_CONVERSION(StatementSequence, IR_STAT_SEQ);
DECLARE_STATEMENT_CONVERSION(LabelPlacementStatement, IR_LABEL);

#undef DECLARE_STATEMENT_CONVERSION

enum NodeKind {
	CODE_EXPRESSION,
	CODE_STATEMENT,
	CODE_JUMP_WITH_PATCHES
};

class Code {
public:
	NodeKind kind;
	
	Code(NodeKind _kind) : kind(_kind) {}
};

class ExpressionCode: public Code {
public:
	Expression *exp;
	
	ExpressionCode(Expression *_exp) : Code(CODE_EXPRESSION), exp(_exp) {}
};

class StatementCode: public Code {
public:
	Statement *statm;
	
	StatementCode(Statement *_statm) : Code(CODE_STATEMENT), statm(_statm) {}
};

class CondJumpPatchesCode: public StatementCode {
public:
	std::list<Label**> replace_true, replace_false;
	
    CondJumpPatchesCode(Statement *_statm) : StatementCode(_statm)
	{
		kind = CODE_JUMP_WITH_PATCHES;
	}
};

/**
 * Recursively delete the entire tree
 */
void DestroyExpression(Expression *&expression);
void DestroyStatement(Statement *&statement);
void DestroyCode(Code *&code);

void putLabels(const std::list<Label**> &replace_true, 
	const std::list<Label**> &replace_false, Label *truelabel, Label *falselabel);

struct Blob {
	Label *label;
	std::vector<unsigned char> data;
};

class IREnvironment {
private:
	LabelFactory labels;
	RegisterFactory registers;
	std::list<Blob >blobs;
public:
	Label *addLabel() {return labels.addLabel();}
	Label *addLabel(const std::string &name) {return labels.addLabel(name);}
	VirtualRegister *addRegister() {return registers.addRegister();}
	VirtualRegister *addRegister(const std::string &name) {return registers.addRegister(name);}
	Blob *addBlob();
	const std::list<Blob> &getBlobs() {return blobs;}
	void printBlobs(FILE *out);
	
	Expression *killCodeToExpression(Code *&code);
	Statement *killCodeToStatement(Code *&code);
	Statement *killCodeToCondJump(Code *&code, std::list<Label**> &replace_true,
		std::list<Label**> &replace_false);
};

void PrintCode(FILE *out, Code *code, int indent = 0);
void PrintStatement(FILE *out, Statement *statm, int indent=0,
	const char *prefix = "");

};

#endif
