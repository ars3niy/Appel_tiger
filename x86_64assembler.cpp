#include "x86_64assembler.h"
#include "x86_64_frame.h"
#include "errormsg.h"
#include <list>
#include <assert.h>
#include <stdlib.h>

namespace Asm {

	enum {
		RAX = 0,
		RDX,
		RCX,
		RBX,
		RSI,
		RDI,
		R8,
		R9,
		R10,
		R11,
		R12,
		R13,
		R14,
		R15,
		RBP,
		RSP,
		FP,
		LAST_REGISTER,
	};

static int paramreg_list[] = {RDI, RSI, RDX, RCX, R8, R9};
static int paramreg_count = 6;
enum {RESULT_REG = RAX};
enum {FPLINK_REG = R10};
static int callersave_list[] = {RDI, RSI, RDX, RCX, R8, R9, RESULT_REG, FPLINK_REG, R11};
static int callersave_count = 9;
static int calleesave_list[] = {RBX, RBP, R12, R13, R14, R15};
static int calleesave_count = 6;

enum {
	I_YIELD = 1,
	I_ADD,
	I_SUB,
	I_MUL,
	I_DIV,
	I_MOV,
	I_JMP,
	I_CMPJE,
	I_CMPJNE,
	I_CMPJL,
	I_CMPJLE,
	I_CMPJG,
	I_CMPJGE,
	I_CMPJUL, // JB
	I_CMPJULE, // JBE
	I_CMPJUG, // JA
	I_CMPJUGE, // JAE
	I_LABEL,
	I_CALL
};

X86_64Assembler::X86_64Assembler(IR::IREnvironment *ir_env)
	: Assembler(ir_env)
{
	exp_int = new IR::IntegerExpression(0);
	exp_label = new IR::LabelAddressExpression(NULL);
	exp_register = new IR::RegisterExpression(NULL);
	
	memory_exp.push_back(exp_int);
	memory_exp.push_back(exp_label);
	memory_exp.push_back(exp_register);
	// reg + const, const + reg
	memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_PLUS, exp_register, exp_int));
	memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_PLUS, exp_int, exp_register));
	// reg - const
	memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_MINUS, exp_register, exp_int));
#if 0
	// reg + reg
	memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_PLUS, exp_register, exp_register));
	
	int mul_index[] = {1,2,4,8};
	for (int i = 0; i < 3; i++) {
		// reg*X, X*reg
		exp_mul_index[i] = new IR::BinaryOpExpression(IR::OP_MUL, exp_register, new IR::IntegerExpression(mul_index[i]));
		exp_mul_index[i+4] = new IR::BinaryOpExpression(IR::OP_MUL, new IR::IntegerExpression(mul_index[i]), exp_register);
		// reg*X + itself = reg*(X+1), (X+1)*reg
		exp_mul_index_plus[i] = new IR::BinaryOpExpression(IR::OP_MUL, exp_register, new IR::IntegerExpression(mul_index[i]+1));
		exp_mul_index_plus[i+4] = new IR::BinaryOpExpression(IR::OP_MUL, new IR::IntegerExpression(mul_index[i]+1), exp_register);
	}
	for (int i = 1; i < 8; i++) if (i != 4) {
		IR::Expression *mul = exp_mul_index[i];
		IR::Expression *reg = exp_register;
		IR::Expression *c = exp_int;
		
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_PLUS, mul, reg));
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_PLUS, reg, mul));
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_PLUS, mul, c));
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_MINUS, mul, c));
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_PLUS, c, mul));
		
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_PLUS, new IR::BinaryOpExpression(IR::OP_PLUS, mul, reg), c));
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_MINUS, new IR::BinaryOpExpression(IR::OP_PLUS, mul, reg), c));
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_PLUS, mul, new IR::BinaryOpExpression(IR::OP_PLUS, reg, c)));
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_PLUS, mul, new IR::BinaryOpExpression(IR::OP_MINUS, reg, c)));
		
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_PLUS, new IR::BinaryOpExpression(IR::OP_PLUS, mul, c), reg));
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_PLUS, new IR::BinaryOpExpression(IR::OP_MINUS, mul, c), reg));
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_PLUS, mul, new IR::BinaryOpExpression(IR::OP_PLUS, c, reg)));
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_MINUS, mul, new IR::BinaryOpExpression(IR::OP_MINUS, c, reg)));
		
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_PLUS, new IR::BinaryOpExpression(IR::OP_PLUS, reg, mul), c));
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_MINUS, new IR::BinaryOpExpression(IR::OP_PLUS, reg, mul), c));
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_PLUS, reg, new IR::BinaryOpExpression(IR::OP_PLUS, mul, c)));
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_MINUS, new IR::BinaryOpExpression(IR::OP_PLUS, reg, mul), c));
		
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_PLUS, new IR::BinaryOpExpression(IR::OP_PLUS, reg, c), mul));
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_PLUS, new IR::BinaryOpExpression(IR::OP_MINUS, reg, c), mul));
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_PLUS, reg, new IR::BinaryOpExpression(IR::OP_PLUS, c, mul)));
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_MINUS, reg, new IR::BinaryOpExpression(IR::OP_MINUS, c, mul)));
		
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_PLUS, new IR::BinaryOpExpression(IR::OP_PLUS, c, mul), reg));
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_PLUS, c, new IR::BinaryOpExpression(IR::OP_PLUS, mul, reg)));
		
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_PLUS, new IR::BinaryOpExpression(IR::OP_PLUS, c, reg), mul));
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_PLUS, c, new IR::BinaryOpExpression(IR::OP_PLUS, reg, mul)));
	}
	for (int i = 0; i < 8; i++) {
		memory_exp.push_back(exp_mul_index_plus[i]);
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_PLUS, exp_mul_index_plus[i], exp_int));
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_MINUS, exp_mul_index_plus[i], exp_int));
		memory_exp.push_back(new IR::BinaryOpExpression(IR::OP_PLUS, exp_int, exp_mul_index_plus[i]));
	}
