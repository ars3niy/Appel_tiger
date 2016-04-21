#ifndef _ASSEMBLER_H
#define _ASSEMBLER_H

#include "intermediate.h"
#include "frame.h"
#include "debugprint.h"

namespace Asm {

std::string IntToStr(IR::IntegerExpression::Signed x);

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
	IR::Label *label;
	bool is_reg_to_reg_assign;
	bool jumps_to_next;
	std::vector<IR::Label *> extra_destinations;
	
	explicit Instruction(const std::string &_notation,
		bool _reg_to_reg_assign = false);
	
	Instruction(const std::string &_notation,
		const std::vector<IR::VirtualRegister *> &_inputs,
		const std::vector<IR::VirtualRegister *> &_outputs,
		bool _reg_to_reg_assign,
		const std::vector<IR::Label *> &_destinations = {},
		bool also_jump_to_next = true);
};

typedef std::list<Instruction> Instructions;

class TemplateChildInfo {
public:
	IR::VirtualRegister *value_storage;
	IR::Expression expression;
	
	TemplateChildInfo(IR::VirtualRegister *_value, IR::Expression _expr)
		: value_storage(_value), expression(_expr) {}
};

/**
 * Possibly generate template instructions different from those
 * in Template::instruction based on the child elements
 * @return true if we have done it
 */
typedef bool (*TemplateAdjuster)(const Instructions &templ, IR::IREnvironment *ir_env,
	Instructions &result, const std::list<TemplateChildInfo> &elements);

/**
 * Certain templates might be inapplicable to certain child elements
 */
typedef bool (*ExpressionMatchChecker)(const IR::Expression expression);

class Template: public DebugPrinter {
public:
	IR::Code code;
	/**
	 * inputs and outputs for the template instruction are additional inputs and outputs
	 * appended after the instantiated inputs and output
	 * extra_destination is either empty for "none" or non-empty for "all extra
	 * destinations for that IR::(Jump/CondJump)Statement"
	 */
	Instructions instructions;
	TemplateAdjuster adjuster_fcn;
	ExpressionMatchChecker match_fcn;
	
	static std::string Input(int ind)
	{
		return "&" + IntToStr(ind);
	}

	static std::string InputAssigned(int ind)
	{
		return "&w" + IntToStr(ind);
	}

	static std::string Output()
	{
		return "&o";
	}
	
	static std::string XOutput(int ind)
	{
		return "&O" + IntToStr(ind);
	}
	
	static std::string XInput(int ind)
	{
		return "&I" + IntToStr(ind);
	}
	
// 	static std::string XInput(int ind)
// 	{
// 		return "&I" + IntToStr(ind);
// 	}
// 
// 	static std::string XOutput(int ind)
// 	{
// 		return "&O" + IntToStr(ind);
// 	}
	
	Template(IR::Expression expression, TemplateAdjuster _adjuster_fcn = NULL,
		ExpressionMatchChecker _match_fcn = NULL) :
		DebugPrinter("assembler.log"),
		code(std::make_shared<IR::ExpressionCode>(expression)),
		adjuster_fcn(_adjuster_fcn),
		match_fcn(_match_fcn) {}

	Template(IR::Statement statement) : 
		DebugPrinter("assembler.log"),
		code(std::make_shared<IR::StatementCode>(statement)),
		adjuster_fcn(NULL), match_fcn(NULL) {}

	Template(const std::string &_command, bool is_reg_to_reg_assign, IR::Expression expression) :
		DebugPrinter("assembler.log"),
		code(std::make_shared<IR::ExpressionCode>(expression)),
		adjuster_fcn(NULL), match_fcn(NULL)
	{
		instructions.push_back(Instruction(_command, is_reg_to_reg_assign));
	}

	Template(const std::string &_command, bool is_reg_to_reg_assign, IR::Statement statement) :
		DebugPrinter("assembler.log"),
		code(std::make_shared<IR::StatementCode>(statement)),
		adjuster_fcn(NULL), match_fcn(NULL)
	{
		instructions.push_back(Instruction(_command, is_reg_to_reg_assign));
	}

	Template(const std::string &_command, bool is_reg_to_reg_assign, IR::Code _code) :
		DebugPrinter("assembler.log"),
		code(_code),
		adjuster_fcn(NULL), match_fcn(NULL)
	{
		instructions.push_back(Instruction(_command, is_reg_to_reg_assign));
	}
	
