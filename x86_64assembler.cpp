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

static const std::string register_names[] = {
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

	exp_int = std::make_shared<IR::IntegerExpression>(0);
	exp_label = std::make_shared<IR::LabelAddressExpression>(nullptr);
	exp_register = std::make_shared<IR::RegisterExpression>(nullptr);
	
	memory_exp.push_back(Operand("($%)", {0}, std::make_shared<IR::MemoryExpression>(exp_int)));
	memory_exp.push_back(Operand("($%)", {0}, std::make_shared<IR::MemoryExpression>(exp_label)));
	memory_exp.push_back(Operand("(%)", {0}, std::make_shared<IR::MemoryExpression>(exp_register)));
	first_assign_by_lea = memory_exp.size();
	// reg + const, const + reg
	memory_exp.push_back(Operand("%(%)", {1,0}, std::make_shared<IR::MemoryExpression>(
		std::make_shared<IR::BinaryOpExpression>(IR::OP_PLUS, exp_register, exp_int)))
	);
	memory_exp.push_back(Operand("%(%)", {0,1}, std::make_shared<IR::MemoryExpression>(
		std::make_shared<IR::BinaryOpExpression>(IR::OP_PLUS, exp_int, exp_register)))
	);
	// reg - const
	memory_exp.push_back(Operand("-%(%)", {1,0}, std::make_shared<IR::MemoryExpression>(
		std::make_shared<IR::BinaryOpExpression>(IR::OP_MINUS, exp_register, exp_int)))
	);
	last_assign_by_lea = memory_exp.size()-1;
	
	
	// reg + reg
	memory_exp.push_back(Operand("(%,%)", {0,1}, std::make_shared<IR::MemoryExpression>(
		std::make_shared<IR::BinaryOpExpression>(IR::OP_PLUS, exp_register, exp_register))));
	
	// reg+reg±const and permutations
	/*IR::Expression terms[3];
	for (int p_c = 0; p_c < 3; p_c++) {
		int p_r1 = (p_c == 0) ? 1 : 0;
		int p_r2 = 3 - p_c - p_r1;
		terms[p_r1] = exp_register;
		terms[p_r2] = exp_register;
		terms[p_c] = exp_int;
		const char *names[3];
		names[p_r1] = "reg";
		names[p_r2] = "reg";
		names[p_c] = "C";

		memory_exp.push_back(Operand(
			"%(%,%)", {p_c, p_r1, p_r2},
			std::make_shared<IR::MemoryExpression>(
				std::make_shared<IR::BinaryOpExpression>
				(IR::OP_PLUS, std::make_shared<IR::BinaryOpExpression>
					(IR::OP_PLUS, terms[0], terms[1]),
				terms[2]))
			));
		debug("(%s + %s) + %s\n = %d(%d,%d)\n",
				names[0], names[1], names[2], p_c, p_r1, p_r2);
		if (p_c == 2) {
			memory_exp.push_back(Operand(
				"-%(%,%)", {p_c, p_r1, p_r2},
				std::make_shared<IR::MemoryExpression>(
					std::make_shared<IR::BinaryOpExpression>
					(IR::OP_MINUS, std::make_shared<IR::BinaryOpExpression>
						(IR::OP_PLUS, terms[0], terms[1]),
					terms[2]))
				));
			debug("(%s + %s) - %s\n = -%d(%d,%d)\n",
					names[0], names[1], names[2], p_c, p_r1, p_r2);
		}
		if (p_c == 1) {
			memory_exp.push_back(Operand(
				"-%(%,%)", {p_c, p_r1, p_r2},
				std::make_shared<IR::MemoryExpression>(
					std::make_shared<IR::BinaryOpExpression>
					(IR::OP_PLUS, std::make_shared<IR::BinaryOpExpression>
						(IR::OP_MINUS, terms[0], terms[1]),
					terms[2]))
				));
			debug("(%s - %s) + %s\n = -%d(%d,%d)\n",
					names[0], names[1], names[2], p_c, p_r1, p_r2);
		}

		memory_exp.push_back(Operand(
			"%(%,%)", {p_c, p_r1, p_r2},
			std::make_shared<IR::MemoryExpression>(
				std::make_shared<IR::BinaryOpExpression>
				(IR::OP_PLUS, terms[0], std::make_shared<IR::BinaryOpExpression>
					(IR::OP_PLUS, terms[1], terms[2])))
				));
		debug("%s + (%s + %s)\n = %d(%d,%d)\n",
				names[0], names[1], names[2], p_c, p_r1, p_r2);
		if (p_c == 2) {
			memory_exp.push_back(Operand(
				"-%(%,%)", {p_c, p_r1, p_r2},
				std::make_shared<IR::MemoryExpression>(
					std::make_shared<IR::BinaryOpExpression>
					(IR::OP_PLUS, terms[0], std::make_shared<IR::BinaryOpExpression>
						(IR::OP_MINUS, terms[1], terms[2])))
				));
			debug("%s + (%s - %s)\n = -%d(%d,%d)\n",
				names[0], names[1], names[2], p_c, p_r1, p_r2);
		}
		if (p_c == 1) {
			memory_exp.push_back(Operand(
				"-%(%,%)", {p_c, p_r1, p_r2},
				std::make_shared<IR::MemoryExpression>(
					std::make_shared<IR::BinaryOpExpression>
					(IR::OP_MINUS, terms[0], std::make_shared<IR::BinaryOpExpression>
						(IR::OP_MINUS, terms[1], terms[2])))
				));
			debug("%s - (%s - %s)\n = -%d(%d,%d)\n",
				names[0], names[1], names[2], p_c, p_r1, p_r2);
		}
	}*/

	int mul_index[] = {2,4,8};
	enum {NMUL = 3};
	IR::Expression exp_mul_index[2*NMUL], exp_mul_index_plus[2*NMUL];
	for (int i = 0; i < NMUL; i++) {
		// reg*X, X*reg
		exp_mul_index[i] = std::make_shared<IR::BinaryOpExpression>
			(IR::OP_MUL, exp_register, std::make_shared<IR::IntegerExpression>(mul_index[i]));
		exp_mul_index[i+NMUL] = std::make_shared<IR::BinaryOpExpression>
			(IR::OP_MUL, std::make_shared<IR::IntegerExpression>(mul_index[i]), exp_register);
	}
	
// 		// reg*X + itself = reg*(X+1), (X+1)*reg
// 		exp_mul_index_plus[i] = std::make_shared<IR::BinaryOpExpression>
// 			(IR::OP_MUL, exp_register, std::make_shared<IR::IntegerExpression>(mul_index[i]+1));
// 		exp_mul_index_plus[i+NMUL] = std::make_shared<IR::BinaryOpExpression>
// 			(IR::OP_MUL, std::make_shared<IR::IntegerExpression>(mul_index[i]+1), exp_register);
// 	}

	for (int i = 0; i < 2*NMUL; i++) {
		std::vector<int> order;
		if (i < NMUL)
			order = {2,0,1};
		else
			order = {2,1,0};
		// reg*(2,4,8) + reg
		memory_exp.push_back(Operand("(%,%,%)", order, std::make_shared<IR::MemoryExpression>(
			std::make_shared<IR::BinaryOpExpression>(IR::OP_PLUS,
				exp_mul_index[i], exp_register)))
		);
		// reg*(2,4,8) ± const
		memory_exp.push_back(Operand("%(%,%)", order, std::make_shared<IR::MemoryExpression>(
			std::make_shared<IR::BinaryOpExpression>(IR::OP_PLUS,
				exp_mul_index[i], exp_int)))
		);
		memory_exp.push_back(Operand("-%(%,%)", order, std::make_shared<IR::MemoryExpression>(
			std::make_shared<IR::BinaryOpExpression>(IR::OP_MINUS,
				exp_mul_index[i], exp_int)))
		);
		
		if (i < NMUL)
			order = {0,1,2};
		else
			order = {0,2,1};
		// reg + reg*(2,4,8)
		memory_exp.push_back(Operand("(%,%,%)", order, std::make_shared<IR::MemoryExpression>(
			std::make_shared<IR::BinaryOpExpression>(IR::OP_PLUS,
				exp_register, exp_mul_index[i])))
		);
		// const + reg*(2,4,8)
		memory_exp.push_back(Operand("%(%,%)", order, std::make_shared<IR::MemoryExpression>(
			std::make_shared<IR::BinaryOpExpression>(IR::OP_PLUS,
				exp_int, exp_mul_index[i])))
		);
	}
	
	/*
	// reg*(2,4,8) + reg ± const and permutations
	for (int factor = 0; factor < 2*NMUL; factor++) {
		for (int p_mul = 0; p_mul < 3; p_mul++)
			for (int p_reg = 0; p_reg < 3; p_reg++) if (p_reg != p_mul) {
				int p_c = 3-p_mul-p_reg;
				terms[p_mul] = exp_mul_index[factor];
				terms[p_reg] = exp_register;
				terms[p_c] = exp_int;
				const char *names[3];
				char buf[10];
				if (factor < NMUL)
					sprintf(buf, "reg*%d", mul_index[factor]);
				else
					sprintf(buf, "%d*reg", mul_index[factor-NMUL]);
				names[p_mul] = buf;
				names[p_reg] = "reg";
				names[p_c] = "C";
				int global_c = p_c + ((p_mul < p_c) ? 1 : 0);
				int global_reg = p_reg + ((p_mul < p_reg) ? 1 : 0);
				int global_regm = p_mul + ((factor < NMUL) ? 0 : 1);
				int global_factor = p_mul + ((factor < NMUL) ? 1 : 0);
				memory_exp.push_back(Operand(
					"%(%,%,%)", {global_c, global_reg, global_regm, global_factor},
					std::make_shared<IR::MemoryExpression>(
						std::make_shared<IR::BinaryOpExpression>
						(IR::OP_PLUS, std::make_shared<IR::BinaryOpExpression>
							(IR::OP_PLUS, terms[0], terms[1]),
						terms[2]))
					));
				debug("(%s + %s) + %s\n = %d(%d,%d,%d)\n",
					   names[0], names[1], names[2], global_c, global_reg, global_regm, global_factor);
				if (p_c == 2) {
					memory_exp.push_back(Operand(
						"-%(%,%,%)", {global_c, global_reg, global_regm, global_factor},
						std::make_shared<IR::MemoryExpression>(
							std::make_shared<IR::BinaryOpExpression>
							(IR::OP_MINUS, std::make_shared<IR::BinaryOpExpression>
								(IR::OP_PLUS, terms[0], terms[1]),
							terms[2]))
						));
					debug("(%s + %s) - %s\n = -%d(%d,%d,%d)\n",
						names[0], names[1], names[2], global_c, global_reg, global_regm, global_factor);
				}
				if (p_c == 1) {
					memory_exp.push_back(Operand(
						"-%(%,%,%)", {global_c, global_reg, global_regm, global_factor},
						std::make_shared<IR::MemoryExpression>(
							std::make_shared<IR::BinaryOpExpression>
							(IR::OP_PLUS, std::make_shared<IR::BinaryOpExpression>
								(IR::OP_MINUS, terms[0], terms[1]),
							terms[2]))
						));
					debug("(%s - %s) + %s\n = -%d(%d,%d,%d)\n",
						names[0], names[1], names[2], global_c, global_reg, global_regm, global_factor);
				}
				
				memory_exp.push_back(Operand(
					"%(%,%,%)", {global_c, global_reg, global_regm, global_factor},
					std::make_shared<IR::MemoryExpression>(
						std::make_shared<IR::BinaryOpExpression>
						(IR::OP_PLUS, terms[0], std::make_shared<IR::BinaryOpExpression>
							(IR::OP_PLUS, terms[1], terms[2])))
					  ));
				debug("%s + (%s + %s)\n = %d(%d,%d,%d)\n",
					   names[0], names[1], names[2], global_c, global_reg, global_regm, global_factor);
				if (p_c == 2) {
					memory_exp.push_back(Operand(
						"-%(%,%,%)", {global_c, global_reg, global_regm, global_factor},
						std::make_shared<IR::MemoryExpression>(
							std::make_shared<IR::BinaryOpExpression>
							(IR::OP_PLUS, terms[0], std::make_shared<IR::BinaryOpExpression>
								(IR::OP_MINUS, terms[1], terms[2])))
						));
					debug("%s + (%s - %s)\n = -%d(%d,%d,%d)\n",
						names[0], names[1], names[2], global_c, global_reg, global_regm, global_factor);
				}
				if (p_c == 1) {
					memory_exp.push_back(Operand(
						"-%(%,%,%)", {global_c, global_reg, global_regm, global_factor},
						std::make_shared<IR::MemoryExpression>(
							std::make_shared<IR::BinaryOpExpression>
							(IR::OP_MINUS, terms[0], std::make_shared<IR::BinaryOpExpression>
								(IR::OP_MINUS, terms[1], terms[2])))
						));
					debug("%s - (%s - %s)\n = -%d(%d,%d,%d)\n",
						names[0], names[1], names[2], global_c, global_reg, global_regm, global_factor);
				}
 			}
 	}*/
	
	addTemplate("movq $" + Template::Input(0) + ", " +
		Template::Output(), exp_int);
	addTemplate("movq $" + Template::Input(0) + ", " +
		Template::Output(), exp_label);
	addTemplate(Template::Input(0) + ":",
		std::make_shared<IR::LabelPlacementStatement>(nullptr));
	addTemplate("movq " + Template::Input(0) + ", " +
		Template::Output(), true, exp_register);
	for (Operand &exp: memory_exp)
		addTemplate("movq " + exp.implement(0) + ", " +
			Template::Output(), exp.expression());
	
	make_arithmetic("addq", IR::OP_PLUS);
	make_arithmetic("subq", IR::OP_MINUS);
	// imul
	make_arithmetic("imulq", IR::OP_MUL);
	// don't forget cqo!! and idivq
	make_arithmetic("idivq", IR::OP_DIV);
	
	make_comparison("je", IR::OP_EQUAL);
	make_comparison("jne", IR::OP_NONEQUAL);
	make_comparison("jl", IR::OP_LESS);
	make_comparison("jle", IR::OP_LESSEQUAL);
	make_comparison("jg", IR::OP_GREATER);
	make_comparison("jge", IR::OP_GREATEQUAL);
	make_comparison("jb", IR::OP_ULESS);
	make_comparison("jbe", IR::OP_ULESSEQUAL);
	make_comparison("ja", IR::OP_UGREATER);
	make_comparison("jae", IR::OP_UGREATEQUAL);
	make_assignment();
	
	addTemplate({"call " + Template::Input(0), std::string("movq ") +
		register_names[RESULT_REG] + ", " + Template::Output()},
		{{}, {machine_registers[RESULT_REG]}},
		{callersave_registers, {}}, {false, true},
		std::make_shared<IR::CallExpression>(exp_register, nullptr));
	addTemplate({"call " + Template::Input(0), std::string("movq ") +
		register_names[RESULT_REG] + ", " + Template::Output()},
		{{}, {machine_registers[RESULT_REG]}},
		{callersave_registers, {}}, {false, true},
		std::make_shared<IR::CallExpression>(exp_label, nullptr));
	
	addTemplate({"jmp " + Template::Input(0)}, {true}, {false},
		std::make_shared<IR::JumpStatement>(exp_label));
	addTemplate({"jmp " + Template::Input(0)}, {true}, {false},
		std::make_shared<IR::JumpStatement>(exp_register));
}