#endif
	
	addTemplate(I_YIELD, exp_int);
	addTemplate(I_YIELD, exp_label);
	addTemplate(I_LABEL, new IR::LabelPlacementStatement(NULL));
	addTemplate(I_YIELD, exp_register);
	for (int i = 0; i < memory_exp.size(); i++)
		addTemplate(I_YIELD, new IR::MemoryExpression(memory_exp[i]));
	
	make_arithmetic(I_ADD, IR::OP_PLUS);
	make_arithmetic(I_SUB, IR::OP_MINUS);
	// imul
	make_arithmetic(I_MUL, IR::OP_MUL);
	// don't forget cqo!! and idivq
	make_arithmetic(I_DIV, IR::OP_DIV);
	
	make_comparison(I_CMPJE, IR::OP_EQUAL);
	make_comparison(I_CMPJNE, IR::OP_NONEQUAL);
	make_comparison(I_CMPJL, IR::OP_LESS);
	make_comparison(I_CMPJLE, IR::OP_LESSEQUAL);
	make_comparison(I_CMPJG, IR::OP_GREATER);
	make_comparison(I_CMPJGE, IR::OP_GREATEQUAL);
	make_comparison(I_CMPJUL, IR::OP_ULESS);
	make_comparison(I_CMPJULE, IR::OP_ULESSEQUAL);
	make_comparison(I_CMPJUG, IR::OP_UGREATER);
	make_comparison(I_CMPJUGE, IR::OP_UGREATEQUAL);
	make_assignment();
	
	addTemplate(I_CALL, new IR::CallExpression(new IR::LabelAddressExpression(NULL), NULL));
	addTemplate(I_CALL, new IR::CallExpression(new IR::RegisterExpression(NULL), NULL));
	addTemplate(I_JMP, new IR::JumpStatement(new IR::LabelAddressExpression(NULL)));
	addTemplate(I_JMP, new IR::JumpStatement(new IR::RegisterExpression(NULL)));
	
	static const char *register_names[] = {
		"%rax",
		"%rdx",
		"%rcx",
		"%rbx",
		"%rsi",
		"%rdi",
		"%r8",
		"%r9",
		"%r10",
		"%r11",
		"%r12",
		"%r13",
		"%r14",
		"%r15",
		"%rbp",
		"%rsp",
		"fp",
	};
	
	machine_registers.resize(LAST_REGISTER);
	
	for (int r = 0; r < LAST_REGISTER; r++) {
		machine_registers[r] = IRenvironment->addRegister(register_names[r]);
		if ((r != FP) && (r != RSP))
			available_registers.push_back(machine_registers[r]);
	}
	
	callersave_registers.resize(callersave_count);
	for (int i = 0; i < callersave_count; i++)
		callersave_registers[i] = machine_registers[callersave_list[i]];
	
	calleesave_registers.resize(calleesave_count);
	for (int i = 0; i < calleesave_count; i++)
		calleesave_registers[i] = machine_registers[calleesave_list[i]];
}

// IR::VirtualRegister* X86_64Assembler::getFramePointerRegister()
// {
// 	return machine_registers[FP];
// }
// 
void X86_64Assembler::make_arithmetic(int asm_code, IR::BinaryOp ir_code)
{
	// don't forget cqo
	addTemplate(asm_code, new IR::BinaryOpExpression(ir_code, exp_register, exp_int));
	addTemplate(asm_code, new IR::BinaryOpExpression(ir_code, exp_register, exp_register));
	for (int i = 0; i < memory_exp.size(); i++) {
		addTemplate(asm_code, new IR::BinaryOpExpression(ir_code, exp_register,
			new IR::MemoryExpression(memory_exp[i])));
		addTemplate(asm_code, new IR::BinaryOpExpression(ir_code, exp_int,
			new IR::MemoryExpression(memory_exp[i])));
		addTemplate(asm_code, new IR::BinaryOpExpression(ir_code,
			new IR::MemoryExpression(memory_exp[i]), exp_register));
		addTemplate(asm_code, new IR::BinaryOpExpression(ir_code,
			new IR::MemoryExpression(memory_exp[i]), exp_int));
	}
	for (int i = 0; i < memory_exp.size(); i++)
		for (int j = 0; j < memory_exp.size(); j++)
			addTemplate(asm_code, new IR::BinaryOpExpression(ir_code,
				new IR::MemoryExpression(memory_exp[i]), new IR::MemoryExpression(memory_exp[j])));
}

