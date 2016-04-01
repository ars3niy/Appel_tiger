#include "x86_64assembler.h"
#include "errormsg.h"
#include <list>

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
	
	addTemplate(I_CALL, new IR::CallExpression(new IR::LabelAddressExpression(NULL)));
	addTemplate(I_CALL, new IR::CallExpression(new IR::RegisterExpression(NULL)));
	addTemplate(I_JMP, new IR::JumpStatement(new IR::LabelAddressExpression(NULL)));
	addTemplate(I_JMP, new IR::JumpStatement(new IR::RegisterExpression(NULL)));
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
	}
}

std::string X86_64Assembler::translateOperand(IR::Expression* expr)
{
}

void X86_64Assembler::translateExpressionTemplate(IR::Expression *templ,
	IR::VirtualRegister *value_storage,
	const std::list<TemplateChildInfo> &children, Instructions &result)
{
	switch (templ->kind) {
		case IR::IR_INTEGER:
		case IR::IR_LABELADDR:
		case IR::IR_REGISTER:
		case IR::IR_MEMORY:
			if (value_storage != NULL)
				result.push_back(std::string("mov ") + translateOperand(templ) +
					value_storage->getName());
			break;
		case IR::IR_BINARYOP:
			if (value_storage != NULL) {
				IR::BinaryOpExpression *bin_op = IR::ToBinaryOpExpression(templ);
				switch (bin_op->operation) {
					case IR::OP_PLUS:
						result.push_back(std::string("movq ") +
							translateOperand(bin_op->left) +
							", " + value_storage->getName());
						result.push_back(std::string("addq ") +
							translateOperand(bin_op->right) +
							", " + value_storage->getName());
						break;
					case IR::OP_MINUS:
						result.push_back(std::string("movq ") +
							translateOperand(bin_op->left) +
							", " + value_storage->getName());
						result.push_back(std::string("subq ") +
							translateOperand(bin_op->right) +
							", " + value_storage->getName());
						break;
					case IR::OP_MUL:
						result.push_back(std::string("movq ") +
							translateOperand(bin_op->left) +
							", %rax");
						result.push_back(std::string("imulq ") +
							translateOperand(bin_op->right));
						result.push_back(std::string("movq %rax, ") +
							value_storage->getName());
						break;
					case IR::OP_DIV:
						result.push_back(std::string("movq ") +
							translateOperand(bin_op->left) +
							", %rax");
						result.push_back("cqo");
						result.push_back(std::string("idivq ") +
							translateOperand(bin_op->right));
						result.push_back(std::string("movq %rax, ") +
							value_storage->getName());
						break;
					default:
						Error::fatalError("X86_64Assembler::translateExpressionTemplate unexpected binary operation");
				}
			}
		case IR::IR_FUN_CALL: {
			IR::CallExpression *call = IR::ToCallExpression(templ);
			/* TODO:
			 * pass arguments
			 * pass frame pointer (if needed??)
			 * cleanup arguments
			 */
			result.push_back(std::string("call ") + translateOperand(call->function));
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
			result.push_back(std::string("jmp ") +
				IR::ToLabelAddressExpression(jump->dest)->label->getName());
			break;
		}
		case IR::IR_COND_JUMP: {
			IR::CondJumpStatement *jump = IR::ToCondJumpStatement(templ);
			result.push_back(std::string("cmp ") +
				translateOperand(jump->right) + ", " + translateOperand(jump->left));
			result.push_back(std::string(cond_jumps[jump->comparison]) +
				jump->true_dest->getName());
			break;
		}
		case IR::IR_LABEL:
			result.push_back(IR::ToLabelPlacementStatement(templ)->label->getName() + ":");
			break;
		case IR::IR_MOVE: {
			IR::MoveStatement *move = IR::ToMoveStatement(templ);
			const char *cmd = "mov ";
			IR::Expression *from = move->from;
			if (move->from->kind == IR::IR_BINARYOP) {
				cmd = "lea ";
				from = new IR::MemoryExpression(from);
			}
			result.push_back(std::string(cmd) +
				translateOperand(from) + ", " + translateOperand(move->to));
			if (move->from->kind == IR::IR_BINARYOP) {
				delete IR::ToMemoryExpression(from);
			}
		}
		default:
			Error::fatalError("X86_64Assembler::translateStatementTemplate unexpected statement kind");
	}
}

	
}