int powerof2(int x)
{
	if (x <= 0)
		return -1;
	int res = 0;
	while (x > 1) {
		if (x % 2 != 0)
			return -1;
		res++;
		x /= 2;
	}
	return res;
}

bool mul_to_shl(bool int_at_front, bool other_is_reg,
	const Instructions &templ, IR::IREnvironment *ir_env,
	Instructions &result, const std::list<TemplateChildInfo> &elements)
{
	const TemplateChildInfo *int_element = NULL;
	assert(! elements.empty());
	if (int_at_front)
		int_element = &elements.front();
	else
		int_element = &elements.back();
	assert (! int_element->value_storage);
	int value = IR::ToIntegerExpression(int_element->expression)->value;
	bool negative = false;
	if (value < 0) {
		negative = true;
		value = -value;
	}
	int power = powerof2(value);
	if (power < 0)
		return false;
	Instructions::const_iterator second = templ.begin();
	++second;
	assert (second != templ.end());
	std::string prefix = "imulq ";
	assert((*second).notation.substr(0, prefix.size()) == prefix);
	result.clear();
	
	std::string cmd_base = "movq " + (*second).notation.substr(prefix.size(),
		(*second).notation.size() - prefix.size()) + ", ";
	IR::VirtualRegister *tmp_reg = NULL;
	if (negative)
		result.push_back(Instruction(cmd_base + Template::XOutput(0), {},
			{tmp_reg}, other_is_reg));
	else
		result.push_back(Instruction(cmd_base + Template::Output(), other_is_reg));
	DebugPrinter dbg("assembler.log");
	dbg.debug("Multiply by shl: %s", result.back().notation.c_str());

	cmd_base = "salq $" + IntToStr(power) + ", ";
	if (negative)
		result.push_back(Instruction(cmd_base + Template::XOutput(0), {},
			{tmp_reg}, false));
	else
		result.push_back(Instruction(cmd_base + Template::Output(), false));
	dbg.debug("Multiply by shl: %s", result.back().notation.c_str());
	if (negative) {
		result.push_back(Instruction("xorq " + Template::Output() + ", " + Template::Output()));
		dbg.debug("Divide by shr: %s", result.back().notation.c_str());
		result.push_back(Instruction("subq " + Template::XInput(0) + ", " + Template::Output()));
		result.back().inputs.push_back(tmp_reg);
		dbg.debug("Divide by shr: %s", result.back().notation.c_str());
	}
	
	return true;
}

