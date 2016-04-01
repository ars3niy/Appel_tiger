#ifndef _ASSEMBLER_H
#define _ASSEMBLER_H

#include "intermediate.h"
#include "translate_utils.h"

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

//typedef std::string Instruction;

class Instruction {
public:
	static std::string Input(int number)
	{
		return std::string("&i") + (char)('0' + number);
	}
	static std::string Output(int number)
	{
		return std::string("&o") + (char)('0' + number);
	}
	
	std::string notation;
	std::vector<IR::VirtualRegister *> inputs, outputs;
	
	/**
	 * NULL element means fall through to the next instruction
	 */
	std::vector<IR::Label *> destinations;
	
	explicit Instruction(const std::string &_notation) : notation(_notation) {}
	
	Instruction(const std::string &_notation, int ninput, IR::VirtualRegister **inputs,
		int noutput, IR::VirtualRegister **outputs, int ndest = 1,
		IR::Label **_destinations = NULL);
};

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
		IR::AbstractFrame *frame, IR::VirtualRegister *result_storage,
		const std::list<TemplateChildInfo> &children, Instructions &result) = 0;
	virtual void translateStatementTemplate(IR::Statement *templ,
		const std::list<TemplateChildInfo> &children, Instructions &result) = 0;
	virtual void translateBlob(const IR::Blob &blob, Instructions &result) = 0;
	
	virtual void getCodeSectionHeader(std::string &header) = 0;
	virtual void getBlobSectionHeader(std::string &header) = 0;
	virtual void functionEpilogue(IR::VirtualRegister *result_storage,
		Instructions &result) = 0;
	virtual void programEpilogue(Instructions &result) = 0;
	void debug(const char *msg, ...);
private:
	
	void FindExpressionTemplate(IR::Expression *expression, InstructionTemplate *&templ);
	void FindStatementTemplate(IR::Statement *statement, InstructionTemplate *&templ);
	bool MatchExpression(IR::Expression *expression, IR::Expression *templ,
		int &nodecount, std::list<TemplateChildInfo> *children = NULL,
		IR::Expression **template_instantiation = NULL);
	bool MatchStatement(IR::Statement *statement, IR::Statement *templ,
		int &nodecount, std::list<TemplateChildInfo> *children = NULL,
		IR::Statement **template_instantiation = NULL);
	void translateExpression(IR::Expression *expression, IR::AbstractFrame *frame,
		IR::VirtualRegister *value_storage, Instructions &result);
	void translateStatement(IR::Statement *statement, IR::AbstractFrame *frame,
		Instructions &result);
public:
	std::string debug_output;
	Assembler(IR::IREnvironment *ir_env);
	
	void translateProgram(IR::Statement *program, IR::AbstractFrame *frame,
		Instructions &result);
	void translateFunctionBody(IR::Code *code, IR::AbstractFrame *frame,
		Instructions &result);
	
	void outputCode(FILE *output, const Instructions &code);
	void outputBlobs(FILE *output, const std::list<IR::Blob> &blobs);
};


}

#endif
