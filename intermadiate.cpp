#include "intermediate.h"
#include "errormsg.h"
#include "frame.h"
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

MoveStatement::MoveStatement(Expression _to, Expression _from)  :
	StatementClass(IR_MOVE), to(_to), from(_from)
{
	if (to->kind == IR_VAR_LOCATION)
		ToVarLocationExp(to)->variable->assign();
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

bool walkStatement(Statement &statm, const CodeWalkCallbacks &callbacks, CodeWalker *arg);

bool walkExpression(Expression &exp, Expression parentExpression,
	Statement parentStatement, const CodeWalkCallbacks &callbacks, CodeWalker *arg)
{
	switch (exp->kind) {
	case IR_INTEGER:
		if (callbacks.doInt)
			return callbacks.doInt(exp, parentExpression, parentStatement, arg);
		else
			return true;
	case IR_LABELADDR:
		if (callbacks.doLabelAddr)
			return callbacks.doLabelAddr(exp, parentExpression, parentStatement, arg);
		else
			return true;
	case IR_REGISTER:
		if (callbacks.doRegister)
			return callbacks.doRegister(exp, parentExpression, parentStatement, arg);
		else
			return true;
	case IR_BINARYOP: {
		auto binop = ToBinaryOpExpression(exp);
		if (! walkExpression(binop->left, exp, nullptr, callbacks, arg))
			return false;
		if (! walkExpression(binop->right, exp, nullptr, callbacks, arg))
			return false;
		if (callbacks.doBinOp)
			return callbacks.doBinOp(exp, parentExpression, parentStatement, arg);
		else
			return true;
	}
	case IR_MEMORY:
		if (! walkExpression(ToMemoryExpression(exp)->address, exp,
				nullptr, callbacks, arg))
			return false;
		if (callbacks.doMemory)
			return callbacks.doMemory(exp, parentExpression, parentStatement, arg);
		else
			return true;
	case IR_VAR_LOCATION:
		if (callbacks.doVarLocation)
			return callbacks.doVarLocation(exp, parentExpression, parentStatement, arg);
		else
			return true;
	case IR_FUN_CALL: {
		auto call = ToCallExpression(exp);
		if (! walkExpression(call->function, exp, nullptr, callbacks, arg))
			return false;
		for (Expression &fun_arg: call->arguments)
			if (! walkExpression(fun_arg, exp, nullptr, callbacks, arg))
				return false;
		if (callbacks.doCall)
			return callbacks.doCall(exp, parentExpression, parentStatement, arg);
		else
			return true;
	}
	case IR_STAT_EXP_SEQ: {
		auto seq = ToStatExpSequence(exp);
		if (! walkStatement(seq->stat, callbacks, arg))
			return false;
		if (! walkExpression(seq->exp, exp, nullptr, callbacks, arg))
			return false;
		if (callbacks.doStatExpSeq)
			return callbacks.doStatExpSeq(exp, parentExpression, parentStatement, arg);
		else
			return true;
	}
	default:
		return true;
	}
}

bool walkStatement(Statement &statm, const CodeWalkCallbacks &callbacks, CodeWalker *arg)
{
	switch (statm->kind) {
	case IR_MOVE: {
		auto move = ToMoveStatement(statm);
		if (! walkExpression(move->from, nullptr, statm, callbacks, arg))
			return false;
		if (! walkExpression(move->to, nullptr, statm, callbacks, arg))
			return false;
		if (callbacks.doMove)
			return callbacks.doMove(statm, arg);
		else
			return true;
	}
	case IR_EXP_IGNORE_RESULT: {
		auto expstatm = ToExpressionStatement(statm);
		if (! walkExpression(expstatm->exp, nullptr, statm, callbacks, arg))
			return false;
		if (callbacks.doExpIgnoreRes)
			return callbacks.doExpIgnoreRes(statm, arg);
		else
			return true;
	}
	case IR_JUMP:
		if (! walkExpression(ToJumpStatement(statm)->dest, nullptr, statm, callbacks, arg))
			return false;
		if (callbacks.doJump)
			return callbacks.doJump(statm, arg);
		else
			return true;
	case IR_COND_JUMP: {
		auto condjump = ToCondJumpStatement(statm);
		if (! walkExpression(condjump->left, nullptr, statm, callbacks, arg))
			return false;
		if (! walkExpression(condjump->right, nullptr, statm, callbacks, arg))
			return false;
		if (callbacks.doCondJump)
			return callbacks.doCondJump(statm, arg);
		else
			return true;
	}
	case IR_STAT_SEQ:
		for (Statement element: ToStatementSequence(statm)->statements)
			if (! walkStatement(element, callbacks, arg))
				return false;
		if (callbacks.doSequence)
			return callbacks.doSequence(statm, arg);
		else
			return true;
	default:
		return true;
	}
}

bool walkCode(Code code, const CodeWalkCallbacks &callbacks, CodeWalker *arg)
{
	if (code->kind == CODE_EXPRESSION) {
		std::shared_ptr<ExpressionCode> exp_code =
			std::static_pointer_cast<ExpressionCode>(code);
		return walkExpression(exp_code->exp, nullptr, nullptr, callbacks, arg);
	} else {
		std::shared_ptr<StatementCode> statm_code =
			std::static_pointer_cast<StatementCode>(code);
		return walkStatement(statm_code->statm, callbacks, arg);
	}
}

Label *IREnvironment::copyLabel(Label *label)
{
	if (label == NULL)
		return NULL;
	
	auto found = label_copies.find(label->getIndex());
	if (found != label_copies.end())
		return found->second;
	
	Label *newlabel = addLabel(label->getName() + "._" + IntToStr(copy_count));
	label_copies.insert(std::make_pair(label->getIndex(), newlabel));
	return newlabel;
}

Expression IREnvironment::CopyExpression(Expression exp)
{
	if (! exp)
		return nullptr;
	switch (exp->kind) {
	case IR_INTEGER:
		return std::make_shared<IntegerExpression>(
			std::static_pointer_cast<IntegerExpression>(exp)->value);
	case IR_LABELADDR:
		return std::make_shared<LabelAddressExpression>(copyLabel(
			std::static_pointer_cast<LabelAddressExpression>(exp)->label));
	case IR_REGISTER:
		return std::make_shared<RegisterExpression>(
			std::static_pointer_cast<RegisterExpression>(exp)->reg);
	case IR_BINARYOP: {
		auto binop = ToBinaryOpExpression(exp);
		return std::make_shared<BinaryOpExpression>(
			binop->operation, CopyExpression(binop->left), CopyExpression(binop->right));
	}
	case IR_MEMORY:
		return std::make_shared<MemoryExpression>(ToMemoryExpression(exp)->address);
	case IR_VAR_LOCATION:
		return std::make_shared<VarLocationExp>(ToVarLocationExp(exp)->variable);
	case IR_FUN_CALL: {
		auto call = ToCallExpression(exp);
		auto copy = std::make_shared<CallExpression>(CopyExpression(call->function),
			CopyExpression(call->callee_parentfp));
		for (Expression fun_arg: call->arguments)
			copy->addArgument(CopyExpression(fun_arg));
	}
	case IR_STAT_EXP_SEQ: {
		auto seq = ToStatExpSequence(exp);
		return std::make_shared<StatExpSequence>(CopyStatement(seq->stat),
			CopyExpression(seq->exp));
	}
	default:
		return nullptr;
	}
}

Statement IREnvironment::CopyStatement(Statement statm)
{
	switch (statm->kind) {
	case IR_MOVE: {
		auto move = ToMoveStatement(statm);
		return std::make_shared<MoveStatement>(CopyExpression(move->to),
			CopyExpression(move->from));
	}
	case IR_EXP_IGNORE_RESULT:
		return std::make_shared<ExpressionStatement>(CopyExpression(
			std::static_pointer_cast<ExpressionStatement>(statm)->exp));
	case IR_JUMP: {
		auto jump = ToJumpStatement(statm);
		auto copy = std::make_shared<JumpStatement>(jump->dest);
		for (Label *label: jump->possible_results)
			copy->addPossibleResult(copyLabel(label));
		return copy;
	}
	case IR_COND_JUMP: {
		auto condjump = ToCondJumpStatement(statm);
		return std::make_shared<CondJumpStatement>(condjump->comparison,
			condjump->left, condjump->right, condjump->true_dest, condjump->false_dest);
	}
	case IR_LABEL:
		return std::make_shared<LabelPlacementStatement>(
			copyLabel(ToLabelPlacementStatement(statm)->label));
	case IR_STAT_SEQ: {
		auto seq = ToStatementSequence(statm);
		auto copy = std::make_shared<StatementSequence>();
		for (Statement substatm: seq->statements)
			copy->addStatement(CopyStatement(substatm));
		return copy;
	}
	default:
		return nullptr;
	}
}

Code IREnvironment::CopyCode(Code code)
{
	copy_count++;
	Code result = nullptr;
	switch (code->kind) {
		case CODE_EXPRESSION:
			result = std::make_shared<ExpressionCode>(
				CopyExpression(std::static_pointer_cast<ExpressionCode>(code)->exp));
			break;
		case CODE_STATEMENT:
			result = std::make_shared<StatementCode>(
				CopyStatement(std::static_pointer_cast<StatementCode>(code)->statm));
			break;
		case CODE_JUMP_WITH_PATCHES:
			Error::fatalError("Won't copy jump with patches");
	}
	label_copies.clear();
	return result;
}

}