bool mul_int_reg_to_shl(const Instructions &templ, IR::IREnvironment *ir_env,
	Instructions &result, const std::list<TemplateChildInfo> &elements)
{
	return mul_to_shl(true, true, templ, ir_env, result, elements);
}

bool mul_int_nonreg_to_shl(const Instructions &templ, IR::IREnvironment *ir_env,
	Instructions &result, const std::list<TemplateChildInfo> &elements)
{
	return mul_to_shl(true, false, templ, ir_env, result, elements);
}

bool mul_reg_int_to_shl(const Instructions &templ, IR::IREnvironment *ir_env,
	Instructions &result, const std::list<TemplateChildInfo> &elements)
{
	return mul_to_shl(false, true, templ, ir_env, result, elements);
}

bool mul_nonreg_int_to_shl(const Instructions &templ, IR::IREnvironment *ir_env,
	Instructions &result, const std::list<TemplateChildInfo> &elements)
{
	return mul_to_shl(false, false, templ, ir_env, result, elements);
}

bool div_X_int_to_shr(const Instructions &templ, IR::IREnvironment *ir_env,
	Instructions &result, const std::list<TemplateChildInfo> &elements)
{
	assert (! elements.back().value_storage);
	int value = IR::ToIntegerExpression(elements.back().expression)->value;
	bool negative = false;
	if (value < 0) {
		negative = true;
		value = -value;
	}
	int power = powerof2(value);
	assert(power >= 0);
	result.clear();
	const Instruction &load = templ.front();
	const Instruction &shift = templ.back();
	assert(load.notation.substr(load.notation.size()-2, 2) == Template::Output());
	const char *shift_prefix = "sarq $";
	assert(shift.notation == shift_prefix + Template::Input(elements.size()-1) +
		+ ", " + Template::Output());
	result.push_back(load);
	IR::VirtualRegister *tmp_reg = NULL;
	if (negative) {
		tmp_reg = ir_env->addRegister();
		result.back().outputs.push_back(tmp_reg);
		result.back().notation = load.notation.substr(0, load.notation.size()-2) +
			Template::XOutput(result.back().outputs.size()-1);
	}
	DebugPrinter dbg("assembler.log");
	dbg.debug("Divide by shr: %s", result.back().notation.c_str());
	result.push_back(shift);
	result.back().notation = shift_prefix + IntToStr(power) + ", ";
	if (negative) {
		result.back().outputs.push_back(tmp_reg);
		result.back().notation += Template::XOutput(result.back().outputs.size()-1);
	} else
		result.back().notation += Template::Output();
	dbg.debug("Divide by shr: %s", result.back().notation.c_str());
	if (negative) {
		result.push_back(Instruction("xorq " + Template::Output() + ", " + Template::Output()));
		dbg.debug("Divide by shr: %s", result.back().notation.c_str());
		result.push_back(Instruction("subq " + Template::XInput(0) + ", " + Template::Output()));
		result.back().inputs.push_back(tmp_reg);
		dbg.debug("Divide by shr: %s", result.back().notation.c_str());
	}
	return true;
}

