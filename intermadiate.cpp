#include "intermediate.h"
#include "errormsg.h"
#include <stdio.h>

namespace IR {

void FlipComparison(CondJumpStatement *jump)
{
	Label *tmp = jump->true_dest;
	jump->true_dest = jump->false_dest;
	jump->false_dest = tmp;
	switch (jump->comparison) {
		case OP_EQUAL:
			jump->comparison = OP_NONEQUAL;
			break;
		case OP_NONEQUAL:
			jump->comparison = OP_EQUAL;
			break;
		case OP_LESS:
			jump->comparison = OP_GREATEQUAL;
			break;
		case OP_GREATER:
			jump->comparison = OP_LESSEQUAL;
			break;
		case OP_LESSEQUAL:
			jump->comparison = OP_GREATER;
			break;
		case OP_GREATEQUAL:
			jump->comparison = OP_LESS;
			break;
		case OP_ULESS:
			jump->comparison = OP_UGREATEQUAL;
			break;
		case OP_UGREATER:
			jump->comparison = OP_ULESSEQUAL;
			break;
		case OP_ULESSEQUAL:
			jump->comparison = OP_UGREATER;
			break;
		case OP_UGREATEQUAL:
			jump->comparison = OP_ULESS;
			break;
		default: ;
	}
}

Label *LabelFactory::addLabel()
{
	labels.push_back(Label(labels.size()));
	return &(labels.back());
}

Label::Label(int _index) : index(_index)
{
	char s[32];
	sprintf(s, ".L%d", _index);
	name = s;
}

VirtualRegister::VirtualRegister(int _index) : index(_index)
{
	char s[32];
	sprintf(s, "t%d", _index);
	name = s;
	prespilled_location = NULL;
}


Label *LabelFactory::addLabel(const std::string &name)
{
	labels.push_back(Label(labels.size(), name));
	return &(labels.back());
}


VirtualRegister *MapRegister(const RegisterMap *register_map,
	VirtualRegister *reg)
{
	if ((register_map != NULL) && (register_map->size() > (unsigned)reg->getIndex()) &&
		((*register_map)[reg->getIndex()] != NULL))
		return (*register_map)[reg->getIndex()];
	else
		return reg;
}

VirtualRegister *RegisterFactory::addRegister()
{
	debug("Adding new unnamed register #%d", registers.size());
	registers.push_back(VirtualRegister(registers.size()));
	return &(registers.back());
}

VirtualRegister *RegisterFactory::addRegister(const std::string &name)
{
	debug("Adding new register %s as #%d", name.c_str(), registers.size());
	registers.push_back(VirtualRegister(registers.size(), name));
	return &(registers.back());
}

Blob *IREnvironment::addBlob()
{
	blobs.push_back(Blob());
	Blob *result = &(blobs.back());
	result->label = addLabel();
	return result;
}

void putLabels(const std::list<Label**> &replace_true, 
	const std::list<Label**> &replace_false, Label *truelabel, Label *falselabel)
{
	for (Label **plabel: replace_true)
		 *plabel = truelabel;
	for (Label **plabel: replace_false)
		 *plabel = falselabel;
}

Expression IREnvironment::codeToExpression(Code code)
{
	switch (code->kind) {
		case CODE_STATEMENT:
			Error::fatalError("Not supposed to convert statements to expressions");
		case CODE_EXPRESSION:
			return std::static_pointer_cast<ExpressionCode>(code)->exp;
		case CODE_JUMP_WITH_PATCHES: {
			Label *true_label = addLabel();
			Label *false_label = addLabel();
			Label *finish_label = addLabel();
			VirtualRegister *value = addRegister();
			putLabels(std::static_pointer_cast<CondJumpPatchesCode>(code)->replace_true,
				std::static_pointer_cast<CondJumpPatchesCode>(code)->replace_false,
				true_label, false_label);
			
			std::shared_ptr<StatementSequence> sequence = std::make_shared<StatementSequence>();
			sequence->addStatement(std::static_pointer_cast<CondJumpPatchesCode>(code)->statm);
			sequence->addStatement(std::make_shared<LabelPlacementStatement>(true_label));
			sequence->addStatement(std::make_shared<MoveStatement>(
				std::make_shared<RegisterExpression>(value),
				std::make_shared<IntegerExpression>(1)));
			sequence->addStatement(std::make_shared<JumpStatement>(
				std::make_shared<LabelAddressExpression>(finish_label), finish_label));
			sequence->addStatement(std::make_shared<LabelPlacementStatement>(false_label));
			sequence->addStatement(std::make_shared<MoveStatement>(
				std::make_shared<RegisterExpression>(value),
				std::make_shared<IntegerExpression>(0)));
			sequence->addStatement(std::make_shared<LabelPlacementStatement>(finish_label));
			
			return std::make_shared<StatExpSequence>(sequence,
				std::make_shared<RegisterExpression>(value));
		}
	}
	return nullptr;
}

Statement IREnvironment::codeToStatement(Code code)
{
	switch (code->kind) {
		case CODE_STATEMENT:
			return std::static_pointer_cast<StatementCode>(code)->statm;
		case CODE_EXPRESSION:
			return std::make_shared<ExpressionStatement>(
				std::static_pointer_cast<ExpressionCode>(code)->exp);
		case CODE_JUMP_WITH_PATCHES: {
			Label *proceed = addLabel();
			putLabels(std::static_pointer_cast<CondJumpPatchesCode>(code)->replace_true,
				std::static_pointer_cast<CondJumpPatchesCode>(code)->replace_false,
				proceed, proceed);
			std::shared_ptr<StatementSequence> seq = std::make_shared<StatementSequence>();
			seq->addStatement(std::static_pointer_cast<CondJumpPatchesCode>(code)->statm);
			seq->addStatement(std::make_shared<LabelPlacementStatement>(proceed));
			return seq;
		}
	}
	return nullptr;
}

Statement IREnvironment::codeToCondJump(Code code,
	std::list<Label**> &replace_true,
	std::list<Label**> &replace_false)
{
	switch (code->kind) {
		case CODE_STATEMENT:
			Error::fatalError("Not supposed to convert statements to conditional jumps");
			break;
		case CODE_EXPRESSION: {
			Expression bool_exp = std::static_pointer_cast<ExpressionCode>(code)->exp;
			Statement result;
			replace_true.clear();
			replace_false.clear();
			if (bool_exp->kind == IR_INTEGER) {
				std::shared_ptr<LabelAddressExpression> dest =
					std::make_shared<IR::LabelAddressExpression>(nullptr);
				result = std::make_shared<JumpStatement>(dest, dest->label);
				if (ToIntegerExpression(bool_exp)->value) {
					replace_true.push_back(& dest->label);
					replace_true.push_back(& ToJumpStatement(result)->possible_results.front());
				} else {
					replace_false.push_back(& dest->label);
					replace_false.push_back(& ToJumpStatement(result)->possible_results.front());
				}
			} else {
				result = std::make_shared<CondJumpStatement>(OP_NONEQUAL,
					bool_exp, std::make_shared<IntegerExpression>(0),
					nullptr, nullptr);
				replace_true.push_back(& ToCondJumpStatement(result)->true_dest);
				replace_false.push_back(& ToCondJumpStatement(result)->false_dest);
			}
			return result;
		}
		case CODE_JUMP_WITH_PATCHES:
			std::shared_ptr<CondJumpStatement> result =
				ToCondJumpStatement(std::static_pointer_cast<CondJumpPatchesCode>(code)->statm);
			replace_true = std::static_pointer_cast<CondJumpPatchesCode>(code)->replace_true;
			replace_false = std::static_pointer_cast<CondJumpPatchesCode>(code)->replace_false;
			return result;
	}
	return nullptr;
}

void walkExpression(Expression &exp, Expression parentExpression,
	Statement parentStatement, const CodeWalkCallbacks &callbacks, CodeWalker *arg)
{
	switch (exp->kind) {
	case IR_REGISTER:
		if (callbacks.doLabelAddr)
			callbacks.doLabelAddr(exp, parentExpression, parentStatement, arg);
		break;
	case IR_LABELADDR:
		if (callbacks.doInt)
			callbacks.doInt(exp, parentExpression, parentStatement, arg);
		break;
	case IR_BINARYOP: {
		auto binop = ToBinaryOpExpression(exp);
		walkExpression(binop->left, exp, nullptr, callbacks, arg);
		walkExpression(binop->right, exp, nullptr, callbacks, arg);
		if (callbacks.doBinOp)
			callbacks.doBinOp(exp, parentExpression, parentStatement, arg);
		break;
	}
	case IR_MEMORY:
		walkExpression(ToMemoryExpression(exp)->address, exp, nullptr, callbacks, arg);
		if (callbacks.doMemory)
			callbacks.doMemory(exp, parentExpression, parentStatement, arg);
		break;
	case IR_VAR_LOCATION:
		walkExpression(ToMemoryExpression(exp)->address, exp, nullptr, callbacks, arg);
		if (callbacks.doVarLocation)
			callbacks.doVarLocation(exp, parentExpression, parentStatement, arg);
		break;
	case IR_FUN_CALL: {
		auto call = ToCallExpression(exp);
		walkExpression(call->function, exp, nullptr, callbacks, arg);
		for (Expression &fun_arg: call->arguments)
			walkExpression(fun_arg, exp, nullptr, callbacks, arg);
		if (callbacks.doCall)
			callbacks.doCall(exp, parentExpression, parentStatement, arg);
		break;
	}
	case IR_STAT_EXP_SEQ: {
		auto seq = ToStatExpSequence(exp);
		walkStatement(seq->stat, callbacks, arg);
		walkExpression(seq->exp, exp, nullptr, callbacks, arg);
		if (callbacks.doStatExpSeq)
			callbacks.doStatExpSeq(exp, parentExpression, parentStatement, arg);
		break;
	}
	default: ;
	}
}

void walkStatement(Statement &statm, const CodeWalkCallbacks &callbacks, CodeWalker *arg)
{
	switch (statm->kind) {
	case IR_MOVE: {
		auto move = ToMoveStatement(statm);
		walkExpression(move->from, nullptr, statm, callbacks, arg);
		walkExpression(move->to, nullptr, statm, callbacks, arg);
		if (callbacks.doMove)
			callbacks.doMove(statm, arg);
		break;
	}
	case IR_EXP_IGNORE_RESULT: {
		auto expstatm = ToExpressionStatement(statm);
		walkExpression(expstatm->exp, nullptr, statm, callbacks, arg);
		if (callbacks.doExpIgnoreRes)
			callbacks.doExpIgnoreRes(statm, arg);
		break;
	}
	case IR_JUMP:
		walkExpression(ToJumpStatement(statm)->dest, nullptr, statm, callbacks, arg);
		if (callbacks.doJump)
			callbacks.doJump(statm, arg);
		break;
	case IR_COND_JUMP: {
		auto condjump = ToCondJumpStatement(statm);
		walkExpression(condjump->left, nullptr, statm, callbacks, arg);
		walkExpression(condjump->right, nullptr, statm, callbacks, arg);
		if (callbacks.doCondJump)
			callbacks.doCondJump(statm, arg);
		break;
	}
	case IR_STAT_SEQ:
		for (Statement element: ToStatementSequence(statm)->statements)
			walkStatement(element, callbacks, arg);
		if (callbacks.doSequence)
			callbacks.doSequence(statm, arg);
		break;
	default: ;
	}
}

void walkCode(Code code, const CodeWalkCallbacks &callbacks, CodeWalker *arg)
{
	if (code->kind == IR::CODE_EXPRESSION) {
		std::shared_ptr<IR::ExpressionCode> exp_code =
			std::static_pointer_cast<IR::ExpressionCode>(code);
		walkExpression(exp_code->exp, nullptr, nullptr, callbacks, arg);
	} else {
		std::shared_ptr<IR::StatementCode> statm_code =
			std::static_pointer_cast<IR::StatementCode>(code);
		walkStatement(statm_code->statm, callbacks, arg);
	}
}

}