	Template(const std::vector<std::string> &_commands, IR::Expression expression) :
		DebugPrinter("assembler.log"),
		code(std::make_shared<IR::ExpressionCode>(expression)),
		adjuster_fcn(NULL), match_fcn(NULL)
	{
		for (const std::string &cmd: _commands)
			instructions.push_back(Instruction(cmd));
	}

	Template(const std::vector<std::string> &_commands, IR::Statement statement) :
		DebugPrinter("assembler.log"),
		code(std::make_shared<IR::StatementCode>(statement)),
		adjuster_fcn(NULL), match_fcn(NULL)
	{
		for (const std::string &cmd: _commands)
			instructions.push_back(Instruction(cmd));
	}

	Template(const std::vector<std::string> &_commands, IR::Code _code) :
		DebugPrinter("assembler.log"),
		code(_code),
		adjuster_fcn(NULL), match_fcn(NULL)
	{
		for (const std::string &cmd: _commands)
			instructions.push_back(Instruction(cmd));
	}
	
	void add(const std::string &command, bool _reg_to_reg_assign = false)
	{
		instructions.push_back(Instruction(command, _reg_to_reg_assign));
	}
	
	void add(const std::string &_notation,
		const std::vector<IR::VirtualRegister *> &_inputs,
		const std::vector<IR::VirtualRegister *> &_outputs,
		bool _reg_to_reg_assign,
		const std::vector<IR::Label *> &_destinations = {},
		bool also_jump_to_next = true)
	{
		instructions.push_back(Instruction(_notation, _inputs, _outputs,
			_reg_to_reg_assign, _destinations, also_jump_to_next));
	}
	
	void implement(IR::IREnvironment *ir_env, Instructions &result,
		const std::list<TemplateChildInfo> &elements,
		IR::VirtualRegister *value_storage, const std::vector<IR::Label *> &extra_dest = {});
};

enum TemplateListKind {
	TL_LIST,
	TL_OP_KIND_KIND_MAP,
};

class TemplateStorage {
public:
	TemplateListKind kind;
	
	TemplateStorage(TemplateListKind _kind) : kind(_kind) {}
	virtual ~TemplateStorage() {}
};

class TemplateList: public TemplateStorage {
public:
	std::list<std::shared_ptr<Template> > list;

	TemplateList() : TemplateStorage(TL_LIST) {}
};

class TemplateOpKindKindMap: public TemplateStorage {
public:
	std::vector<std::list<std::shared_ptr<Template> > > map;
	int operation_count;

	TemplateOpKindKindMap(int _operation_count) :
		TemplateStorage(TL_OP_KIND_KIND_MAP), operation_count(_operation_count)
	{
		map.resize(operation_count/* * IR::IR_EXPR_MAX * IR::IR_EXPR_MAX*/);
	}
	
	std::list<std::shared_ptr<Template> > *getList(int op/*, int kind1, int kind2*/)
	{
		return &(map[op/* * IR::IR_EXPR_MAX * IR::IR_EXPR_MAX +
			kind1 * IR::IR_EXPR_MAX + kind2*/]);
	}
};

class Assembler: public DebugPrinter {
protected:
	class Operand {
	private:
		IR::Expression expr;
		std::string format_string;
		std::vector<int> elements;
		
	public:
		Operand(const char *_format,
				const std::vector<int> &_elements,
				IR::Expression _expr
   			) :
			expr(_expr), format_string(_format), elements(_elements) {}
		
		std::string implement(int first_index) const;
		const IR::Expression expression() const {return expr;}
		int elementCount() const {return elements.size();}
	};
	
	IR::IREnvironment *IRenvironment;
	std::vector<TemplateStorage *> expr_templates;
	std::vector<TemplateStorage *> statm_templates;
	
	std::list<std::shared_ptr<Template> > *getTemplatesList(IR::Expression expr);
	std::list<std::shared_ptr<Template> > *getTemplatesList(IR::Statement statm);
	std::list<std::shared_ptr<Template> > *getTemplatesList(IR::Code code);
	void addTemplate(const std::string &_command, IR::Expression expr);
	void addTemplate(const std::string &_command, IR::Statement statm);
	void addTemplate(const std::string &_command, bool is_reg_to_reg_assign, IR::Expression expr);
	void addTemplate(const std::string &_command, bool is_reg_to_reg_assign, IR::Statement statm);
	void addTemplate(const std::vector<std::string> &_commands, IR::Expression expr);
	void addTemplate(const std::vector<std::string> &_commands, IR::Statement statm);
	void addTemplate(const std::vector<std::string> &_commands,
		const std::vector<std::vector<IR::VirtualRegister *> > &extra_inputs,
		const std::vector<std::vector<IR::VirtualRegister *> > &extra_outputs,
		const std::vector<bool> is_reg_to_reg_assign, IR::Expression expr,
		TemplateAdjuster adjuster_fcn = NULL, ExpressionMatchChecker match_fcn = NULL);
	void addTemplate(const std::vector<std::string> &_commands,
		const std::vector<bool> &is_jump_to_extra_dest,
		const std::vector<bool> &jumps_to_next, IR::Statement statm);
	void addTemplate(std::shared_ptr<Template> templ);
	