bool can_divide_by_const(const IR::Expression expression)
{
	if (expression->kind != IR::IR_BINARYOP)
		return false;
	std::shared_ptr<IR::BinaryOpExpression> bin_op = IR::ToBinaryOpExpression(expression);
	if ((bin_op->operation != IR::OP_DIV) || (bin_op->right->kind != IR::IR_INTEGER))
		return false;
	int value = IR::ToIntegerExpression(bin_op->right)->value;
	int power = powerof2(abs(value));
	DebugPrinter dbg("assembler.log");
	dbg.debug("Checking if we can divide by %d: shift power = %d", value, power);
	return power >= 0;
// 	assert (! elements.back().value_storage);
// 	int value = IR::ToIntegerExpression(elements.back().expression)->value;
// 	return powerof2(abs(value)) >= 0;
}

// IR::VirtualRegister* X86_64Assembler::getFramePointerRegister()
// {
// 	return machine_registers[FP];
// }
// 
void X86_64Assembler::make_arithmetic(const char *command, IR::BinaryOp ir_code)
{
	std::vector<std::string> t_reg_int, t_int_reg, t_reg_reg,
		t_reg_mem, t_int_mem, t_mem_reg, t_mem_int;
	std::vector<std::vector<IR::VirtualRegister *> > extra_inputs, extra_outputs;
	std::vector<bool> is_move_reg_int, is_move_int_reg, is_move_reg_reg;
	std::vector<bool> is_move_reg_mem, is_move_int_mem, is_move_mem_reg, is_move_mem_int;
	std::vector<bool> is_move_mem_mem;
	TemplateAdjuster adjust_int_reg = NULL, adjust_int_mem = NULL,
	                 adjust_reg_int = NULL, adjust_mem_int = NULL;
	ExpressionMatchChecker match_X_int = NULL;
	if ((ir_code == IR::OP_PLUS) || (ir_code == IR::OP_MINUS)) {
		t_reg_int = {"movq " + Template::Input(0) + ", " + Template::Output(),
			std::string(command) + " $" + Template::Input(1) + ", " + Template::Output()};
		if (ir_code == IR::OP_PLUS)
			t_int_reg = {"movq " + Template::Input(1) + ", " + Template::Output(),
				std::string(command) + " $" + Template::Input(0) + ", " + Template::Output()};
		else
			t_int_reg = {"movq $" + Template::Input(0) + ", " + Template::Output(),
				std::string(command) + " " + Template::Input(1) + ", " + Template::Output()};
		t_reg_reg = {"movq " + Template::Input(0) + ", " + Template::Output(),
			std::string(command) + " " + Template::Input(1) + ", " + Template::Output()};
		extra_inputs = {{}, {}};
		extra_outputs = {{}, {}};
		is_move_reg_int = {true, false};
		is_move_reg_reg = {true, false};
		is_move_reg_mem = {true, false};
		is_move_int_mem = {false, false};
		is_move_mem_int = {false, false};
		is_move_mem_mem = {false, false};
		if (ir_code == IR::OP_PLUS) {
			is_move_int_reg = {true, false};
			is_move_mem_reg = {true, false};
		} else {
			is_move_int_reg = {false, false};
			is_move_mem_reg = {false, false};
		}
	} else if (ir_code == IR::OP_MUL) {
		t_reg_int = {"movq $" + Template::Input(1) + ", " + register_names[RAX],
			std::string(command) + " " + Template::Input(0),
			"movq " + register_names[RAX] + ", " + Template::Output()};
		t_int_reg = {"movq $" + Template::Input(0) + ", " + register_names[RAX],
			std::string(command) + " " + Template::Input(1),
			"movq " + register_names[RAX] + ", " + Template::Output()};
		t_reg_reg = {"movq " + Template::Input(0) + ", " + register_names[RAX],
			std::string(command) + " " + Template::Input(1),
			"movq " + register_names[RAX] + ", " + Template::Output()};
		
		extra_inputs = {{}, {machine_registers[RAX]}, {machine_registers[RAX]}};
		extra_outputs = {{machine_registers[RAX]},
			{machine_registers[RAX], machine_registers[RDX]}, {}};
		is_move_reg_int = {false, false, true};
		is_move_int_reg = {false, false, true};
		is_move_reg_reg = {true, false, true};
		is_move_reg_mem = {true, false, true};
		is_move_int_mem = {false, false, true};
		is_move_mem_reg = {true, false, true};
		is_move_mem_int = {false, false, true};
		is_move_mem_mem = {false, false, true};
		adjust_reg_int = mul_reg_int_to_shl;
		adjust_int_reg = mul_int_reg_to_shl;
		adjust_mem_int = mul_nonreg_int_to_shl;
		adjust_int_mem = mul_int_nonreg_to_shl;
	} else if (ir_code == IR::OP_DIV) {
		t_reg_int = {"movq " + Template::Input(0) + ", " + Template::Output(),
			"sarq $" + Template::Input(1) + ", " + Template::Output()};
		t_int_reg = {"movq $" + Template::Input(0) + ", " + register_names[RAX],
			"cqo",
			std::string(command) + " " + Template::Input(1),
			"movq " + register_names[RAX] + ", " + Template::Output()};
		t_reg_reg = {"movq " + Template::Input(0) + ", " + register_names[RAX],
			"cqo",
			std::string(command) + " " + Template::Input(1),
			"movq " + register_names[RAX] + ", " + Template::Output()};
		extra_inputs = {{}, {machine_registers[RAX]},
			{{machine_registers[RAX], machine_registers[RDX]}}, {machine_registers[RAX]}};
		extra_outputs = {{machine_registers[RAX]},
			{machine_registers[RAX], machine_registers[RDX]},
			{machine_registers[RAX], machine_registers[RDX]}, {}};
		is_move_reg_int = {true, false};
		is_move_int_reg = {false, false, false, true};
		is_move_reg_reg = {true, false, false, true};
		is_move_reg_mem = {true, false, false, true};
		is_move_int_mem = {false, false, false, true};
		is_move_mem_reg = {false, false, false, true};
		is_move_mem_int = {false, false};
		is_move_mem_mem = {false, false, false, true};
		match_X_int = can_divide_by_const;
		adjust_reg_int = div_X_int_to_shr;
		adjust_mem_int = div_X_int_to_shr;
	} else
		Error::fatalError("x86_64 assembler: Strange binary operation");

	if (ir_code == IR::OP_DIV)
		addTemplate(t_reg_int, {{},{}}, {{},{}}, is_move_reg_int,
			std::make_shared<IR::BinaryOpExpression>(ir_code, exp_register, exp_int),
			adjust_reg_int, match_X_int);
	else
		addTemplate(t_reg_int, extra_inputs, extra_outputs, is_move_reg_int,
			std::make_shared<IR::BinaryOpExpression>(ir_code, exp_register, exp_int),
			adjust_reg_int, match_X_int);
	addTemplate(t_int_reg, extra_inputs, extra_outputs, is_move_int_reg,
		std::make_shared<IR::BinaryOpExpression>(ir_code, exp_int, exp_register),
		adjust_int_reg);
	addTemplate(t_reg_reg, extra_inputs, extra_outputs, is_move_reg_reg,
		std::make_shared<IR::BinaryOpExpression>(ir_code, exp_register, exp_register));
	Operand second_simple("%", {0}, nullptr);
	for (const Operand &exp: memory_exp) {
		if ((ir_code == IR::OP_PLUS) || (ir_code == IR::OP_MINUS)) {
			t_reg_mem = {"movq " + Template::Input(0) + ", " + Template::Output(),
				std::string(command) + " " + exp.implement(1) + ", " + Template::Output()};
			t_int_mem = {"movq $" + Template::Input(0) + ", " + Template::Output(),
				std::string(command) + " " + exp.implement(1) + ", " + Template::Output()};
			if (ir_code == IR::OP_PLUS)
				t_mem_reg = {"movq " + second_simple.implement(exp.elementCount()) + ", " + Template::Output(),
					std::string(command) + " " + exp.implement(0) + ", " + Template::Output()};
			else
				t_mem_reg = {"movq " + exp.implement(0) + ", " + Template::Output(),
					std::string(command) + " " + second_simple.implement(exp.elementCount()) + ", " + Template::Output()};
			t_mem_int = {"movq " + exp.implement(0) + ", " + Template::Output(),
				std::string(command) + " $" + second_simple.implement(exp.elementCount()) + ", " + Template::Output()};
		} else if (ir_code == IR::OP_MUL) {
			t_reg_mem = {"movq " + Template::Input(0) + ", " + register_names[RAX],
				std::string(command) + " " + exp.implement(1),
				"movq " + register_names[RAX] + ", " + Template::Output()};
			t_int_mem = {"movq $" + Template::Input(0) + ", " + register_names[RAX],
				std::string(command) + " " + exp.implement(1),
				"movq " + register_names[RAX] + ", " + Template::Output()};
			t_mem_reg = {"movq " + second_simple.implement(exp.elementCount()) + ", " + register_names[RAX],
				std::string(command) + " " + exp.implement(0),
				"movq " + register_names[RAX] + ", " + Template::Output()};
			t_mem_int = {"movq $" + second_simple.implement(exp.elementCount()) + ", " + register_names[RAX],
				std::string(command) + " " + exp.implement(0),
				"movq " + register_names[RAX] + ", " + Template::Output()};
		} else if (ir_code == IR::OP_DIV) {
			t_reg_mem = {"movq " + Template::Input(0) + ", " + register_names[RAX], "cqo",
				std::string(command) + " " + exp.implement(1),
				"movq " + register_names[RAX] + ", " + Template::Output()};
			t_int_mem = {"movq $" + Template::Input(0) + ", " + register_names[RAX], "cqo",
				std::string(command) + " " + exp.implement(1),
				"movq " + register_names[RAX] + ", " + Template::Output()};
			t_mem_reg = {"movq " + exp.implement(0) + ", " + register_names[RAX], "cqo",
				std::string(command) + " " + second_simple.implement(exp.elementCount()),
				"movq " + register_names[RAX] + ", " + Template::Output()};
			t_mem_int = {"movq " + exp.implement(0) + ", " + Template::Output(),
				"sarq $" + second_simple.implement(exp.elementCount()) + ", " + Template::Output()};
		}
		addTemplate(t_reg_mem, extra_inputs, extra_outputs, is_move_reg_mem,
			std::make_shared<IR::BinaryOpExpression>(ir_code, exp_register,
				exp.expression()));
		
		addTemplate(t_int_mem, extra_inputs, extra_outputs, is_move_int_mem, 
			std::make_shared<IR::BinaryOpExpression>(ir_code, exp_int,
				exp.expression()), adjust_int_mem);
		
 		addTemplate(t_mem_reg, extra_inputs, extra_outputs, is_move_mem_reg,
			std::make_shared<IR::BinaryOpExpression>(ir_code,
				exp.expression(), exp_register));
		
		if (ir_code == IR::OP_DIV)
			addTemplate(t_mem_int, {{},{}}, {{},{}}, is_move_mem_int,
				std::make_shared<IR::BinaryOpExpression>(ir_code,
					exp.expression(), exp_int), adjust_mem_int, match_X_int);
		else
			addTemplate(t_mem_int, extra_inputs, extra_outputs, is_move_mem_int,
				std::make_shared<IR::BinaryOpExpression>(ir_code,
					exp.expression(), exp_int), adjust_mem_int, match_X_int);
	}
	std::vector<std::string> t_mem_mem;
	for (const Operand &exp1: memory_exp)
		for (const Operand &exp2: memory_exp) {
			if ((ir_code == IR::OP_PLUS) || (ir_code == IR::OP_MINUS)) {
				t_mem_mem = {"movq " + exp1.implement(0) + ", " + Template::Output(),
					std::string(command) + " " + exp2.implement(exp1.elementCount()) + ", " + Template::Output()};
			} else if (ir_code == IR::OP_MUL) {
				t_mem_mem = {"movq " + exp1.implement(0) + ", " + register_names[RAX],
					std::string(command) + " " + exp2.implement(exp1.elementCount()),
					"movq " + register_names[RAX] + ", " + Template::Output()};
			} else if (ir_code == IR::OP_DIV) {
				t_mem_mem = {"movq " + exp1.implement(0) + ", " + register_names[RAX], "cqo",
					std::string(command) + " " + exp2.implement(exp1.elementCount()),
					"movq " + register_names[RAX] + ", " + Template::Output()};
			}
			addTemplate(t_mem_mem, extra_inputs, extra_outputs, is_move_mem_mem,
				std::make_shared<IR::BinaryOpExpression>(ir_code, exp1.expression(),
					exp2.expression()));
		}
}

