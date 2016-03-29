#ifndef _INTERMEDIATE_H
#define _INTERMEDIATE_H

#include <list>
#include <assert.h>
#include <string>
#include <stdio.h>
#include <vector>

namespace IR { // Intermediate Representation

class Label {
private:
	int index;
	std::string name;
public:
	Label(int _index) : index(_index)
	{
		char s[32];
		sprintf(s, ".L%d", _index);
		name = s;
	}
	
	Label(int _index, const std::string &_name) : index(_index), name(_name) {}
	
	/**
	 * Sequential number, from 0 to total number of labels - 1
	 */
	int getIndex() {return index;}
	
	const std::string &getName() {return name;}
};

class LabelFactory {
private:
	std::list<Label> labels;
public:
	Label *addLabel();
	Label *addLabel(const std::string &name);
};

class Register {
private:
	int index;
public:
	Register(int _index) : index(_index) {}
	
	/**
	 * Sequential number, from 0 to total number of labels - 1
	 */
	int getIndex() {return index;}
};

class RegisterFactory {
private:
	std::list<Register> registers;
public:
	Register *addRegister();
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
};

enum StatementKind {
	// Statements - no value
	IR_MOVE,     // REGISTER or MEMORY expression, expression
	IR_EXP_IGNORE_RESULT, // expression
	IR_JUMP,     // expression, list of possible destination labels
	IR_COND_JUMP,// two expressions, comparisen operation, true label, false label
	IR_STAT_SEQ, // list of statementsn
	IR_LABEL     // label, places it
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
	OP_SHAR
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
	Register *reg;
	
	RegisterExpression(Register *_reg) : Expression(IR_REGISTER), reg(_reg) {}
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
	
	CallExpression(Expression *_func) : Expression(IR_FUN_CALL), function(_func) {}
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
	{
		assert((to->kind == IR_REGISTER) || (to->kind == IR_MEMORY));
	}
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
	
	void pullStatementsOutOfTwoOperands(Expression *&left,
		Expression *&right, StatementSequence *&collected_statements);
	void canonicalizeMemoryExp(Expression *&exp);
	void canonicalizeBinaryOpExp(Expression *&exp);
	void canonicalizeCallExp(Expression *&exp,
		Expression *parentExpression, Statement *parentStatement);
	void combineStatExpSequences(StatExpSequence *exp);
	
	void canonicalizeMoveStatement(Statement *&statm);
	void canonicalizeExpressionStatement(Statement *&statm);
	void canonicalizeJumpStatement(Statement *&statm);
	void canonicalizeCondJumpStatement(Statement *&statm);
	void mergeChildStatSequences(StatementSequence *statm);
public:
	Label *addLabel() {return labels.addLabel();}
	Label *addLabel(const std::string &name) {return labels.addLabel(name);}
	Register *addRegister() {return registers.addRegister();}
	Blob *addBlob();
	void printBlobs(FILE *out);
	
	Expression *killCodeToExpression(Code *&code);
	Statement *killCodeToStatement(Code *&code);
	Statement *killCodeToCondJump(Code *&code, std::list<Label**> &replace_true,
		std::list<Label**> &replace_false);

	/**
	 * Either parentExpression or parentStatement (or both) must be NULL
	 */
	void canonicalizeExpression(Expression *&exp,
		Expression *parentExpression, Statement *parentStatement);
	void canonicalizeStatement(Statement *&statm);
};

void PrintCode(FILE *out, Code *code, int indent = 0);
void PrintStatement(FILE *out, Statement *statm, int indent=0,
	const char *prefix = "");

};

#endif