	/*virtual void translateExpressionTemplate(IR::Expression templ,
		IR::AbstractFrame *frame, IR::VirtualRegister *result_storage,
		const std::list<TemplateChildInfo> &children, Instructions &result) = 0;
	virtual void translateStatementTemplate(IR::Statement templ,
		const std::list<TemplateChildInfo> &children, Instructions &result) = 0;*/
	virtual void placeCallArguments(const std::list<IR::Expression> &arguments,
		IR::AbstractFrame *frame,
		const IR::Expression parentfp_param, Instructions &result) = 0;
	virtual void removeCallArguments(const std::list<IR::Expression> &arguments,
		Instructions &result) = 0;
	virtual void translateBlob(const IR::Blob &blob, Instructions &result) = 0;
	
	virtual void getCodeSectionHeader(std::string &header) = 0;
	virtual void getBlobSectionHeader(std::string &header) = 0;
	virtual void functionPrologue(IR::Label *fcn_label,
		IR::AbstractFrame *frame, Instructions &result,
		std::vector<IR::VirtualRegister *> &prologue_regs) = 0;
	virtual void functionEpilogue(IR::AbstractFrame *frame, 
		IR::VirtualRegister *result_storage,
		std::vector<IR::VirtualRegister *> &prologue_regs,
		Instructions &result) = 0;
	virtual void framePrologue(IR::Label *label, IR::AbstractFrame *frame,
		Instructions &result) = 0;
	virtual void frameEpilogue(IR::AbstractFrame *frame, Instructions &result) = 0;
	virtual void programPrologue(IR::AbstractFrame *frame, Instructions &result) = 0;
	virtual void programEpilogue(IR::AbstractFrame *frame, Instructions &result) = 0;
	virtual void implementFramePointer(IR::AbstractFrame *frame,
		Instructions &result) = 0;
private:
	int finding_time;
	std::shared_ptr<Template> FindExpressionTemplate(IR::Expression expression);
	std::shared_ptr<Template> FindStatementTemplate(IR::Statement statement);
	bool MatchExpression(IR::Expression expression, IR::Expression templ,
		int &nodecount, std::list<TemplateChildInfo> *children = NULL);
	bool MatchMoveDestination(IR::Expression expression, IR::Expression templ,
		int &nodecount, std::list<TemplateChildInfo> *children = NULL);
	bool MatchStatement(IR::Statement statement, IR::Statement templ,
		int &nodecount, std::list<TemplateChildInfo> *children = NULL);
protected:
	void translateExpression(IR::Expression expression, IR::AbstractFrame *frame,
		IR::VirtualRegister *value_storage, Instructions &result);
	void translateStatement(IR::Statement statement, IR::AbstractFrame *frame,
		Instructions &result);
public:
	Assembler(IR::IREnvironment *ir_env);
	~Assembler();
	int findingTime() {return finding_time;}
	
	void translateProgram(IR::Statement program, IR::AbstractFrame *frame,
		Instructions &result);
	void implementProgramFrameSize(IR::AbstractFrame *frame,
		Instructions &result);
	void translateFunctionBody(IR::Code code,  IR::Label *fcn_label,
		IR::AbstractFrame *frame, Instructions &result);
	void implementFunctionFrameSize(IR::Label *fcn_label, IR::AbstractFrame *frame,
		Instructions &result);
	
	void outputCode(FILE *output,
		const std::list<Instructions> &code,
		const IR::RegisterMap *register_map);
	void outputBlobs(FILE *output, const std::list<IR::Blob> &blobs);
	
	/**
	 * Frame pointer register must NOT be one of them
	 */
	virtual const std::vector<IR::VirtualRegister *> &getAvailableRegisters() = 0;
	
	virtual void spillRegister(IR::AbstractFrame *frame, Instructions &code,
		IR::VirtualRegister *reg) = 0;
	virtual bool canReverseCondJump(const Instruction &jump) const = 0;
	virtual void reverseCondJump(Instruction &jump, IR::Label *new_destination) const = 0;
};


}

#endif