void X86_64Assembler::make_comparison(const char *jxx_cmd, IR::ComparisonOp ir_code)
{
	std::string jump = std::string(jxx_cmd) + " " + Template::Input(2);
	std::vector<bool> extra_dests = {false, true};
	std::vector<bool> jumps_to_next = {true, true};
	addTemplate({"cmp $" + Template::Input(1) + ", " + Template::Input(0), jump},
		extra_dests, jumps_to_next,
		std::make_shared<IR::CondJumpStatement>(ir_code, exp_register, exp_int, nullptr, nullptr));
	addTemplate({"cmp " + Template::Input(1) + ", " + Template::Input(0), jump},
		extra_dests, jumps_to_next,
		std::make_shared<IR::CondJumpStatement>(ir_code, exp_register, exp_register, nullptr, nullptr));
	Operand second_simple("%", {0}, nullptr);
	for (const Operand &exp: memory_exp) {
		jump = std::string(jxx_cmd) + " " + second_simple.implement(exp.elementCount()+1);
		addTemplate({"cmp " + exp.implement(1) + ", " + Template::Input(0), jump},
			extra_dests, jumps_to_next,
			std::make_shared<IR::CondJumpStatement>(ir_code, exp_register,
				exp.expression(), nullptr, nullptr));
		addTemplate({"cmp " + exp.implement(1) + ", $" + Template::Input(0), jump},
			extra_dests, jumps_to_next,
			std::make_shared<IR::CondJumpStatement>(ir_code, exp_int,
				exp.expression(), nullptr, nullptr));
		addTemplate({"cmp " + second_simple.implement(exp.elementCount()) + ", " +
				exp.implement(0), jump},
			extra_dests, jumps_to_next,
			std::make_shared<IR::CondJumpStatement>(ir_code,
				exp.expression(), exp_register, nullptr, nullptr));
		addTemplate({"cmp $" + second_simple.implement(exp.elementCount()) + ", " +
				exp.implement(0), jump},
			extra_dests, jumps_to_next,
			std::make_shared<IR::CondJumpStatement>(ir_code,
				exp.expression(), exp_int, nullptr, nullptr));
	}
}

