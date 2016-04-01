#include "assembler.h"
#include "errormsg.h"
#include "error.h"

namespace Asm {

Assembler::Assembler(IR::IREnvironment *ir_env)
	: IRenvironment(ir_env)
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

std::list<InstructionTemplate> *Assembler::getTemplatesList(IR::Expression *expr)
{
	if (expr->kind == IR::IR_BINARYOP) {
		IR::BinaryOpExpression *binop = IR::ToBinaryOpExpression(expr);
		return ((TemplateOpKindKindMap *)expr_templates[(int)expr->kind])->
			getList((int)binop->operation/*, (int)binop->left->kind, (int)binop->right->kind*/);
	} else
		return & ((TemplateList *)expr_templates[(int)expr->kind])->list;
}

std::list<InstructionTemplate> *Assembler::getTemplatesList(IR::Statement *statm)
{
	if ((statm->kind == IR::IR_COND_JUMP) /*|| (statm->kind = IR::IR_MOVE)*/) {
		int op, kind1, kind2;
// 		if (statm->kind == IR::IR_COND_JUMP) {
			IR::CondJumpStatement *cj = IR::ToCondJumpStatement(statm);
			op = (int)cj->comparison;
			kind1 = (int)cj->left->kind;
			kind2 = (int)cj->right->kind;
// 		} else {
// 			IR::MoveStatement *m = IR::ToMoveStatement(statm);
// 			op = 0;
// 			kind1 = (int)m->to->kind;
// 			kind2 = (int)m->from->kind;
// 		}
		return ((TemplateOpKindKindMap *)expr_templates[(int)statm->kind])->
			getList(op/*, kind1, kind2*/);
	} else
		return & ((TemplateList *)statm_templates[(int)statm->kind])->list;
}

void Assembler::addTemplate(int code, IR::Expression *expr)
{
	std::list<InstructionTemplate> *list = getTemplatesList(expr);
	list->push_back(InstructionTemplate(code, expr));
}

void Assembler::addTemplate(int code, IR::Statement *statm)
{
	std::list<InstructionTemplate> *list = getTemplatesList(statm);
	list->push_back(InstructionTemplate(code, statm));
}

bool Assembler::MatchExpression(IR::Expression *expression, IR::Expression *templ,
	int &nodecount, std::list<TemplateChildInfo> *children,
	IR::Expression **template_instantiation)
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
				*template_instantiation = new IR::RegisterExpression(value_storage);
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
				*template_instantiation = new IR::IntegerExpression(
					IR::ToIntegerExpression(expression)->value);
			return (IR::ToIntegerExpression(templ)->value == 0) ||
				(IR::ToIntegerExpression(templ)->value ==
				IR::ToIntegerExpression(expression)->value);
		case IR::IR_LABELADDR:
			if (template_instantiation != NULL)
				*template_instantiation = new IR::LabelAddressExpression(
					IR::ToLabelAddressExpression(expression)->label);
			return true;
		case IR::IR_REGISTER:
			if (template_instantiation != NULL)
				*template_instantiation = new IR::RegisterExpression(
					IR::ToRegisterExpression(expression)->reg);
			return true;
		case IR::IR_FUN_CALL: {
			IR::CallExpression *call_expr = IR::ToCallExpression(expression);
			IR::CallExpression *call_inst = NULL;
			IR::Expression **func_inst = NULL;
			if (template_instantiation != NULL) {
				call_inst = new IR::CallExpression(NULL);
				func_inst = &call_inst->function;
			}
			if (children != NULL) {
				for (std::list<IR::Expression *>::iterator arg =
					call_expr->arguments.begin(); arg != call_expr->arguments.end();
					arg++) {
					children->push_back(TemplateChildInfo(NULL, *arg));
					if (call_inst != NULL) {
						IR::VirtualRegister *arg_reg = IRenvironment->addRegister();
						children->back().value_storage = arg_reg;
						call_inst->addArgument(new IR::RegisterExpression(arg_reg));
					}
				}
			}
			return MatchExpression(
				call_expr, IR::ToCallExpression(templ)->function,
				nodecount, children, func_inst);
		}
		case IR::IR_BINARYOP: {
			IR::BinaryOpExpression *binop_expr = IR::ToBinaryOpExpression(expression);
			IR::BinaryOpExpression *binop_templ = IR::ToBinaryOpExpression(templ);
			
			if (binop_expr->operation != binop_templ->operation)
				return false;
			IR::Expression **left_inst = NULL, **right_inst = NULL;
			if (template_instantiation != NULL) {
				IR::BinaryOpExpression *binop_inst = new IR::BinaryOpExpression(
					binop_expr->operation, NULL, NULL);
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
			IR::Expression **addr_inst = NULL;
			if (template_instantiation != NULL) {
				IR::MemoryExpression *mem_inst = new IR::MemoryExpression(NULL);
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

bool Assembler::MatchStatement(IR::Statement *statement, IR::Statement *templ,
	int &nodecount, std::list<TemplateChildInfo> *children,
	IR::Statement **template_instantiation)
{
	// getTemplatesList takes care of this
	assert(statement->kind == templ->kind);
	
	nodecount++;
	
	switch (statement->kind) {
		case IR::IR_LABEL:
			return true;
		case IR::IR_MOVE: {
			IR::MoveStatement *move_statm = IR::ToMoveStatement(statement);
			IR::MoveStatement *move_templ = IR::ToMoveStatement(templ);
			IR::Expression **to_inst = NULL, **from_inst = NULL;
			if (template_instantiation != NULL) {
				IR::MoveStatement *move_inst = new IR::MoveStatement(NULL, NULL);
				*template_instantiation = move_inst;
				to_inst = &move_inst->to;
				from_inst = &move_inst->from;
			}
			return MatchExpression(move_statm->to, move_templ->to, 
					nodecount, children, to_inst) &&
				MatchExpression(move_statm->from, move_templ->from, 
					nodecount, children, from_inst);
		}
		case IR::IR_JUMP: {
			IR::Expression **dest_inst = NULL;
			if (template_instantiation != NULL) {
				IR::JumpStatement *jump_inst = new IR::JumpStatement(NULL);
				*template_instantiation = jump_inst;
				dest_inst = &jump_inst->dest;
			}
			return MatchExpression(
				IR::ToJumpStatement(statement)->dest,
				IR::ToJumpStatement(templ)->dest,
				nodecount, children, dest_inst);
		}
		case IR::IR_COND_JUMP: {
			IR::CondJumpStatement *cj_statm = IR::ToCondJumpStatement(statement);
			IR::CondJumpStatement *cj_templ = IR::ToCondJumpStatement(templ);
			
			// getTemplatesList takes care of this
			assert(cj_statm->comparison == cj_templ->comparison);

			IR::Expression **left_inst = NULL, **right_inst = NULL;
			if (template_instantiation != NULL) {
				IR::CondJumpStatement *cj_inst = new IR::CondJumpStatement(
					cj_statm->comparison, NULL, NULL, cj_statm->true_dest,
					cj_statm->false_dest);
				*template_instantiation = cj_inst;
				left_inst = &cj_inst->left;
				right_inst = &cj_inst->right;
			}
			return MatchExpression(cj_statm->left, cj_templ->left, 
					nodecount, children) &&
				MatchExpression(cj_statm->right, cj_templ->right, 
					nodecount, children);
		}
		default:
			Error::fatalError("Strange statement template kind");
	}
}

void Assembler::FindExpressionTemplate(IR::Expression* expression,
	InstructionTemplate* &templ)
{
	std::list<InstructionTemplate> *templates = getTemplatesList(expression);
	int best_nodecount = -1;
	templ = NULL;
	for (std::list<InstructionTemplate>::iterator cur_templ = templates->begin();
			cur_templ  != templates->end(); cur_templ ++) {
		assert((*cur_templ ).code->kind == IR::CODE_EXPRESSION);
		IR::Expression *templ_exp = ((IR::ExpressionCode *) (*cur_templ ).code)->exp;
		int nodecount;
		std::list<IR::Expression *> current_children;
		if (MatchExpression(expression, templ_exp, nodecount)) {
			if (nodecount > best_nodecount) {
				best_nodecount = nodecount;
				templ = &(*cur_templ);
			}
		}
	}
}

void Assembler::FindStatementTemplate(IR::Statement* statement,
	InstructionTemplate* &templ)
{
	std::list<InstructionTemplate> *templates = getTemplatesList(statement);
	int best_nodecount = -1;
	templ = NULL;

	for (std::list<InstructionTemplate>::iterator cur_templ = templates->begin();
			cur_templ != templates->end(); cur_templ++) {
		assert((*cur_templ).code->kind == IR::CODE_STATEMENT);
		IR::Statement *templ_statm = ((IR::StatementCode *) (*cur_templ).code)->statm;
		int nodecount;
		std::list<IR::Expression *> current_children;
		if (MatchStatement(statement, templ_statm, nodecount)) {
			if (nodecount > best_nodecount) {
				best_nodecount = nodecount;
				templ = &(*cur_templ);
			}
		}
	}
}

void Assembler::translateExpression(IR::Expression* expression,
	IR::VirtualRegister* value_storage, Instructions& result)
{
	if (expression->kind == IR::IR_STAT_EXP_SEQ) {
		IR::StatExpSequence *statexp = IR::ToStatExpSequence(expression);
		translateStatement(statexp->stat, result);
		translateExpression(statexp->exp, value_storage, result);
	} else {
		InstructionTemplate *templ;
		FindExpressionTemplate(expression, templ);
		if (templ == NULL)
			Error::fatalError("Failed to find expression template");
		
		std::list<TemplateChildInfo> children;
		IR::Expression *template_instantiation;
		assert(templ->code->kind == IR::CODE_EXPRESSION);
		int nodecount;
		MatchExpression(expression, ((IR::ExpressionCode *)templ->code)->exp,
			nodecount, &children, &template_instantiation);
		for (std::list<TemplateChildInfo>::iterator child = children.begin();
				child != children.end(); child++)
			translateExpression((*child).expression, (*child).value_storage,
				result);
		translateExpressionTemplate(template_instantiation, value_storage,
			children, result);
		
		IR::DestroyExpression(template_instantiation);
	}
}

void Assembler::translateStatement(IR::Statement* statement, Instructions& result)
{
	if (statement->kind == IR::IR_STAT_SEQ) {
		IR::StatementSequence *seq = IR::ToStatementSequence(statement);
		for (std::list<IR::Statement *>::iterator statm = seq->statements.begin();
				statm != seq->statements.end(); statm++)
			 translateStatement(*statm, result);
	} else if (statement->kind == IR::IR_EXP_IGNORE_RESULT) {
		translateExpression(IR::ToExpressionStatement(statement)->exp, NULL, result);
	} else {
		InstructionTemplate *templ;
		FindStatementTemplate(statement, templ);
		if (templ == NULL)
			Error::fatalError("Failed to find statement template");
		
		std::list<TemplateChildInfo> children;
		IR::Statement *template_instantiation;
		assert(templ->code->kind == IR::CODE_STATEMENT);
		int nodecount;
		MatchStatement(statement, ((IR::StatementCode *)templ->code)->statm,
			nodecount, &children, &template_instantiation);
		for (std::list<TemplateChildInfo>::iterator child = children.begin();
				child != children.end(); child++)
			translateExpression((*child).expression, (*child).value_storage,
				result);
		translateStatementTemplate(template_instantiation, children, result);
		
		IR::DestroyStatement(template_instantiation);
	}
}


}