void X86_64Assembler::make_comparison(int asm_code, IR::ComparisonOp ir_code)
{
	addTemplate(asm_code, new IR::CondJumpStatement(ir_code, exp_register, exp_int, NULL, NULL));
	addTemplate(asm_code, new IR::CondJumpStatement(ir_code, exp_register, exp_register, NULL, NULL));
	for (int i = 0; i < memory_exp.size(); i++) {
		addTemplate(asm_code, new IR::CondJumpStatement(ir_code, exp_register,
			new IR::MemoryExpression(memory_exp[i]), NULL, NULL));
		addTemplate(asm_code, new IR::CondJumpStatement(ir_code, exp_int,
			new IR::MemoryExpression(memory_exp[i]), NULL, NULL));
		addTemplate(asm_code, new IR::CondJumpStatement(ir_code,
			new IR::MemoryExpression(memory_exp[i]), exp_register, NULL, NULL));
		addTemplate(asm_code, new IR::CondJumpStatement(ir_code,
			new IR::MemoryExpression(memory_exp[i]), exp_int, NULL, NULL));
	}
}

void X86_64Assembler::make_assignment()
{
	int asm_code = I_MOV;
	addTemplate(asm_code, new IR::MoveStatement(exp_register, exp_int));
	addTemplate(asm_code, new IR::MoveStatement(exp_register, exp_label));
	addTemplate(asm_code, new IR::MoveStatement(exp_register, exp_register));
	for (int i = 0; i < memory_exp.size(); i++) {
		addTemplate(asm_code, new IR::MoveStatement(exp_register,
			new IR::MemoryExpression(memory_exp[i])));
		addTemplate(asm_code, new IR::MoveStatement(exp_register, memory_exp[i]));
		addTemplate(asm_code, new IR::MoveStatement(
			new IR::MemoryExpression(memory_exp[i]), exp_register));
		addTemplate(asm_code, new IR::MoveStatement(
			new IR::MemoryExpression(memory_exp[i]), exp_int));
		addTemplate(asm_code, new IR::MoveStatement(
			new IR::MemoryExpression(memory_exp[i]), exp_label));
	}
}

static std::string IntToStr(int x)
{
	char buf[30];
	sprintf(buf, "%d", x);
	return std::string(buf);
}

void X86_64Assembler::makeOperand(IR::Expression *expression,
	std::vector<IR::VirtualRegister *> &add_inputs,
	std::string &notation)
{
	switch (expression->kind) {
		case IR::IR_INTEGER:
			notation = "$" + IntToStr(IR::ToIntegerExpression(expression)->value);
			break;
		case IR::IR_LABELADDR:
			notation = "$" + IR::ToLabelAddressExpression(expression)->label->getName();
			break;
		case IR::IR_REGISTER:
			notation = Instruction::Input(add_inputs.size());
			add_inputs.push_back(IR::ToRegisterExpression(expression)->reg);
			break;
		case IR::IR_MEMORY:
			IR::Expression *addr = IR::ToMemoryExpression(expression)->address;
			switch (addr->kind) {
				case IR::IR_REGISTER:
					notation = "(" + Instruction::Input(add_inputs.size()) + ")";
					add_inputs.push_back(IR::ToRegisterExpression(addr)->reg);
					break;
				case IR::IR_LABELADDR:
					notation = std::string("(") +
						IR::ToLabelAddressExpression(addr)->label->getName() + ")";
					break;
				case IR::IR_BINARYOP: {
					IR::BinaryOpExpression *binop = IR::ToBinaryOpExpression(addr);
					if (binop->operation == IR::OP_PLUS) {
						if (binop->left->kind == IR::IR_INTEGER) {
							IR::MemoryExpression base(binop->right);
							int offset = IR::ToIntegerExpression(binop->left)->value;
							makeOperand(&base, add_inputs, notation);
							if (offset != 0)
								notation = IntToStr(offset) + notation;
						} else if (binop->right->kind == IR::IR_INTEGER) {
							IR::MemoryExpression base(binop->left);
							int offset = IR::ToIntegerExpression(binop->right)->value;
							makeOperand(&base, add_inputs, notation);
							if (offset != 0)
								notation = IntToStr(offset) + notation;
						} else
							Error::fatalError("Too complicated x86_64 assembler memory address");
					} else if (binop->operation == IR::OP_MINUS) {
						if (binop->right->kind == IR::IR_INTEGER) {
							IR::MemoryExpression base(binop->left);
							int offset = -IR::ToIntegerExpression(binop->right)->value;
							makeOperand(&base, add_inputs, notation);
							if (offset != 0)
								notation = IntToStr(offset) + notation;
						} else
							Error::fatalError("Too complicated x86_64 assembler memory address");
					} else
						Error::fatalError("Too complicated x86_64 assembler memory address");
				}
				break;
			}
			break;
	}
}
	