void X86_64Assembler::make_assignment()
{
	addTemplate("movq $" + Template::Input(1) + ", " + Template::InputAssigned(0),
		std::make_shared<IR::MoveStatement>(exp_register, exp_int));
	addTemplate("movq $" + Template::Input(1) + ", " + Template::InputAssigned(0),
		std::make_shared<IR::MoveStatement>(exp_register, exp_label));
	addTemplate("movq " + Template::Input(1) + ", " + Template::InputAssigned(0), true,
		std::make_shared<IR::MoveStatement>(exp_register, exp_register));
	Operand second_simple("%", {0}, nullptr);
	int ind = 0;
	for (const Operand &exp: memory_exp) {
		addTemplate("movq " + exp.implement(1) + ", " + Template::InputAssigned(0),
			std::make_shared<IR::MoveStatement>(exp_register,
				exp.expression()));
		if ((ind >= first_assign_by_lea) && (ind <= last_assign_by_lea))
			addTemplate("leaq " + exp.implement(1) + ", " + Template::InputAssigned(0),
				std::make_shared<IR::MoveStatement>(exp_register,
					IR::ToMemoryExpression(exp.expression())->address));
		addTemplate("movq " + second_simple.implement(exp.elementCount()) + ", " + exp.implement(0),
			std::make_shared<IR::MoveStatement>(
				exp.expression(), exp_register));
		addTemplate("movq $" + second_simple.implement(exp.elementCount()) + ", " + exp.implement(0),
			std::make_shared<IR::MoveStatement>(
				exp.expression(), exp_int));
		addTemplate("movq $" + second_simple.implement(exp.elementCount()) + ", " + exp.implement(0),
			std::make_shared<IR::MoveStatement>(
				exp.expression(), exp_label));
		ind++;
	}
}

