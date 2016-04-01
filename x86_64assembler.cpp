#include "x86_64assembler.h"
#include "x86_64_frame.h"
#include "errormsg.h"
#include <list>
#include <assert.h>

namespace Asm {

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
	
	addTemplate(I_CALL, new IR::CallExpression(new IR::LabelAddressExpression(NULL), false));
	addTemplate(I_CALL, new IR::CallExpression(new IR::RegisterExpression(NULL), false));
	addTemplate(I_JMP, new IR::JumpStatement(new IR::LabelAddressExpression(NULL)));
	addTemplate(I_JMP, new IR::JumpStatement(new IR::RegisterExpression(NULL)));
	
	rax = IRenvironment->addRegister("%rax");
	rdx = IRenvironment->addRegister("%rdx");
	rcx = IRenvironment->addRegister("%rcx");
	rsi = IRenvironment->addRegister("%rsi");
	rdi = IRenvironment->addRegister("%rdi");
	r8 = IRenvironment->addRegister("%r8");
	r9 = IRenvironment->addRegister("%r9");
	r10 = IRenvironment->addRegister("%r10");
	r11 = IRenvironment->addRegister("%r11");
}

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

std::string IntToStr(int x)
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
	IR::VirtualRegister *output1)
{
	std::vector<IR::VirtualRegister *> inputs;
	std::string notation;
	makeOperand(operand, inputs, notation);
	notation = prefix + notation + suffix;
	IR::VirtualRegister *outputs[] = {output0, output1};
	int noutput = 0;
	if (output0 != NULL) {
		if (output1 != NULL)
			noutput = 2;
		else
			noutput = 1;
	}
	result.push_back(Instruction(notation, inputs.size(), inputs.data(),
		noutput, outputs));
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

std::string X86_64Assembler::translateOperand(IR::Expression* expr)
{
	switch (expr->kind) {
		case IR::IR_INTEGER:
			return std::string("$") + IntToStr(IR::ToIntegerExpression(expr)->value);
		case IR::IR_LABELADDR:
			return std::string("$") + IR::ToLabelAddressExpression(expr)->label->getName();
		case IR::IR_REGISTER:
			return std::string("%") + IR::ToRegisterExpression(expr)->reg->getName();
		case IR::IR_MEMORY: {
			IR::Expression *addr = IR::ToMemoryExpression(expr)->address;
			switch (addr->kind) {
				case IR::IR_REGISTER:
					return std::string("(%") +
						IR::ToRegisterExpression(addr)->reg->getName() + ")";
				case IR::IR_LABELADDR:
					return std::string("(") +
						IR::ToLabelAddressExpression(addr)->label->getName() + ")";
				case IR::IR_BINARYOP: {
					IR::BinaryOpExpression *binop = IR::ToBinaryOpExpression(addr);
					if (binop->operation == IR::OP_PLUS) {
						if (binop->left->kind == IR::IR_INTEGER) {
							IR::MemoryExpression base(binop->right);
							int offset = IR::ToIntegerExpression(binop->left)->value;
							if (offset != 0)
								return IntToStr(offset) + translateOperand(&base);
							else
								return translateOperand(&base);
						} else if (binop->right->kind == IR::IR_INTEGER) {
							IR::MemoryExpression base(binop->left);
							int offset = IR::ToIntegerExpression(binop->right)->value;
							if (offset != 0)
								return IntToStr(offset) + translateOperand(&base);
							else
								return translateOperand(&base);
						} else
							Error::fatalError("Too complicated x86_64 assembler memory address");
					} else if (binop->operation == IR::OP_MINUS) {
						if (binop->right->kind == IR::IR_INTEGER) {
							IR::MemoryExpression base(binop->left);
							int offset = -IR::ToIntegerExpression(binop->right)->value;
							if (offset != 0)
								return IntToStr(offset) + translateOperand(&base);
							else
								return translateOperand(&base);
						} else
							Error::fatalError("Too complicated x86_64 assembler memory address");
					} else
						Error::fatalError("Too complicated x86_64 assembler memory address");
				}
				default:
					Error::fatalError("Strange x86_64 assembler memory address");
			}
		}
		default:
			Error::fatalError("Strange x86_64 assembler operand");
	}
}

void X86_64Assembler::placeCallArguments(const std::list<IR::Expression * >& arguments,
	Instructions &result)
{
	IR::VirtualRegister *arg_registers[] = {rdi, rsi, rdx, rcx, r8, r9};
	int arg_count = 0;
	std::list<IR::Expression *>::const_iterator arg;
	for (arg = arguments.begin(); arg != arguments.end(); arg++) {
		assert((*arg)->kind == IR::IR_REGISTER);
		result.push_back(Instruction(
			"movq " + Instruction::Input(0) + ", " + Instruction::Output(0),
			1, &IR::ToRegisterExpression(*arg)->reg, 1, &arg_registers[arg_count]));
		arg_count++;
		if (arg_count == 6)
			break;
	}
	if (arg_count == 6) {
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
			if (value_storage != NULL)
				addInstruction(result, "movq ", templ,
					", " + Instruction::Output(0), value_storage);
			break;
		case IR::IR_BINARYOP:
			if (value_storage != NULL) {
				IR::BinaryOpExpression *bin_op = IR::ToBinaryOpExpression(templ);
				switch (bin_op->operation) {
					case IR::OP_PLUS:
						addInstruction(result, "movq ", bin_op->left,
							", " + Instruction::Output(0), value_storage);
						addInstruction(result, "addq ", bin_op->right,
							", " + Instruction::Output(0), value_storage);
						break;
					case IR::OP_MINUS:
						addInstruction(result, "movq ", bin_op->left,
							", " + Instruction::Output(0), value_storage);
						addInstruction(result, "subq ", bin_op->right,
							", " + Instruction::Output(0), value_storage);
						break;
					case IR::OP_MUL:
						addInstruction(result, "movq ", bin_op->left,
							", " + Instruction::Output(0), rax);
						addInstruction(result, "imulq ", bin_op->right,
							", " + Instruction::Output(0), rax, rdx);
						result.push_back(Instruction("movq " +
							Instruction::Input(0) + ", " + Instruction::Output(0),
							1, &rax, 1, &value_storage));
						break;
					case IR::OP_DIV:
						addInstruction(result, "movq ", bin_op->left,
							", " + Instruction::Output(0), rax);
						result.push_back(Instruction("cqo",
							0, NULL, 1, &rdx));
						addInstruction(result, "imulq ", bin_op->right,
							", " + Instruction::Output(0), rax, rdx);
						result.push_back(Instruction("movq " +
							Instruction::Input(0) + ", " + Instruction::Output(0),
							1, &rax, 1, &value_storage));
						break;
					default:
						Error::fatalError("X86_64Assembler::translateExpressionTemplate unexpected binary operation");
				}
			}
			break;
		case IR::IR_FUN_CALL: {
			IR::CallExpression *call = IR::ToCallExpression(templ);
			/* TODO:
			 * pass arguments
			 * pass frame pointer (if needed??)
			 * cleanup arguments
			 */
			if (call->needs_parent_fp)
				result.push_back(Instruction(
					"movq " + Instruction::Input(0) + ", " + Instruction::Output(0),
					1, &((IR::X86_64Frame *)frame)->framepointer, 1, &r10));
			placeCallArguments(call->arguments, result);
			assert(call->function->kind == IR::IR_LABELADDR);
			IR::VirtualRegister *callersave[] = {rax, rdx, rcx, rsi, rdi, r8, r9, r10, r11};
			result.push_back(Instruction("call " + 
				IR::ToLabelAddressExpression(call->function)->label->getName(),
				0, NULL, sizeof(callersave)/sizeof(callersave[0]), callersave));
			removeCallArguments(call->arguments, result);
			if (value_storage != NULL)
				result.push_back(Instruction("movq " + Instruction::Input(0) +
					", " + Instruction::Output(0),
					1, &rax, 1, &value_storage));
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
				jump->true_dest->getName(), 0, NULL, 0, NULL, 2, destinations));
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
				addInstruction(result, "leaq ", fake_addr, ", ",
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
	std::string content = ".ascii \"";
	for (int i = 0; i < blob.data.size(); i++)
		if (isalnum(blob.data[i]))
			content += blob.data[i];
		else {
			char buf[10];
			sprintf(buf, "\\x%.2x", blob.data[i]);
			content += buf;
		}
	result.push_back(Instruction(content + "\""));
}

void X86_64Assembler::getBlobSectionHeader(std::string& header)
{
	header = ".section\t.rodata\n";
}

void X86_64Assembler::getCodeSectionHeader(std::string& header)
{
	header = ".text\n.global main\nmain:\n";
}

void X86_64Assembler::functionEpilogue(IR::VirtualRegister* result_storage,
	Instructions &result)
{
	result.push_back(Instruction("movq " + Instruction::Input(0) + ", " +
		Instruction::Output(0), 1, &result_storage, 1, &rax));
}

void X86_64Assembler::programEpilogue(Instructions &result)
{
	result.push_back(Instruction("movq $0, %rax"));
	result.push_back(Instruction("ret"));
}

}
