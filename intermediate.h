#ifndef _INTERMEDIATE_H
#define _INTERMEDIATE_H

#include <list>
#include <assert.h>
#include <string>
#include <stdio.h>
#include <vector>
#include <memory>
#include "debugprint.h"
#include "errormsg.h"

namespace IR { // Intermediate Representation

class StatementClass;
class ExpressionClass;
class CodeClass;

typedef std::shared_ptr<ExpressionClass> Expression;
typedef std::shared_ptr<StatementClass> Statement;
typedef std::shared_ptr<CodeClass> Code;

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
	int getIndex() const {return index;}
	
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

class VarLocation;

class VirtualRegister {
private:
	int index;
	std::string name;
	VarLocation *prespilled_location;
public:
	VirtualRegister(int _index, const std::string &_name) :
		index(_index), name(_name), prespilled_location(NULL) {}
	VirtualRegister(int _index);
	
	void prespill(VarLocation *location) {prespilled_location = location;}
	
	bool isPrespilled() {return prespilled_location != NULL;}
	VarLocation *getPrespilledLocation() {return prespilled_location;}
	
	/**
	 * Sequential number, from 0 to total number of virtual registers - 1
	 */
	int getIndex() const {return index;}
	
	const std::string &getName() const {return name;}
};

typedef std::vector<IR::VirtualRegister *> RegisterMap;

IR::VirtualRegister *MapRegister(const IR::RegisterMap *register_map,
	IR::VirtualRegister *reg);

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
	IR_VAR_LOCATION,
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

class ExpressionClass {
public:
	ExpressionKind kind;
	Error::InputPos position;
	
	ExpressionClass(ExpressionKind _kind) : kind(_kind) {position.linenumber = -1;}
};

class IntegerExpression: public ExpressionClass {
public:
	int value;
	
	IntegerExpression(int _value) : ExpressionClass(IR_INTEGER), value(_value) {}
};

class LabelAddressExpression: public ExpressionClass {
public:
	Label *label;
	
	LabelAddressExpression(Label *_label) : ExpressionClass(IR_LABELADDR), label(_label) {}
};

class RegisterExpression: public ExpressionClass {
public:
	VirtualRegister *reg;
	
	RegisterExpression(VirtualRegister *_reg) : ExpressionClass(IR_REGISTER), reg(_reg) {}
};

class BinaryOpExpression: public ExpressionClass {
public:
	BinaryOp operation;
	Expression left, right;
	
    BinaryOpExpression(BinaryOp _op, Expression _left, Expression _right) :
		ExpressionClass(IR_BINARYOP), operation(_op), left(_left), right(_right) {}
};

class MemoryExpression: public ExpressionClass {
public:
	Expression address;
	
    MemoryExpression(Expression _address) : ExpressionClass(IR_MEMORY),
		address(_address) {}
};

class VarLocationExp: public ExpressionClass {
public:
	VarLocation *variable;
	
    VarLocationExp(VarLocation *_variable) : ExpressionClass(IR_MEMORY),
		variable(_variable) {}
};

class CallExpression: public ExpressionClass {
public:
	Expression function;
	std::list<Expression > arguments;
	Expression callee_parentfp;
	
	CallExpression(Expression _func, Expression _callee_parentfp) :
		ExpressionClass(IR_FUN_CALL), function(_func), callee_parentfp(_callee_parentfp)
	{}
	
	void addArgument(Expression arg)
	{
		arguments.push_back(arg);
	}
};

class StatExpSequence: public ExpressionClass {
public:
	Statement stat;
	Expression exp;
	
    StatExpSequence(Statement _stat, Expression _exp) :
		ExpressionClass(IR_STAT_EXP_SEQ), stat(_stat), exp(_exp) {}
};

#define DECLARE_EXPRESSION_CONVERSION(type, code) \
static inline std::shared_ptr<type> To ## type(Expression e) \
{ \
	assert(e->kind == code); \
	return std::static_pointer_cast<type>(e); \
}

DECLARE_EXPRESSION_CONVERSION(IntegerExpression, IR_INTEGER)
DECLARE_EXPRESSION_CONVERSION(LabelAddressExpression, IR_LABELADDR)
DECLARE_EXPRESSION_CONVERSION(RegisterExpression, IR_REGISTER)
DECLARE_EXPRESSION_CONVERSION(BinaryOpExpression, IR_BINARYOP)
DECLARE_EXPRESSION_CONVERSION(MemoryExpression, IR_MEMORY)
DECLARE_EXPRESSION_CONVERSION(VarLocationExp, IR_VAR_LOCATION)
DECLARE_EXPRESSION_CONVERSION(CallExpression, IR_FUN_CALL)
DECLARE_EXPRESSION_CONVERSION(StatExpSequence, IR_STAT_EXP_SEQ)

#undef DECLARE_EXPRESSION_CONVERSION

class StatementClass {
public:
	StatementKind kind;
	
	StatementClass(StatementKind _kind) : kind(_kind) {}
};

class MoveStatement: public StatementClass {
public:
	Expression to, from;
	
    MoveStatement(Expression _to, Expression _from) :
		StatementClass(IR_MOVE), to(_to), from(_from)
	{}
};

class ExpressionStatement: public StatementClass {
public:
	Expression exp;
	
    ExpressionStatement(Expression _exp) : StatementClass(IR_EXP_IGNORE_RESULT),
		exp(_exp) {}
};

class JumpStatement: public StatementClass {
public:
	Expression dest;
	
	std::list<Label *> possible_results;
	