void X86_64Assembler::makeOperand(IR::Expression expression,
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
		case IR::IR_MEMORY: {
			IR::Expression addr = IR::ToMemoryExpression(expression)->address;
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
					std::shared_ptr<IR::BinaryOpExpression> binop =
						IR::ToBinaryOpExpression(addr);
					if (binop->operation == IR::OP_PLUS) {
						if (binop->left->kind == IR::IR_INTEGER) {
							std::shared_ptr<IR::MemoryExpression> base =
								std::make_shared<IR::MemoryExpression>(binop->right);
							int offset = IR::ToIntegerExpression(binop->left)->value;
							makeOperand(base, add_inputs, notation);
							if (offset != 0)
								notation = IntToStr(offset) + notation;
						} else if (binop->right->kind == IR::IR_INTEGER) {
							std::shared_ptr<IR::MemoryExpression> base =
								std::make_shared<IR::MemoryExpression>(binop->left);
							int offset = IR::ToIntegerExpression(binop->right)->value;
							makeOperand(base, add_inputs, notation);
							if (offset != 0)
								notation = IntToStr(offset) + notation;
						} else
							Error::fatalError("Too complicated x86_64 assembler memory address");
					} else if (binop->operation == IR::OP_MINUS) {
						if (binop->right->kind == IR::IR_INTEGER) {
							std::shared_ptr<IR::MemoryExpression> base =
								std::make_shared<IR::MemoryExpression>(binop->left);
							int offset = -IR::ToIntegerExpression(binop->right)->value;
							makeOperand(base, add_inputs, notation);
							if (offset != 0)
								notation = IntToStr(offset) + notation;
						} else
							Error::fatalError("Too complicated x86_64 assembler memory address");
					} else
						Error::fatalError("Too complicated x86_64 assembler memory address");
					break;
				}
			default:
				Error::fatalError("Too complicated x86_64 assembler memory address");
			}
			break;
		}
		default:
			Error::fatalError("Strange assembler operand");
	}
}
	
void X86_64Assembler::addInstruction(Instructions &result,
	const std::string &prefix, IR::Expression operand,
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
	std::vector<IR::VirtualRegister *> outputs;
	if (output0 != NULL) {
		outputs.push_back(output0);
		if (extra_output != NULL)
			outputs.push_back(extra_output);
	}
	Instructions::iterator position = result.end();
	if (insert_before != NULL)
		position = *insert_before;
	
	Instructions::iterator newinst = result.insert(position,
		Instruction(notation, inputs,
			outputs,
			(prefix == "movq ") && (operand->kind == IR::IR_REGISTER)));
	if (insert_before != NULL)
		debug("Spill: %s -> prepend %s", (*position).notation.c_str(),
			  (*newinst).notation.c_str());
}

void X86_64Assembler::placeCallArguments(const std::list<IR::Expression>& arguments,
	IR::AbstractFrame *frame,
	const IR::Expression parentfp_param, Instructions &result)
{
	if (parentfp_param) {
		translateExpression(parentfp_param, frame, 
			machine_registers[R10], result);
	}
	int arg_count = 0;
	auto arg = arguments.begin();
	for (; arg != arguments.end(); arg++) {
		assert((*arg)->kind == IR::IR_REGISTER);
		result.push_back(Instruction(
			"movq " + Instruction::Input(0) + ", " + Instruction::Output(0),
			{IR::ToRegisterExpression(*arg)->reg},
			{machine_registers[paramreg_list[arg_count]]},
			true));
		arg_count++;
		if (arg_count == paramreg_count)
			break;
	}
	if (arg_count == paramreg_count) {
		auto extra_arg = arguments.end();
		extra_arg--; 
		while (extra_arg != arg) {
			assert((*extra_arg)->kind == IR::IR_REGISTER);
			result.push_back(Instruction(
				"push " + Instruction::Input(0),
				{IR::ToRegisterExpression(*extra_arg)->reg}, {}, false));
			extra_arg--;
		}
	}
}

void X86_64Assembler::removeCallArguments(const std::list< IR::Expression>& arguments,
	Instructions &result)
{
	int stack_arg_count = arguments.size()-6;
	if (stack_arg_count > 0)
 		result.push_back(Instruction(std::string("add ") +
			IntToStr(stack_arg_count*8) + std::string(", %rsp")));
}