void X86_64Assembler::addInstruction(Instructions &result,
	const std::string &prefix, IR::Expression *operand,
	const std::string &suffix, IR::VirtualRegister *output0,
	IR::VirtualRegister *extra_input, IR::VirtualRegister *extra_output,
	Instructions::iterator *insert_before)
{
	std::vector<IR::VirtualRegister *> inputs;
	std::string notation;
	makeOperand(operand, inputs, notation);
	if (extra_input != NULL)
		inputs.push_back(extra_input);
	notation = prefix + notation + suffix;
	IR::VirtualRegister *outputs[] = {output0, extra_output};
	int noutput = 0;
	if (output0 != NULL) {
		if (extra_output != NULL)
			noutput = 2;
		else
			noutput = 1;
	}
	Instructions::iterator position = result.end();
	if (insert_before != NULL)
		position = *insert_before;
	
	Instructions::iterator newinst = result.insert(position,
		Instruction(notation, inputs.size(), inputs.data(),
			noutput, outputs, 1, NULL,
			(prefix == "movq ") && (operand->kind == IR::IR_REGISTER)));
	if (insert_before != NULL)
		debug("Spill: %s -> prepend %s", (*position).notation.c_str(),
			  (*newinst).notation.c_str());
}

void X86_64Assembler::addInstruction(Instructions &result,
	const std::string &prefix, IR::Expression *operand0,
	const std::string &suffix, IR::Expression *operand1)
{
	std::vector<IR::VirtualRegister *> inputs;
	std::string s0;
	makeOperand(operand0, inputs, s0);
	std::string s1;
	makeOperand(operand1, inputs, s1);
	result.push_back(Instruction(prefix + s0 + suffix + s1,
		inputs.size(), inputs.data(), 0, NULL));
}

void X86_64Assembler::placeCallArguments(const std::list<IR::Expression * >& arguments,
	Instructions &result)
{
	int arg_count = 0;
	std::list<IR::Expression *>::const_iterator arg;
	for (arg = arguments.begin(); arg != arguments.end(); arg++) {
		assert((*arg)->kind == IR::IR_REGISTER);
		result.push_back(Instruction(
			"movq " + Instruction::Input(0) + ", " + Instruction::Output(0),
			1, &IR::ToRegisterExpression(*arg)->reg, 1,
			&machine_registers[paramreg_list[arg_count]],
			1, NULL, true));
		arg_count++;
		if (arg_count == paramreg_count)
			break;
	}
	if (arg_count == paramreg_count) {
		std::list<IR::Expression *>::const_iterator extra_arg = arguments.end();
		extra_arg--; 
		while (extra_arg != arg) {
			assert((*extra_arg)->kind == IR::IR_REGISTER);
			result.push_back(Instruction(
				"push " + Instruction::Input(0),
				1, &IR::ToRegisterExpression(*extra_arg)->reg, 0, NULL));
			extra_arg--;
		}
	}
}

void X86_64Assembler::removeCallArguments(const std::list< IR::Expression* >& arguments,
	Instructions &result)
{
	int stack_arg_count = arguments.size()-6;
	if (stack_arg_count > 0)
 		result.push_back(Instruction(std::string("add ") +
			IntToStr(stack_arg_count*8) + std::string(", %rsp")));
}

