#ifndef _ASSEMBLER_H
#define _ASSEMBLER_H

#include "intermediate.h"

namespace Asm {

struct InstructionTemplate {
	int instruction;
	IR::Code *code;
	
	InstructionTemplate(int _inst, IR::Expression *expression) :
		instruction(_inst), code(new IR::ExpressionCode(expression))
	{}

	InstructionTemplate(int _inst, IR::Statement *statement) :
		instruction(_inst), code(new IR::StatementCode(statement))
	{}

	InstructionTemplate(int _inst, IR::Code *_code) :
		instruction(_inst), code(_code)
	{}
};

enum TemplateListKind {
	TL_LIST,
	TL_OP_KIND_KIND_MAP,
};

class TemplateStorage {
public:
	TemplateListKind kind;
	
	TemplateStorage(TemplateListKind _kind) : kind(_kind) {}
};

class TemplateList: public TemplateStorage {
public:
	std::list<InstructionTemplate> list;

	TemplateList() : TemplateStorage(TL_LIST) {}
};

class TemplateOpKindKindMap: public TemplateStorage {
public:
	std::vector<std::list<InstructionTemplate> > map;
	int operation_count;

	TemplateOpKindKindMap(int _operation_count) :
		TemplateStorage(TL_OP_KIND_KIND_MAP), operation_count(_operation_count)
	{
		map.resize(operation_count/* * IR::IR_EXPR_MAX * IR::IR_EXPR_MAX*/);
	}
	
	std::list<InstructionTemplate> *getList(int op/*, int kind1, int kind2*/)
	{
		return &(map[op/* * IR::IR_EXPR_MAX * IR::IR_EXPR_MAX +
			kind1 * IR::IR_EXPR_MAX + kind2*/]);
	}
};

typedef std::string Instruction;
typedef std::list<Instruction> Instructions;

class Assembler {
protected:
	IR::IREnvironment *IRenvironment;
	std::vector<TemplateStorage *> expr_templates;
	std::vector<TemplateStorage *> statm_templates;
	
	std::list<InstructionTemplate > *getTemplatesList(IR::Expression *expr);
	std::list<InstructionTemplate > *getTemplatesList(IR::Statement *statm);
	void addTemplate(int code, IR::Expression *expr);
	void addTemplate(int code, IR::Statement *statm);
	
	class TemplateChildInfo {
	public:
		IR::VirtualRegister *value_storage;
		IR::Expression *expression;
		
		TemplateChildInfo(IR::VirtualRegister *_value, IR::Expression *_expr)
			: value_storage(_value), expression(_expr) {}
	};

	virtual void translateExpressionTemplate(IR::Expression *templ,
		IR::VirtualRegister *result_storage,
		const std::list<TemplateChildInfo> &children, Instructions &result) = 0;
	virtual void translateStatementTemplate(IR::Statement *templ,
		const std::list<TemplateChildInfo> &children, Instructions &result) = 0;
private:
	
	void FindExpressionTemplate(IR::Expression *expression, InstructionTemplate *&templ);
	void FindStatementTemplate(IR::Statement *statement, InstructionTemplate *&templ);
	bool MatchExpression(IR::Expression *expression, IR::Expression *templ,
		int &nodecount, std::list<TemplateChildInfo> *children = NULL,
		IR::Expression **template_instantiation = NULL);
	bool MatchStatement(IR::Statement *statement, IR::Statement *templ,
		int &nodecount, std::list<TemplateChildInfo> *children = NULL,
		IR::Statement **template_instantiation = NULL);
public:
	Assembler(IR::IREnvironment *ir_env);
	
	void translateExpression(IR::Expression *expression,
		IR::VirtualRegister *value_storage, Instructions &result);
	void translateStatement(IR::Statement *statement, Instructions &result);
};


}

#endif