void X86_64Assembler::translateBlob(const IR::Blob& blob, Instructions& result)
{
	result.push_back(Instruction(blob.label->getName() + ":"));
	std::string content = "";
	for (unsigned char c: blob.data) {
		char buf[20];
		sprintf(buf, ".byte 0x%.2x\n", c);
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
	result.push_back(Instruction("# callee-save registers",
		{}, calleesave_registers, false));
	prologue_regs.resize(calleesave_count);
	for (int i = 0; i < calleesave_count; i++) {
		prologue_regs[i] = IRenvironment->addRegister();
		result.push_back(Instruction("movq " + Instruction::Input(0) +
			", " + Instruction::Output(0),
			{calleesave_registers[i]}, {prologue_regs[i]}, true));
	}
	
	const std::list<IR::VarLocation *>parameters =
		frame->getParameters();
	int param_index = 0;
	for (IR::VarLocation *param: parameters) {
		if (param_index < paramreg_count) {
			assert(param->isRegister());
			IR::VirtualRegister *storage_reg =
				((IR::X86_64VarLocation *) param)->getRegister();
			result.push_back(Instruction(std::string("movq ") +
				Instruction::Input(0) + ", " + Instruction::Output(0),
				{machine_registers[paramreg_list[param_index]]}, 
				{storage_reg}, true));
		} else
			assert(! param->isRegister());
		param_index++;
	}
	IR::VarLocation *parent_fp = frame->getParentFpForUs();
	if (parent_fp != NULL) {
		assert(parent_fp->isRegister());
		IR::VirtualRegister *storage_reg =
			((IR::X86_64VarLocation *) parent_fp)->getRegister();
		result.push_back(Instruction(std::string("movq ") +
			Instruction::Input(0) + ", " + Instruction::Output(0),
			{machine_registers[R10]}, 
			{storage_reg}, true));
	}
}

void X86_64Assembler::functionEpilogue(IR::AbstractFrame *frame,
	IR::VirtualRegister* result_storage,
	std::vector<IR::VirtualRegister *> &prologue_regs,
	Instructions &result)
{
	if (result_storage != NULL)
		result.push_back(Instruction("movq " + Instruction::Input(0) + ", " +
			Instruction::Output(0), {result_storage}, {machine_registers[RAX]},
			true));

	assert(prologue_regs.size() == calleesave_registers.size());
	for (int i = 0; i < calleesave_count; i++) {
		result.push_back(Instruction("movq " + Instruction::Input(0) +
			", " + Instruction::Output(0),
			{prologue_regs[i]}, {calleesave_registers[i]}, true));
	}
	result.push_back(Instruction("# callee-save registers",
		calleesave_registers, {}, false));
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
	for (Instruction &inst: result) {
		debug("Inserting frame pointer: %s", inst.notation.c_str());
		for (IR::VirtualRegister *output: inst.outputs)
			if (output->getIndex() == frame->getFramePointer()->getIndex())
				Error::fatalError("Cannot write to frame pointer");
		for (unsigned i = 0; i < inst.inputs.size(); i++)
			if (inst.inputs[i]->getIndex() == frame->getFramePointer()->getIndex()) {
				inst.inputs[i] = machine_registers[RSP];
				addOffset(inst.notation, i,
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
	std::shared_ptr<IR::MemoryExpression> replacement)
{
	for (unsigned i = 0; i < (*inst).inputs.size(); i++)
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
	std::shared_ptr<IR::MemoryExpression> replacement)
{
	for (unsigned i = 0; i < (*inst).outputs.size(); i++)
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
				assert(pos == (int)original.size()-3);
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
				code.insert(inst, Instruction(new_command, registers, {}, false));
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
		IR::Expression storage_exp =
			stored_location->createCode(stored_location->owner_frame);
		
		bool seen_usage = false, seen_assignment = false;
		for (Instructions::iterator inst = code.begin(); inst != code.end(); inst++) {
			bool is_assigned = false;
			for (IR::VirtualRegister *output: (*inst).outputs)
				if (output->getIndex() == reg->getIndex()) {
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
			for (IR::VirtualRegister *input: (*inst).inputs)
				if (input->getIndex() == reg->getIndex()) {
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
	} else {
		IR::X86_64VarLocation *stored_location =
			(IR::X86_64VarLocation *)frame->addVariable(".store." + reg->getName(),
				8, true);
		IR::Expression storage_exp = IR::ToMemoryExpression(
			stored_location->createCode(stored_location->owner_frame));
		
		for (Instructions::iterator inst = code.begin(); inst != code.end(); inst++) {
			debug("Spilling: %s", (*inst).notation.c_str());
			bool is_assigned = false, is_used = false;
			for (IR::VirtualRegister *output: (*inst).outputs)
				if (output->getIndex() == reg->getIndex())
					is_assigned = true;
			for (IR::VirtualRegister *input: (*inst).inputs)
				if (input->getIndex() == reg->getIndex())
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
					for (IR::VirtualRegister *&input: (*inst).inputs)
						if (input->getIndex() == reg->getIndex())
							input = temp;
					replaceRegisterAssignment(code, inst, reg,
						IR::ToMemoryExpression(storage_exp));
				}
			} else if (is_assigned) {
				replaceRegisterAssignment(code, inst, reg,
					IR::ToMemoryExpression(storage_exp));
			}
		}
	}
}

bool X86_64Assembler::canReverseCondJump(const Instruction &jump) const
{
	if ((jump.extra_destinations.size() == 1) && jump.jumps_to_next)
		return true;
	return false;
}

void X86_64Assembler::reverseCondJump(Instruction &jump, IR::Label *new_destination) const
{
	static const char *comparisons[] = {
		"je", "jne", "jl", "jle", "jg", "jge", "jb", "jbe", "ja", "jae", NULL
	};
	static const char *reverse[] = {
		"jne", "je", "jge", "jg", "jle", "jl", "jae", "ja", "jbe", "jb", NULL
	};
	unsigned i = jump.notation.find(' ');
	std::string prefix = jump.notation.substr(0, i);
	for (int cmp = 0; comparisons[cmp] != NULL; cmp++)
		if (prefix == comparisons[cmp]) {
			jump.notation = std::string(reverse[cmp]) + " " +
				new_destination->getName();
			return;
		}
	Error::fatalError("Supposed to be able to flip when requested");
}

}