void X86_64Assembler::translateExpressionTemplate(IR::Expression *templ,
	IR::AbstractFrame *frame, IR::VirtualRegister *value_storage,
	const std::list<TemplateChildInfo> &children, Instructions &result)
{
	switch (templ->kind) {
		case IR::IR_INTEGER:
		case IR::IR_LABELADDR:
		case IR::IR_REGISTER:
		case IR::IR_MEMORY:
			if (value_storage != NULL) {
				addInstruction(result, "movq ", templ,
					", " + Instruction::Output(0), value_storage);
				debug("Simple expression type %d, translated as %s", templ->kind,
					result.back().notation.c_str());
			}
			break;
		case IR::IR_BINARYOP:
			if (value_storage != NULL) {
				IR::BinaryOpExpression *bin_op = IR::ToBinaryOpExpression(templ);
				debug("Translating binary operation %d translated as:", bin_op->operation);
				switch (bin_op->operation) {
					case IR::OP_PLUS:
						addInstruction(result, "movq ", bin_op->left,
							", " + Instruction::Output(0), value_storage);
						debug("\t%s", result.back().notation.c_str());
						addInstruction(result, "addq ", bin_op->right,
							", " + Instruction::Output(0), value_storage);
						debug("\t%s", result.back().notation.c_str());
						break;
					case IR::OP_MINUS:
						addInstruction(result, "movq ", bin_op->left,
							", " + Instruction::Output(0), value_storage);
						debug("\t%s", result.back().notation.c_str());
						addInstruction(result, "subq ", bin_op->right,
							", " + Instruction::Output(0), value_storage);
						debug("\t%s", result.back().notation.c_str());
						break;
					case IR::OP_MUL:
						addInstruction(result, "movq ", bin_op->left,
							", " + Instruction::Output(0), machine_registers[RAX]);
						debug("\t%s", result.back().notation.c_str());
						if ((bin_op->right->kind == IR::IR_INTEGER) ||
							(bin_op->right->kind == IR::IR_LABELADDR)) {
							IR::VirtualRegister *tmp_reg = IRenvironment->addRegister();
							addInstruction(result, "movq ", bin_op->right,
								", " + Instruction::Output(0), tmp_reg);
							debug("\t%s", result.back().notation.c_str());
							IR::VirtualRegister *inputs[] = {
								machine_registers[RAX],
								tmp_reg};
							IR::VirtualRegister *outputs[] = {
								machine_registers[RAX],
								machine_registers[RDX]};
							result.push_back(Instruction(
								"imulq " + Instruction::Input(1),
								2, &inputs[0], 2, &outputs[0]));
							debug("\t%s", result.back().notation.c_str());
						} else {
							addInstruction(result, "imulq ", bin_op->right,
								"", machine_registers[RAX],
								machine_registers[RAX], machine_registers[RDX]);
							debug("\t%s", result.back().notation.c_str());
						}
						result.push_back(Instruction("movq " +
							Instruction::Input(0) + ", " + Instruction::Output(0),
							1, &machine_registers[RAX], 1, &value_storage,
							1, NULL, true));
						debug("\t%s", result.back().notation.c_str());
						break;
					case IR::OP_DIV:
						addInstruction(result, "movq ", bin_op->left,
							", " + Instruction::Output(0), machine_registers[RAX]);
						debug("\t%s", result.back().notation.c_str());
						result.push_back(Instruction("cqo",
							0, NULL, 1, &machine_registers[RDX]));
						debug("\t%s", result.back().notation.c_str());
						if ((bin_op->right->kind == IR::IR_INTEGER) ||
							(bin_op->right->kind == IR::IR_LABELADDR)) {
							IR::VirtualRegister *tmp_reg = IRenvironment->addRegister();
							addInstruction(result, "movq ", bin_op->right,
								", " + Instruction::Output(0), tmp_reg);
							debug("\t%s", result.back().notation.c_str());
							IR::VirtualRegister *inputs[] = {
								machine_registers[RAX],
								machine_registers[RDX],
								tmp_reg};
							IR::VirtualRegister *outputs[] = {
								machine_registers[RAX],
								machine_registers[RDX]};
							result.push_back(Instruction(
								"idivq " + Instruction::Input(2),
								3, &inputs[0], 2, &outputs[0]));
						debug("\t%s", result.back().notation.c_str());
						} else {
							addInstruction(result, "idivq ", bin_op->right,
								"", machine_registers[RAX],
								machine_registers[RAX], machine_registers[RDX]);
							debug("\t%s", result.back().notation.c_str());
						}
						result.push_back(Instruction("movq " +
							Instruction::Input(0) + ", " + Instruction::Output(0),
							1, &machine_registers[RAX], 1, &value_storage,
							1, NULL, true));
						debug("\t%s", result.back().notation.c_str());
						break;
					default:
						Error::fatalError("X86_64Assembler::translateExpressionTemplate unexpected binary operation");
				}
			}
			break;
		case IR::IR_FUN_CALL: {
			IR::CallExpression *call = IR::ToCallExpression(templ);
			if (call->callee_parentfp != NULL) {
				translateExpression(call->callee_parentfp, frame, 
					machine_registers[R10], result);
			}
			placeCallArguments(call->arguments, result);
			assert(call->function->kind == IR::IR_LABELADDR);
			IR::VirtualRegister **callersave = callersave_registers.data();
			result.push_back(Instruction("call " + 
				IR::ToLabelAddressExpression(call->function)->label->getName(),
				0, NULL, callersave_count, callersave));
			removeCallArguments(call->arguments, result);
			if (value_storage != NULL)
				result.push_back(Instruction("movq " + Instruction::Input(0) +
					", " + Instruction::Output(0),
					1, &machine_registers[RAX], 1, &value_storage,
					1, NULL, true));
			break;
		}
		default:
			Error::fatalError("X86_64Assembler::translateExpressionTemplate unexpected expression kind");
	}
}

const char *cond_jumps[] = {
	"je",
	"jne",
	"jl",
	"jle",
	"jg",
	"jge",
	"jb",
	"jbe",
	"ja",
	"jae",
};

void X86_64Assembler::translateStatementTemplate(IR::Statement *templ,
	const std::list<TemplateChildInfo> &children, Instructions &result)
{
	switch (templ->kind) {
		case IR::IR_JUMP: {
			IR::JumpStatement *jump = IR::ToJumpStatement(templ);
			assert(jump->dest->kind == IR::IR_LABELADDR);
			result.push_back(Instruction("jmp " +
				IR::ToLabelAddressExpression(jump->dest)->label->getName(),
				0, NULL, 0, NULL, 1, &IR::ToLabelAddressExpression(jump->dest)->label));
			break;
		}
		case IR::IR_COND_JUMP: {
			IR::CondJumpStatement *jump = IR::ToCondJumpStatement(templ);
			addInstruction(result, "cmp ", jump->right, ", ", jump->left);
			IR::Label *destinations[] = {NULL, jump->true_dest};
			result.push_back(Instruction(std::string(cond_jumps[jump->comparison]) +
				" " + jump->true_dest->getName(), 0, NULL, 0, NULL, 2, destinations));
			break;
		}
		case IR::IR_LABEL:
			result.push_back(Instruction(
				IR::ToLabelPlacementStatement(templ)->label->getName() + ":"));
			break;
		case IR::IR_MOVE: {
			IR::MoveStatement *move = IR::ToMoveStatement(templ);
			const char *cmd = "movq ";
			IR::Expression *from = move->from;
			if (move->from->kind == IR::IR_BINARYOP) {
				assert(move->to->kind == IR::IR_REGISTER);
				IR::MemoryExpression *fake_addr = new IR::MemoryExpression(move->from);
				addInstruction(result, "leaq ", fake_addr, ", "  + Instruction::Output(0),
					ToRegisterExpression(move->to)->reg);
				delete fake_addr;
			} else if (move->from->kind == IR::IR_MEMORY) {
				assert(move->to->kind == IR::IR_REGISTER);
				addInstruction(result, "movq ", move->from, ", " + Instruction::Output(0),
					ToRegisterExpression(move->to)->reg);
			} else {
				if (move->to->kind == IR::IR_REGISTER)
					addInstruction(result, "movq ", move->from, ", " + Instruction::Output(0),
						ToRegisterExpression(move->to)->reg);
				else {
					assert(move->to->kind == IR::IR_MEMORY);
					std::vector<IR::VirtualRegister *> registers;
					std::string operand0, operand1;
					makeOperand(move->from, registers, operand0);
					makeOperand(move->to, registers, operand1);
					result.push_back(Instruction("movq " + operand0 + ", " +
						operand1, registers.size(), registers.data(), 0, NULL));
				}
			}
			break;
		}
		default:
			Error::fatalError("X86_64Assembler::translateStatementTemplate unexpected statement kind");
	}
}