    JumpStatement(Expression _dest) : StatementClass(IR_JUMP), dest(_dest) {}
    JumpStatement(Expression _dest, Label *only_option) :
		StatementClass(IR_JUMP), dest(_dest)
	{
		possible_results.push_back(only_option);
	}
    void addPossibleResult(Label *result)
	{
		possible_results.push_back(result);
	}
};

class CondJumpStatement: public StatementClass {
public:
	ComparisonOp comparison;
	Expression left, right;
	Label *true_dest, *false_dest;
	
	CondJumpStatement(ComparisonOp _comparison, Expression _left,
			Expression _right, Label *_true_dest, Label *_false_dest) :
		StatementClass(IR_COND_JUMP), comparison(_comparison), left(_left),
		right(_right), true_dest(_true_dest), false_dest(_false_dest) {}
};

void FlipComparison(CondJumpStatement *jump);

class StatementSequence: public StatementClass {
public:
	std::list<Statement> statements;
	
    StatementSequence() : StatementClass(IR_STAT_SEQ) {}
    void addStatement(Statement stat)
	{
		statements.push_back(stat);
	}
};

class LabelPlacementStatement: public StatementClass {
public:
	Label *label;
	
    LabelPlacementStatement(Label *_label) : StatementClass(IR_LABEL), label(_label) {}
};

#define DECLARE_STATEMENT_CONVERSION(type, code) \
static inline std::shared_ptr<type> To ## type(Statement s) \
{ \
	assert(s->kind == code); \
	return std::static_pointer_cast<type>(s); \
}

DECLARE_STATEMENT_CONVERSION(MoveStatement, IR_MOVE)
DECLARE_STATEMENT_CONVERSION(ExpressionStatement, IR_EXP_IGNORE_RESULT)
DECLARE_STATEMENT_CONVERSION(JumpStatement, IR_JUMP)
DECLARE_STATEMENT_CONVERSION(CondJumpStatement, IR_COND_JUMP)
DECLARE_STATEMENT_CONVERSION(StatementSequence, IR_STAT_SEQ)
DECLARE_STATEMENT_CONVERSION(LabelPlacementStatement, IR_LABEL)

#undef DECLARE_STATEMENT_CONVERSION

enum NodeKind {
	CODE_EXPRESSION,
	CODE_STATEMENT,
	CODE_JUMP_WITH_PATCHES
};

class CodeClass {
public:
	NodeKind kind;
	
	CodeClass(NodeKind _kind) : kind(_kind) {}
};

class ExpressionCode: public CodeClass {
public:
	Expression exp;
	
	ExpressionCode(Expression _exp) : CodeClass(CODE_EXPRESSION), exp(_exp) {}
};

class StatementCode: public CodeClass {
public:
	Statement statm;
	
	StatementCode(Statement _statm) : CodeClass(CODE_STATEMENT), statm(_statm) {}
};

class CondJumpPatchesCode: public StatementCode {
public:
	std::list<Label**> replace_true, replace_false;
	
    CondJumpPatchesCode(Statement _statm) : StatementCode(_statm)
	{
		kind = CODE_JUMP_WITH_PATCHES;
	}
};

void putLabels(const std::list<Label**> &replace_true, 
	const std::list<Label**> &replace_false, Label *truelabel, Label *falselabel);

// for dynamic_cast
class CodeWalker {
private:
	virtual void __dummy() {}
};

class CodeWalkCallbacks {
public:
	void (*doInt)(Expression &exp, Expression parent_exp,
		Statement parent_statm, CodeWalker *arg) = NULL;
	void (*doLabelAddr)(Expression &exp, Expression parent_exp,
		Statement parent_statm, CodeWalker *arg) = NULL;
	void (*doRegister)(Expression &exp, Expression parent_exp,
		Statement parent_statm, CodeWalker *arg) = NULL;
	void (*doBinOp)(Expression &exp, Expression parent_exp,
		Statement parent_statm, CodeWalker *arg) = NULL;
	void (*doMemory)(Expression &exp, Expression parent_exp,
		Statement parent_statm, CodeWalker *arg) = NULL;
	void (*doVarLocation)(Expression &exp, Expression parent_exp,
		Statement parent_statm, CodeWalker *arg) = NULL;
	void (*doCall)(Expression &exp, Expression parent_exp,
		Statement parent_statm, CodeWalker *arg) = NULL;
	void (*doStatExpSeq)(Expression &exp, Expression parent_exp,
		Statement parent_statm, CodeWalker *arg) = NULL;
	
	void (*doMove)(Statement &statm, CodeWalker *arg) = NULL;
	void (*doExpIgnoreRes)(Statement &statm, CodeWalker *arg) = NULL;
	void (*doJump)(Statement &statm, CodeWalker *arg) = NULL;
	void (*doCondJump)(Statement &statm, CodeWalker *arg) = NULL;
	void (*doSequence)(Statement &statm, CodeWalker *arg) = NULL;
	void (*doLabelPlacement)(Statement &statm, CodeWalker *arg) = NULL;
};

void walkExpression(Expression &exp, Expression parentExpression,
	Statement parentStatement, const CodeWalkCallbacks &callbacks, CodeWalker *arg);
void walkStatement(Statement &statm, const CodeWalkCallbacks &callbacks, CodeWalker *arg);
void walkCode(Code code, const CodeWalkCallbacks &callbacks, CodeWalker *arg);

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
	
	Expression codeToExpression(Code code);
	Statement codeToStatement(Code code);
	Statement codeToCondJump(Code code, std::list<Label**> &replace_true,
		std::list<Label**> &replace_false);
};

void PrintCode(FILE *out, Code code, int indent = 0);
void PrintStatement(FILE *out, Statement statm, int indent=0,
	const char *prefix = "");

};

#endif
