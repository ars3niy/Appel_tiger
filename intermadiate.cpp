#include "intermediate.h"
#include "errormsg.h"
#include <stdio.h>

namespace IR {

void DestroyExpression(Expression *&expression)
{
	switch (expression->kind) {
		case IR_INTEGER:
			delete ToIntegerExpression(expression);
			break;
		case IR_LABELADDR:
			delete ToLabelAddressExpression(expression);
			break;
		case IR_REGISTER:
			delete ToRegisterExpression(expression);
			break;
		case IR_BINARYOP:
			DestroyExpression(ToBinaryOpExpression(expression)->left);
			DestroyExpression(ToBinaryOpExpression(expression)->right);
			delete ToBinaryOpExpression(expression);
			break;
		case IR_MEMORY:
			DestroyExpression(ToMemoryExpression(expression)->address);
			delete ToMemoryExpression(expression);
			break;
		case IR_FUN_CALL:
			DestroyExpression(ToCallExpression(expression)->function);
			delete ToCallExpression(expression);
			break;			
		case IR_STAT_EXP_SEQ: {
			DestroyStatement(ToStatExpSequence(expression)->stat);
			DestroyExpression(ToStatExpSequence(expression)->exp);
			delete ToStatExpSequence(expression);
			break;
		}
	}
	expression = NULL;
}

void DestroyStatement(Statement *&statement)
{
	switch (statement->kind) {
		case IR_MOVE:
			DestroyExpression(ToMoveStatement(statement)->to);
			DestroyExpression(ToMoveStatement(statement)->from);
			delete ToMoveStatement(statement);
			break;
		case IR_EXP_IGNORE_RESULT:
			DestroyExpression(ToExpressionStatement(statement)->exp);
			delete ToExpressionStatement(statement);
			break;
		case IR_JUMP:
			delete ToJumpStatement(statement);
			break;
		case IR_COND_JUMP:
			DestroyExpression(ToCondJumpStatement(statement)->left);
			DestroyExpression(ToCondJumpStatement(statement)->right);
			delete ToCondJumpStatement(statement);
			break;
		case IR_STAT_SEQ: {
			StatementSequence *seq = ToStatementSequence(statement);
			for (std::list<Statement *>::iterator s = seq->statements.begin();
					s != seq->statements.end(); s++)
				DestroyStatement(*s);
			delete seq;
			break;
		}
		case IR_LABEL:
			delete ToLabelPlacementStatement(statement);
			break;
	}
	statement = NULL;
}

void DestroyCode(Code *&code)
{
	switch (code->kind) {
		case CODE_EXPRESSION:
			DestroyExpression(((ExpressionCode *)code)->exp);
			delete ((ExpressionCode *)code);
			break;
		case CODE_STATEMENT:
			DestroyStatement(((StatementCode *)code)->statm);
			delete ((ExpressionCode *)code);
			break;
		case CODE_JUMP_WITH_PATCHES:
			DestroyStatement(((CondJumpPatchesCode *)code)->statm);
			delete ((CondJumpPatchesCode *)code);
			break;
	}
	code = NULL;
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
}


Label *LabelFactory::addLabel(const std::string &name)
{
	labels.push_back(Label(labels.size(), name));
	return &(labels.back());
}

VirtualRegister *RegisterFactory::addRegister()
{
	registers.push_back(VirtualRegister(registers.size()));
	return &(registers.back());
}

VirtualRegister *RegisterFactory::addRegister(const std::string &name)
{
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
	for (std::list<Label**>::const_iterator plabel = replace_true.begin();
			plabel != replace_true.end(); plabel++)
		 *(*plabel) = truelabel;
	for (std::list<Label**>::const_iterator plabel = replace_false.begin();
			plabel != replace_false.end(); plabel++)
		 *(*plabel) = falselabel;
}

Expression *IREnvironment::killCodeToExpression(Code *&code)
{
	IR::Expression *result = NULL;
	switch (code->kind) {
		case CODE_STATEMENT:
			Error::fatalError("Not supposed to convert statements to expressions");
			break;
		case CODE_EXPRESSION:
			result = ((ExpressionCode *)code)->exp;
			delete (ExpressionCode *)code;
			break;
		case CODE_JUMP_WITH_PATCHES: {
			Label *true_label = addLabel();
			Label *false_label = addLabel();
			Label *finish_label = addLabel();
			VirtualRegister *value = addRegister();
			putLabels(((CondJumpPatchesCode *)code)->replace_true,
				((CondJumpPatchesCode *)code)->replace_false,
				true_label, false_label);
			
			StatementSequence *sequence = new StatementSequence;
			sequence->addStatement(((CondJumpPatchesCode *)code)->statm);
			sequence->addStatement(new LabelPlacementStatement(true_label));
			sequence->addStatement(new MoveStatement(new RegisterExpression(value),
				new IntegerExpression(1)));
			sequence->addStatement(new JumpStatement(
				new LabelAddressExpression(finish_label), finish_label));
			sequence->addStatement(new LabelPlacementStatement(false_label));
			sequence->addStatement(new MoveStatement(new RegisterExpression(value),
				new IntegerExpression(0)));
			sequence->addStatement(new LabelPlacementStatement(finish_label));
			
			result = new StatExpSequence(sequence, new RegisterExpression(value));
			delete (CondJumpPatchesCode *)code;
		}
	}
	code = NULL;
	return result;
}

Statement *IREnvironment::killCodeToStatement(Code *&code)
{
	IR::Statement *result = NULL;
	switch (code->kind) {
		case CODE_STATEMENT:
			result = ((StatementCode *)code)->statm;
			break;
		case CODE_EXPRESSION:
			result = new ExpressionStatement(((ExpressionCode *)code)->exp);
			break;
		case CODE_JUMP_WITH_PATCHES: {
			Label *proceed = addLabel();
			putLabels(((CondJumpPatchesCode *)code)->replace_true,
				((CondJumpPatchesCode *)code)->replace_false,
				proceed, proceed);
			StatementSequence *seq = new StatementSequence;
			seq->addStatement(((CondJumpPatchesCode *)code)->statm);
			seq->addStatement(new LabelPlacementStatement(proceed));
			result = seq;
			break;
		}
	}
	code = NULL;
	return result;
}

Statement *IREnvironment::killCodeToCondJump(Code *&code,
	std::list<Label**> &replace_true,
	std::list<Label**> &replace_false)
{
	Statement *result = NULL;
	switch (code->kind) {
		case CODE_STATEMENT:
			Error::fatalError("Not supposed to convert statements to conditional jumps");
			break;
		case CODE_EXPRESSION:
			result = new CondJumpStatement(OP_NONEQUAL,
				((ExpressionCode *)code)->exp, new IntegerExpression(0),
				NULL, NULL);
			replace_true.clear();
			replace_false.clear();
			replace_true.push_back(& ToCondJumpStatement(result)->true_dest);
			replace_false.push_back(& ToCondJumpStatement(result)->false_dest);
			delete (ExpressionCode *)code;
			break;
		case CODE_JUMP_WITH_PATCHES:
			result = ((CondJumpPatchesCode *)code)->statm;
			replace_true = ((CondJumpPatchesCode *)code)->replace_true;
			replace_false = ((CondJumpPatchesCode *)code)->replace_false;
			delete (CondJumpPatchesCode *)code;
			break;
	}
	code = NULL;
	return result;
}

}