void X86_64Assembler::translateBlob(const IR::Blob& blob, Instructions& result)
{
	result.push_back(Instruction(blob.label->getName() + ":"));
	std::string content = "";
	for (int i = 0; i < blob.data.size(); i++) {
		char buf[20];
		sprintf(buf, ".byte 0x%.2x\n", blob.data[i]);
		content += buf;
	}
	result.push_back(Instruction(content));
}

void X86_64Assembler::getBlobSectionHeader(std::string& header)
{
	header = ".section\t.rodata\n";
}

void X86_64Assembler::getCodeSectionHeader(std::string& header)
{
	header = ".text\n.global main\n";
}

void X86_64Assembler::framePrologue(IR::Label *fcn_label,
	IR::AbstractFrame *frame, Instructions &result)
{
	int framesize = ((IR::X86_64Frame *)frame)->getFrameSize();
	if (framesize > 0)
		result.push_front(Instruction("subq $" + IntToStr(framesize) +
			", %rsp"));
	result.push_front(Instruction(fcn_label->getName() + ":"));
}

void X86_64Assembler::frameEpilogue(IR::AbstractFrame *frame, Instructions &result)
{
	int framesize = ((IR::X86_64Frame *)frame)->getFrameSize();
	if (framesize > 0)
		result.push_back(Instruction("addq $" + IntToStr(framesize) +
			", %rsp"));
	result.push_back(Instruction("ret"));
}

void X86_64Assembler::functionPrologue(IR::Label *fcn_label,
	IR::AbstractFrame *frame, Instructions &result,
	std::vector<IR::VirtualRegister *> &prologue_regs)
{
	IR::VirtualRegister **calleesave = calleesave_registers.data();
	result.push_back(Instruction("# callee-save registers",
		0, NULL, calleesave_count, calleesave));
	prologue_regs.resize(calleesave_count);
	for (int i = 0; i < calleesave_count; i++) {
		prologue_regs[i] = IRenvironment->addRegister();
		result.push_back(Instruction("movq " + Instruction::Input(0) +
			", " + Instruction::Output(0),
			1, &calleesave_registers[i], 1, &prologue_regs[i], 1, NULL, true));
	}
	
	const std::list<IR::AbstractVarLocation *>parameters =
		frame->getParameters();
	int param_index = 0;
	for (std::list<IR::AbstractVarLocation *>::const_iterator param =
			parameters.begin(); param != parameters.end(); ++param) {
		if (param_index < paramreg_count) {
			assert((*param)->isRegister());
			IR::VirtualRegister *storage_reg =
				((IR::X86_64VarLocation *) *param)->getRegister();
			result.push_back(Instruction(std::string("movq ") +
				Instruction::Input(0) + ", " + Instruction::Output(0),
				1, &machine_registers[paramreg_list[param_index]], 
				1, &storage_reg, 1, NULL, true));
		} else
			assert(! (*param)->isRegister());
		param_index++;
	}
	IR::AbstractVarLocation *parent_fp = frame->getParentFpForUs();
	if (parent_fp != NULL) {
		assert(parent_fp->isRegister());
		IR::VirtualRegister *storage_reg =
			((IR::X86_64VarLocation *) parent_fp)->getRegister();
		result.push_back(Instruction(std::string("movq ") +
			Instruction::Input(0) + ", " + Instruction::Output(0),
			1, &machine_registers[R10], 
			1, &storage_reg, 1, NULL, true));
	}
}

void X86_64Assembler::functionEpilogue(IR::AbstractFrame *frame,
	IR::VirtualRegister* result_storage,
	std::vector<IR::VirtualRegister *> &prologue_regs,
	Instructions &result)
{
	if (result_storage != NULL)
		result.push_back(Instruction("movq " + Instruction::Input(0) + ", " +
			Instruction::Output(0), 1, &result_storage, 1, &machine_registers[RAX],
			1, NULL, true));

	IR::VirtualRegister **calleesave = calleesave_registers.data();
	assert(prologue_regs.size() == calleesave_registers.size());
	for (int i = 0; i < calleesave_count; i++) {
		result.push_back(Instruction("movq " + Instruction::Input(0) +
			", " + Instruction::Output(0),
			1, &prologue_regs[i], 1, &calleesave_registers[i], 1, NULL, true));
	}
	result.push_back(Instruction("# callee-save registers",
		calleesave_count, calleesave, 0, NULL));
}

