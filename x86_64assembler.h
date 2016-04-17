#ifndef _X86_64ASSEMBLER_H
#define _X86_64ASSEMBLER_H

#include "assembler.h"

namespace Asm {

class X86_64Assembler: public Assembler {
private:
	std::vector<Operand> memory_exp;
	int first_assign_by_lea, last_assign_by_lea;
	IR::Expression exp_int, exp_label, exp_register;
	std::vector<IR::VirtualRegister *>machine_registers;
	std::vector<IR::VirtualRegister *>available_registers,
		callersave_registers, calleesave_registers;
	
	void make_arithmetic(const char *command, IR::BinaryOp ir_code);
	void make_comparison(const char *jxx_cmd, IR::ComparisonOp ir_code);
	void make_assignment();
	void addInstruction(Instructions &result,
		const std::string &prefix, IR::Expression operand,
		const std::string &suffix, IR::VirtualRegister *output0,
		IR::VirtualRegister *extra_input = NULL,
		IR::VirtualRegister *extra_output = NULL,
		Instructions::iterator *insert_before = NULL);
	//void addInstruction(Instructions &result,
	//	const std::string &prefix, IR::Expression operand0,
	//	const std::string &suffix, IR::Expression operand1);
	void makeOperand(IR::Expression expression,
		std::vector<IR::VirtualRegister *> &add_inputs,
		std::string &notation);
	void addOffset(std::string &command, int inputreg_index, int offset);
	void replaceRegisterUsage(Instructions &code,
		std::list<Instruction>::iterator inst, IR::VirtualRegister *reg,
		std::shared_ptr<IR::MemoryExpression> replacement);
	void replaceRegisterAssignment(Instructions &code,
		std::list<Instruction>::iterator inst, IR::VirtualRegister *reg,
		std::shared_ptr<IR::MemoryExpression> replacement);
protected:
	/*virtual void translateExpressionTemplate(IR::Expression templ,
		IR::AbstractFrame *frame, IR::VirtualRegister *value_storage,
		const std::list<TemplateChildInfo> &children, Instructions &result);
	virtual void translateStatementTemplate(IR::Statement templ,
		const std::list<TemplateChildInfo> &children, Instructions &result);*/
	virtual void placeCallArguments(const std::list<IR::Expression> &arguments,
		IR::AbstractFrame *frame,
		const IR::Expression parentfp_param, Instructions &result);
	virtual void removeCallArguments(const std::list<IR::Expression> &arguments,
		Instructions &result);
	virtual void translateBlob(const IR::Blob &blob, Instructions &result);
	virtual void getCodeSectionHeader(std::string &header);
	virtual void getBlobSectionHeader(std::string &header);
	virtual void functionPrologue(IR::Label *fcn_label,
		IR::AbstractFrame *frame, Instructions &result,
		std::vector<IR::VirtualRegister *> &prologue_regs);
	virtual void functionEpilogue(IR::AbstractFrame *frame, 
		IR::VirtualRegister *result_storage,
		std::vector<IR::VirtualRegister *> &prologue_regs,
		Instructions &result);
	virtual void framePrologue(IR::Label *label, IR::AbstractFrame *frame,
		Instructions &result);
	virtual void frameEpilogue(IR::AbstractFrame *frame, Instructions &result);
	virtual void programPrologue(IR::AbstractFrame *frame, Instructions &result);
	virtual void programEpilogue(IR::AbstractFrame *frame, Instructions &result);
	virtual void implementFramePointer(IR::AbstractFrame *frame,
		Instructions &result);
public:
	virtual const std::vector<IR::VirtualRegister *> &getAvailableRegisters()
		{return available_registers;}
	virtual void spillRegister(IR::AbstractFrame *frame, Instructions &code,
		IR::VirtualRegister *reg);

	X86_64Assembler(IR::IREnvironment *ir_env);
};

}

#endif
