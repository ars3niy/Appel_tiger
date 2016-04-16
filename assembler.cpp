#include "assembler.h"
#include "errormsg.h"
#include "error.h"
#include <stdarg.h>

namespace Asm {

Instruction::Instruction(const std::string &_notation,
	bool _reg_to_reg_assign)
		: notation(_notation), label(NULL),
		is_reg_to_reg_assign(_reg_to_reg_assign)
{
	jumps_to_next = true;
}

Instruction::Instruction(const std::string &_notation,
		const std::vector<IR::VirtualRegister *> &_inputs,
		const std::vector<IR::VirtualRegister *> &_outputs,
		bool _reg_to_reg_assign,
		const std::vector<IR::Label *> &_destinations,
		bool also_jump_to_next) :
	notation(_notation), inputs(_inputs), outputs(_outputs),
	//destinations(_destinations),
	label(NULL),
	is_reg_to_reg_assign(_reg_to_reg_assign)
{
	//if (destinations.size() == 0)
	//	destinations.push_back(NULL);
	if (_destinations.size() == 0)
		jumps_to_next = true;
	else {
		extra_destinations = _destinations;
		jumps_to_next = also_jump_to_next;
	}
}

Assembler::Assembler(IR::IREnvironment *ir_env)
	: IRenvironment(ir_env), DebugPrinter("assembler.log")
{
	expr_templates.resize((int)IR::IR_EXPR_MAX);
	for (int i = 0; i < IR::IR_EXPR_MAX; i++) {
		if (i == (int)IR::IR_BINARYOP)
			expr_templates[i] = new TemplateOpKindKindMap(IR::BINOP_MAX);
		else
			expr_templates[i] = new TemplateList;
	}
	
	statm_templates.resize((int)IR::IR_STAT_MAX);
	for (int i = 0; i < IR::IR_STAT_MAX; i++) {
		if (i == (int)IR::IR_COND_JUMP)
			statm_templates[i] = new TemplateOpKindKindMap(IR::COMPOP_MAX);
		else
			statm_templates[i] = new TemplateList;
	}
}

std::list<InstructionTemplate> *Assembler::getTemplatesList(IR::Expression expr)
{
	if (expr->kind == IR::IR_BINARYOP) {
		std::shared_ptr<IR::BinaryOpExpression> binop = IR::ToBinaryOpExpression(expr);
		return ((TemplateOpKindKindMap *)expr_templates[(int)expr->kind])->
			getList((int)binop->operation/*, (int)binop->left->kind, (int)binop->right->kind*/);
	} else
		return & ((TemplateList *)expr_templates[(int)expr->kind])->list;
}

std::list<InstructionTemplate> *Assembler::getTemplatesList(IR::Statement statm)
{
	if ((statm->kind == IR::IR_COND_JUMP) /*|| (statm->kind = IR::IR_MOVE)*/) {
		int op, kind1, kind2;
// 		if (statm->kind == IR::IR_COND_JUMP) {
			std::shared_ptr<IR::CondJumpStatement> cj = IR::ToCondJumpStatement(statm);
			op = (int)cj->comparison;
			kind1 = (int)cj->left->kind;
			kind2 = (int)cj->right->kind;
// 		} else {
// 			IR::MoveStatement *m = IR::ToMoveStatement(statm);
// 			op = 0;
// 			kind1 = (int)m->to->kind;
// 			kind2 = (int)m->from->kind;
// 		}
		return ((TemplateOpKindKindMap *)statm_templates[(int)statm->kind])->
			getList(op/*, kind1, kind2*/);
	} else
		return & ((TemplateList *)statm_templates[(int)statm->kind])->list;
}

void Assembler::addTemplate(int code, IR::Expression expr)
{
	std::list<InstructionTemplate> *list = getTemplatesList(expr);
	list->push_back(InstructionTemplate(code, expr));
}

void Assembler::addTemplate(int code, IR::Statement statm)
{
	std::list<InstructionTemplate> *list = getTemplatesList(statm);
	list->push_back(InstructionTemplate(code, statm));
}

bool Assembler::MatchMoveDestination(IR::Expression expression, IR::Expression templ,
	int &nodecount, std::list<TemplateChildInfo> *children,
	IR::Expression *template_instantiation)
{
	if (templ->kind != expression->kind)
		return false;
	return MatchExpression(expression, templ, nodecount, children, template_instantiation);
}

bool Assembler::MatchExpression(IR::Expression expression, IR::Expression templ,
	int &nodecount, std::list<TemplateChildInfo> *children,
	IR::Expression *template_instantiation)
{
	if ((templ->kind == IR::IR_REGISTER) && (expression->kind != IR::IR_REGISTER)) {
		// Matched register template against non-register expression
		// Non-register expression gets computed separately and
		// "glued" to the template
		if (children != NULL) {
			children->push_back(TemplateChildInfo(NULL, expression));
			if (template_instantiation != NULL) {
				IR::VirtualRegister *value_storage = IRenvironment->addRegister();
				children->back().value_storage = value_storage;
				*template_instantiation = std::make_shared<IR::RegisterExpression>(value_storage);
			}
		}
		return true;
	}
	
	if (templ->kind != expression->kind)
		return false;
	
	nodecount++;
	assert(templ->kind != IR::IR_STAT_EXP_SEQ);
	
	switch (templ->kind) {
		case IR::IR_INTEGER:
			if (template_instantiation != NULL)
				*template_instantiation = std::make_shared<IR::IntegerExpression>(
					IR::ToIntegerExpression(expression)->value);
			return (IR::ToIntegerExpression(templ)->value == 0) ||
				(IR::ToIntegerExpression(templ)->value ==
				IR::ToIntegerExpression(expression)->value);
		case IR::IR_LABELADDR:
			if (template_instantiation != NULL)
				*template_instantiation = std::make_shared<IR::LabelAddressExpression>(
					IR::ToLabelAddressExpression(expression)->label);
			return true;
		case IR::IR_REGISTER:
			if (template_instantiation != NULL)
				*template_instantiation = std::make_shared<IR::RegisterExpression>(
					IR::ToRegisterExpression(expression)->reg);
			return true;
		case IR::IR_FUN_CALL: {
			std::shared_ptr<IR::CallExpression> call_expr = IR::ToCallExpression(expression);
			std::shared_ptr<IR::CallExpression> call_inst = nullptr;
			IR::Expression *func_inst = NULL;
			if (template_instantiation != NULL) {
				call_inst = std::make_shared<IR::CallExpression>(nullptr, call_expr->callee_parentfp);
				func_inst = &call_inst->function;
				*template_instantiation = call_inst;
			}
			if (children != NULL) {
				for (IR::Expression arg: call_expr->arguments) {
					children->push_back(TemplateChildInfo(NULL, arg));
					if (call_inst != NULL) {
						IR::VirtualRegister *arg_reg = IRenvironment->addRegister();
						children->back().value_storage = arg_reg;
						call_inst->addArgument(std::make_shared<IR::RegisterExpression>(arg_reg));
					}
				}
			}
			return MatchExpression(
				call_expr->function, IR::ToCallExpression(templ)->function,
				nodecount, children, func_inst);
		}
		case IR::IR_BINARYOP: {
			std::shared_ptr<IR::BinaryOpExpression> binop_expr =
				IR::ToBinaryOpExpression(expression);
			std::shared_ptr<IR::BinaryOpExpression> binop_templ =
				IR::ToBinaryOpExpression(templ);
			
			if (binop_expr->operation != binop_templ->operation)
				return false;
			IR::Expression *left_inst = NULL, *right_inst = NULL;
			if (template_instantiation != NULL) {
				std::shared_ptr<IR::BinaryOpExpression> binop_inst =
					std::make_shared<IR::BinaryOpExpression>(
						binop_expr->operation, nullptr, nullptr);
				*template_instantiation = binop_inst;
				left_inst = &binop_inst->left;
				right_inst = &binop_inst->right;
			}
			return MatchExpression(binop_expr->left, binop_templ->left,
					nodecount, children, left_inst) && 
				MatchExpression(binop_expr->right, binop_templ->right,
					nodecount, children, right_inst);
		}
		case IR::IR_MEMORY: {
			IR::Expression *addr_inst = NULL;
			if (template_instantiation != NULL) {
				std::shared_ptr<IR::MemoryExpression> mem_inst =
					std::make_shared<IR::MemoryExpression>(nullptr);
				*template_instantiation = mem_inst;
				addr_inst = &mem_inst->address;
			}
			return MatchExpression(
				IR::ToMemoryExpression(expression)->address,
				IR::ToMemoryExpression(templ)->address,
				nodecount, children, addr_inst);
		}
		default:
			Error::fatalError("Strange expression template kind");
	}
}

bool Assembler::MatchStatement(IR::Statement statement, IR::Statement templ,
	int &nodecount, std::list<TemplateChildInfo> *children,
	IR::Statement *template_instantiation)
{
	// getTemplatesList takes care of this
	assert(statement->kind == templ->kind);
	
	nodecount++;
	
	switch (statement->kind) {
		case IR::IR_LABEL:
			if (template_instantiation != NULL)
				*template_instantiation = std::make_shared<IR::LabelPlacementStatement>(
					IR::ToLabelPlacementStatement(statement)->label);
			return true;
		case IR::IR_MOVE: {
			std::shared_ptr<IR::MoveStatement> move_statm = IR::ToMoveStatement(statement);
			std::shared_ptr<IR::MoveStatement> move_templ = IR::ToMoveStatement(templ);
			IR::Expression *to_inst = NULL, *from_inst = NULL;
			if (template_instantiation != NULL) {
				std::shared_ptr<IR::MoveStatement>move_inst =
					std::make_shared<IR::MoveStatement>(nullptr, nullptr);
				*template_instantiation = move_inst;
				to_inst = &move_inst->to;
				from_inst = &move_inst->from;
			}
			return MatchMoveDestination(move_statm->to, move_templ->to, 
					nodecount, children, to_inst) &&
				MatchExpression(move_statm->from, move_templ->from, 
					nodecount, children, from_inst);
		}
		case IR::IR_JUMP: {
			IR::Expression *dest_inst = NULL;
			if (template_instantiation != NULL) {
				std::shared_ptr<IR::JumpStatement> jump_inst =
					std::make_shared<IR::JumpStatement>(nullptr);
				*template_instantiation = jump_inst;
				dest_inst = &jump_inst->dest;
			}
			return MatchExpression(
				IR::ToJumpStatement(statement)->dest,
				IR::ToJumpStatement(templ)->dest,
				nodecount, children, dest_inst);
		}
		case IR::IR_COND_JUMP: {
			std::shared_ptr<IR::CondJumpStatement> cj_statm = IR::ToCondJumpStatement(statement);
			std::shared_ptr<IR::CondJumpStatement> cj_templ = IR::ToCondJumpStatement(templ);
			
			// getTemplatesList takes care of this
			assert(cj_statm->comparison == cj_templ->comparison);

			IR::Expression *left_inst = NULL, *right_inst = NULL;
			if (template_instantiation != NULL) {
				std::shared_ptr<IR::CondJumpStatement> cj_inst =
					std::make_shared<IR::CondJumpStatement>(
						cj_statm->comparison, nullptr, nullptr, cj_statm->true_dest,
						cj_statm->false_dest);
				*template_instantiation = cj_inst;
				left_inst = &cj_inst->left;
				right_inst = &cj_inst->right;
			}
			return MatchExpression(cj_statm->left, cj_templ->left, 
					nodecount, children, left_inst) &&
				MatchExpression(cj_statm->right, cj_templ->right, 
					nodecount, children, right_inst);
		}
		default:
			Error::fatalError("Strange statement template kind");
	}
}

void Assembler::FindExpressionTemplate(IR::Expression expression,
	InstructionTemplate* &templ)
{
	std::list<InstructionTemplate> *templates = getTemplatesList(expression);
	int best_nodecount = -1;
	templ = NULL;
	for (InstructionTemplate &cur_templ: *templates) {
		assert(cur_templ.code->kind == IR::CODE_EXPRESSION);
		IR::Expression templ_exp = std::static_pointer_cast<IR::ExpressionCode>(cur_templ.code)->exp;
		int nodecount = 0;
		if (MatchExpression(expression, templ_exp, nodecount)) {
			if (nodecount > best_nodecount) {
				best_nodecount = nodecount;
				templ = &cur_templ;
			}
		}
	}
}

void Assembler::FindStatementTemplate(IR::Statement statement,
	InstructionTemplate* &templ)
{
	std::list<InstructionTemplate> *templates = getTemplatesList(statement);
	int best_nodecount = -1;
	templ = NULL;

	for (InstructionTemplate &cur_templ: *templates) {
		assert(cur_templ.code->kind == IR::CODE_STATEMENT);
		IR::Statement templ_statm = std::static_pointer_cast<IR::StatementCode>(cur_templ.code)->statm;
		int nodecount = 0;
		if (MatchStatement(statement, templ_statm, nodecount)) {
			if (nodecount > best_nodecount) {
				best_nodecount = nodecount;
				templ = &cur_templ;
			}
		}
	}
}

void Assembler::translateExpression(IR::Expression expression,
	IR::AbstractFrame *frame, IR::VirtualRegister* value_storage, 
	Instructions& result)
{
	if (expression->kind == IR::IR_STAT_EXP_SEQ) {
		std::shared_ptr<IR::StatExpSequence> statexp = IR::ToStatExpSequence(expression);
		translateStatement(statexp->stat, frame, result);
		translateExpression(statexp->exp, frame, value_storage, result);
	} else {
		InstructionTemplate *templ;
		FindExpressionTemplate(expression, templ);
		if (templ == NULL)
			Error::fatalError("Failed to find expression template");
		
		std::list<TemplateChildInfo> children;
		IR::Expression template_instantiation;
		assert(templ->code->kind == IR::CODE_EXPRESSION);
		int nodecount = 0;
		MatchExpression(expression,
			std::static_pointer_cast<IR::ExpressionCode>(templ->code)->exp,
			nodecount, &children, &template_instantiation);
		for (TemplateChildInfo &child: children)
			translateExpression(child.expression, frame, child.value_storage,
				result);
		translateExpressionTemplate(template_instantiation, frame, value_storage,
			children, result);
	}
}

void Assembler::translateStatement(IR::Statement statement, 
	IR::AbstractFrame *frame, Instructions& result)
{
	if (statement->kind == IR::IR_STAT_SEQ) {
		std::shared_ptr<IR::StatementSequence> seq = IR::ToStatementSequence(statement);
		for (IR::Statement statm: seq->statements)
			 translateStatement(statm, frame, result);
	} else if (statement->kind == IR::IR_EXP_IGNORE_RESULT) {
		translateExpression(IR::ToExpressionStatement(statement)->exp, 
			frame, NULL, result);
	} else {
		InstructionTemplate *templ;
		FindStatementTemplate(statement, templ);
		if (templ == NULL)
			Error::fatalError("Failed to find statement template");
		
		std::list<TemplateChildInfo> children;
		IR::Statement template_instantiation;
		assert(templ->code->kind == IR::CODE_STATEMENT);
		int nodecount = 0;
		MatchStatement(statement,
			std::static_pointer_cast<IR::StatementCode>(templ->code)->statm,
			nodecount, &children, &template_instantiation);
		for (TemplateChildInfo &child: children) {
			translateExpression(child.expression, frame, child.value_storage,
				result);
		}
		translateStatementTemplate(template_instantiation, children, result);
		if (statement->kind == IR::IR_LABEL)
			result.back().label = IR::ToLabelPlacementStatement(statement)->label;
	}
}

IR::VirtualRegister *MapRegister(const IR::RegisterMap *register_map,
	IR::VirtualRegister *reg)
{
	if ((register_map != NULL) && (register_map->size() > reg->getIndex()) &&
		((*register_map)[reg->getIndex()] != NULL))
		return (*register_map)[reg->getIndex()];
	else
		return reg;
}

void Assembler::outputCode(FILE* output, const std::list<Instructions>& code,
	const IR::RegisterMap *register_map)
{
	std::string header;
	getCodeSectionHeader(header);
	fwrite(header.c_str(), header.size(), 1, output);
	if ((header.size() > 0) && (header[header.size()-1] != '\n'))
		fputc('\n', output);
	
	for (const Instructions &chunk: code) {
		int line = -1;
		for (const Instruction &inst: chunk) {
			line++;
			const std::string &s = inst.notation;
			
			if (inst.is_reg_to_reg_assign) {
				assert(inst.inputs.size() == 1);
				assert(inst.outputs.size() == 1);
				if (MapRegister(register_map, inst.inputs[0])->getIndex() ==
						MapRegister(register_map, inst.outputs[0])->getIndex())
					continue;
			}
			int len = 0;
			if ((s.size() > 0) && (s[s.size()-1] != ':')) {
				fputs("    ", output);
				len += 4;
			}
		
			int i = 0;
			while (i < s.size()) {
				if (s[i] != '&') {
					fputc(s[i], output);
					len++;
					i++;
				} else {
					i++;
					if (i+1 < s.size()) {
						int arg_index = s[i+1] - '0';
						const std::vector<IR::VirtualRegister *> *registers;
						
						if (s[i] == 'i')
							registers = &inst.inputs;
						else if (s[i] == 'o')
							registers = &inst.outputs;
						else
							Error::fatalError("Misformed instruction");
						
						if (arg_index > (*registers).size())
							Error::fatalError("Misformed instruction");
						else {
							IR::VirtualRegister *reg =
								MapRegister(register_map, (*registers)[arg_index]);
							fprintf(output, "%s", reg->getName().c_str());
							len += reg->getName().size();
						}
						i += 2;
					}
				}
			}
			for (int i = 0; i < 35-len; i++)
				fputc(' ', output);
			fprintf(output, " # %2d ", line);
			if (inst.is_reg_to_reg_assign)
				fprintf(output, "MOVE ");
			bool use_reg = false;
			if (inst.inputs.size() > 0) {
				use_reg = true;
				fprintf(output, "<- ");
				for (int i = 0; i < inst.inputs.size(); i++)
					fprintf(output, "%s ", inst.inputs[i]->getName().c_str());
			}
			if (inst.outputs.size() > 0) {
				use_reg = true;
				fprintf(output, "-> ");
				for (int i = 0; i < inst.outputs.size(); i++)
					fprintf(output, "%s ", inst.outputs[i]->getName().c_str());
			}
			if (! use_reg)
				fprintf(output, "no register use");
			fputc('\n', output);
		}
	}
}

void Assembler::outputBlobs(FILE* output, const std::list< IR::Blob >& blobs)
{
	Instructions content;
	for (const IR::Blob &blob: blobs)
		translateBlob(blob, content);
	
	std::string header;
	getBlobSectionHeader(header);
	fwrite(header.c_str(), header.size(), 1, output);
	if ((header.size() > 0) && (header[header.size()-1] != '\n'))
		fputc('\n', output);
	
	for (Instruction &inst: content)
		fprintf(output, "%s\n", inst.notation.c_str());
}

void Assembler::translateFunctionBody(IR::Code code, IR::Label *fcn_label,
	IR::AbstractFrame *frame, Instructions &result)
{
	if (code == NULL)
		// External function, ignore
		return;
	switch (code->kind) {
		case IR::CODE_EXPRESSION: {
			IR::VirtualRegister *result_storage = IRenvironment->addRegister();
			std::vector<IR::VirtualRegister *> prologue_regs;
			functionPrologue(fcn_label, frame, result, prologue_regs);
			translateExpression(std::static_pointer_cast<IR::ExpressionCode>(code)->exp,
				frame, result_storage, result);
			functionEpilogue(frame, result_storage, prologue_regs, result);
			break;
		}
		case IR::CODE_STATEMENT: {
			std::vector<IR::VirtualRegister *> prologue_regs;
			functionPrologue(fcn_label, frame, result, prologue_regs);
			translateStatement(std::static_pointer_cast<IR::StatementCode>(code)->statm,
				frame, result);
			functionEpilogue(frame, NULL, prologue_regs, result);
			break;
		}
		default:
			Error::fatalError("Assembler::translateFunctionBody strange body code");
	}
}


void Assembler::translateProgram(IR::Statement program, IR::AbstractFrame *frame,
	Instructions& result)
{
	translateStatement(program, frame, result);
}

void Assembler::implementProgramFrameSize(IR::AbstractFrame *frame,
	Instructions &result)
{
	programPrologue(frame, result);
	programEpilogue(frame, result);
	implementFramePointer(frame, result);
}

void Assembler::implementFunctionFrameSize(IR::Label *fcn_label,
	IR::AbstractFrame *frame, Instructions &result)
{
	framePrologue(fcn_label, frame, result);
	frameEpilogue(frame, result);
		implementFramePointer(frame, result);
}

}