void X86_64Assembler::programPrologue(IR::AbstractFrame *frame, Instructions &result)
{
	IR::Label label(0, "main");
	framePrologue(&label, frame, result);
}

void X86_64Assembler::programEpilogue(IR::AbstractFrame *frame, Instructions &result)
{
	result.push_back(Instruction("movq $0, %rax"));
	frameEpilogue(frame, result);
}

/**
 * Replace &iX (X = inputreg_index) with (semantically) &iX + offset
 */
void X86_64Assembler::addOffset(std::string &command, int inputreg_index, int offset)
{
	std::string p = std::string("&i") + IntToStr(inputreg_index);
	assert(p.size() == 3);
	const char *s = command.c_str();
	const char *t = strstr(s, p.c_str());
	assert(t != NULL);
	int pos = t - s;
	
	assert((pos > 0) && (pos + p.size() < command.size()));
	assert((s[pos-1] == ' ') || (s[pos-1] == '('));
	std::string result;
	
	if (s[pos-1] == ' ') {
		assert(! strncmp(s, "movq", 4));
		result = "leaq " + IntToStr(offset) + "(" + p + ")" +
			command.substr(pos + p.size(), command.size() - (pos + p.size()));
	} else {
		assert(s[pos + p.size()] == ')');
		int before_offset = pos;
		while ((before_offset > 0) && (s[before_offset] != ' '))
			before_offset--;
		assert(before_offset > 0);
		int existing_offset = 0;
		if (before_offset < pos-1)
			existing_offset = atoi(command.substr(before_offset+1,
				pos - (before_offset+1)).c_str());
		std::string offset_s;
		if (existing_offset + offset == 0)
			offset_s = "";
		else
			offset_s = IntToStr(existing_offset+offset);
		result = command.substr(0, before_offset+1) +
			offset_s + command.substr(pos-1, command.size() - (pos-1));
	}
	debug("%s +%d -> %s", command.c_str(), offset, result.c_str());
	command = result;
}

void X86_64Assembler::implementFramePointer(IR::AbstractFrame* frame, Instructions& result)
{
	for (Instructions::iterator inst = result.begin(); inst != result.end(); inst++) {
		debug("Inserting frame pointer: %s", (*inst).notation.c_str());
		for (int i = 0; i < (*inst).outputs.size(); i++)
			if ((*inst).outputs[i]->getIndex() == frame->getFramePointer()->getIndex())
				Error::fatalError("Cannot write to frame pointer");
		for (int i = 0; i < (*inst).inputs.size(); i++)
			if ((*inst).inputs[i]->getIndex() == frame->getFramePointer()->getIndex()) {
				(*inst).inputs[i] = machine_registers[RSP];
				addOffset((*inst).notation, i,
					((IR::X86_64Frame *)frame)->getFrameSize());
				break;
			}
	}
}

int findRegister(const std::string &notation, bool input, int index)
{
	const char *s = notation.c_str();
	std::string part;
	if (input)
		part = "&i" + IntToStr(index);
	else
		part = "&o" + IntToStr(index);
	const char *t = strstr(s, part.c_str());
	if (t == NULL)
		return -1;
	else
		return t-s;
}

void X86_64Assembler::replaceRegisterUsage(Instructions &code,
	std::list<Instruction>::iterator inst, IR::VirtualRegister *reg,
	IR::MemoryExpression *replacement)
{
	for (int i = 0; i < (*inst).inputs.size(); i++)
		if ((*inst).inputs[i]->getIndex() == reg->getIndex()) {
			bool need_splitting = true;
			const std::string &original = (*inst).notation;
			if (((*inst).outputs.size() > 0) &&
					((*inst).outputs[0]->getIndex() != reg->getIndex()) &&
					((*inst).inputs.size() == 1)) {
				int pos = findRegister(original, true, 0);
				assert(pos > 0);
				need_splitting = original[pos-1] == '(';
			}
			if (! need_splitting) {
				std::string storage_notation;
				(*inst).inputs.clear();
				makeOperand(replacement, (*inst).inputs, storage_notation);
				int pos = findRegister(original, true, 0);
				std::string new_command = original.substr(0, pos) +
					storage_notation + original.substr(pos+3,
					original.size() - (pos+3));
				debug("spilling %s -> %s", original.c_str(), new_command.c_str());
				(*inst).notation = new_command;
				(*inst).is_reg_to_reg_assign = false;
			} else {
				IR::VirtualRegister *temp = IRenvironment->addRegister();
				addInstruction(code, "movq ", replacement, ", " + Instruction::Output(0), temp,
					NULL, NULL, &inst);
				(*inst).inputs[i] = temp;
			}
			return;
		}
}

void X86_64Assembler::replaceRegisterAssignment(Instructions &code,
	std::list<Instruction>::iterator inst, IR::VirtualRegister *reg,
	IR::MemoryExpression *replacement)
{
	for (int i = 0; i < (*inst).outputs.size(); i++)
		if ((*inst).outputs[i]->getIndex() == reg->getIndex()) {
			bool need_splitting;
			const std::string &original = (*inst).notation;
			if ((*inst).inputs.size() > 1)
				need_splitting = true;
			else if ((*inst).inputs.size() == 0)
				need_splitting = false;
			else {
				int pos = findRegister(original, true, 0);
				assert(pos > 0);
				need_splitting = original[pos-1] == '(';
			}
			if (! need_splitting) {
				std::string storage_notation;
				makeOperand(replacement, (*inst).inputs, storage_notation);
				int pos = findRegister(original, false, 0);
				assert(pos == original.size()-3);
				std::string new_command = original.substr(0, pos) +
					storage_notation;
				debug("spilling %s -> %s", original.c_str(), new_command.c_str());
				(*inst).notation = new_command;
				(*inst).is_reg_to_reg_assign = false;
			} else {
				IR::VirtualRegister *temp = IRenvironment->addRegister();
				(*inst).outputs[i] = temp;
				const std::string &original = (*inst).notation;
				inst++;

				std::vector<IR::VirtualRegister *> registers;
				registers.push_back(temp);
				std::string operand0 = Instruction::Input(0);
				std::string operand1;
				makeOperand(replacement, registers, operand1);
				std::string new_command = "movq " + operand0 + ", " + operand1;
				code.insert(inst, Instruction(new_command, registers.size(),
					registers.data(), 0, NULL));
				debug("spilling %s -> append %s", original.c_str(),
					new_command.c_str());
			}
		}
}

void X86_64Assembler::spillRegister(IR::AbstractFrame *frame, Instructions &code,
	IR::VirtualRegister *reg)
{
	if (reg->isPrespilled()) {
		IR::X86_64VarLocation *stored_location =
			(IR::X86_64VarLocation *)reg->getPrespilledLocation();
		IR::Expression *storage_exp =
			stored_location->createCode(stored_location->owner_frame);
		
		Instructions::iterator inst = code.begin();
		bool seen_usage = false, seen_assignment = false;
		for (Instructions::iterator inst = code.begin(); inst != code.end(); inst++) {
			bool is_assigned = false;
			for (int i = 0; i < (*inst).outputs.size(); i++)
				if ((*inst).outputs[i]->getIndex() == reg->getIndex()) {
					is_assigned = true;
					seen_assignment = true;
					assert((*inst).is_reg_to_reg_assign);
					assert(! seen_usage);
					assert(! seen_assignment);
					assert((*inst).inputs.size() == 1);
					assert((*inst).outputs.size() == 1);
					std::string storage_notation;
					makeOperand(storage_exp, (*inst).inputs, storage_notation);
					
					const std::string &s = (*inst).notation;
					assert(s.substr(s.size()-3, 3) == "&o0");
					std::string replacement = s.substr(0, s.size()-3) +
						storage_notation;
					debug("Spilling %s -> %s", s.c_str(),
						  replacement.c_str());
					(*inst).notation = replacement;
					(*inst).is_reg_to_reg_assign = false;
				}
			
			bool is_used = false;
			for (int i = 0; i < (*inst).inputs.size(); i++)
				if ((*inst).inputs[i]->getIndex() == reg->getIndex()) {
					assert(! is_assigned);
					assert(seen_assignment);
					is_used = true;
					seen_usage = true;
					break;
				}
			if (is_used)
				replaceRegisterUsage(code, inst, reg,
					IR::ToMemoryExpression(storage_exp));
		}
		IR::DestroyExpression(storage_exp);
	} else {
		IR::X86_64VarLocation *stored_location =
			(IR::X86_64VarLocation *)frame->addVariable(".store." + reg->getName(),
				8, true);
		IR::Expression *storage_exp = IR::ToMemoryExpression(
			stored_location->createCode(stored_location->owner_frame));
		
		for (Instructions::iterator inst = code.begin(); inst != code.end(); inst++) {
			debug("Spilling: %s", (*inst).notation.c_str());
			bool is_assigned = false, is_used = false;
			for (int i = 0; i < (*inst).outputs.size(); i++)
				if ((*inst).outputs[i]->getIndex() == reg->getIndex())
					is_assigned = true;
			for (int i = 0; i < (*inst).inputs.size(); i++)
				if ((*inst).inputs[i]->getIndex() == reg->getIndex())
					is_used = true;
			if (is_used) {
				if (! is_assigned)
					replaceRegisterUsage(code, inst, reg,
						IR::ToMemoryExpression(storage_exp));
				else {
					IR::VirtualRegister *temp = IRenvironment->addRegister();
					addInstruction(code, "movq ", storage_exp,
						", " + Instruction::Output(0), temp,
						NULL, NULL, &inst);
					for (int i = 0; i < (*inst).inputs.size(); i++)
						if ((*inst).inputs[i]->getIndex() == reg->getIndex())
							(*inst).inputs[i] = temp;
					replaceRegisterAssignment(code, inst, reg,
						IR::ToMemoryExpression(storage_exp));
				}
			} else if (is_assigned) {
				replaceRegisterAssignment(code, inst, reg,
					IR::ToMemoryExpression(storage_exp));
			}
		}
		
		IR::DestroyExpression(storage_exp);
	}
}

}
