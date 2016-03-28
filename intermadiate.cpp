#include "intermediate.h"
#include "errormsg.h"
#include <stdio.h>

namespace IR {

Label *LabelFactory::addLabel()
{
	labels.push_back(Label(labels.size()));
	return &(labels.back());
}

Label *LabelFactory::addLabel(const std::string &name)
{
	labels.push_back(Label(labels.size(), name));
	return &(labels.back());
}

Register *RegisterFactory::addRegister()
{
	registers.push_back(Register(registers.size()));
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
			break;
		case CODE_JUMP_WITH_PATCHES: {
			Label *true_label = addLabel();
			Label *false_label = addLabel();
			Label *finish_label = addLabel();
			Register *value = addRegister();
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
		}
	}
	delete code;
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
	delete code;
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
			replace_true.push_back(&((CondJumpStatement *)result)->true_dest);
			replace_false.push_back(&((CondJumpStatement *)result)->false_dest);
			break;
		case CODE_JUMP_WITH_PATCHES:
			result = ((CondJumpPatchesCode *)code)->statm;
			replace_true = ((CondJumpPatchesCode *)code)->replace_true;
			replace_false = ((CondJumpPatchesCode *)code)->replace_false;
			break;
	}
	delete code;
	code = NULL;
	return result;
}

void preprint(int indent, const char *prefix = "")
{
	for (int i = 0; i < indent; i++)
		fputc(' ', stdout);
	printf("%s", prefix);
}

const char *BINARYOPNAMES[] = {
	"OP_PLUS",
	"OP_MINUS",
	"OP_MUL",
	"OP_DIV",
	"OP_AND",
	"OP_OR",
	"OP_XOR",
	"OP_SHL",
	"OP_SHR",
	"OP_SHAR",
};

const char *COMPARNAMES[] = {
	"OP_EQUAL",
	"OP_NONEQUAL",
	"OP_LESS",
	"OP_LESSEQUAL",
	"OP_GREATER",
	"OP_GREATEQUAL",
	"OP_ULESS",
	"OP_ULESSEQUAL",
	"OP_UGREATER",
	"OP_UGREATEQUAL"
};

void PrintStatement(Statement *statm, int indent, const char *prefix = "");

void PrintExpression(Expression *exp, int indent, const char *prefix = "")
{
	preprint(indent, prefix);
	switch (exp->kind) {
	case IR_INTEGER:
		printf("%d\n", ((IntegerExpression *)exp)->value);
		break;
	case IR_LABELADDR:
		printf("Label address %s\n",
			((LabelAddressExpression *)exp)->label->getName().c_str());
		break;
	case IR_REGISTER:
		printf("Register %d\n", ((RegisterExpression *)exp)->reg->getIndex());
		break;
	case IR_BINARYOP: 
		printf("%s\n", BINARYOPNAMES[((BinaryOpExpression *)exp)->operation]);
		PrintExpression(((BinaryOpExpression *)exp)->left, indent+4, "Left: ");
		PrintExpression(((BinaryOpExpression *)exp)->right, indent+4, "Right: ");
		break;
	case IR_MEMORY:
		printf("Memory value\n");
		PrintExpression(((MemoryExpression *)exp)->address, indent+4, "Address: ");
		break;
	case IR_FUN_CALL: 
		printf("Function call\n");
		PrintExpression(((CallExpression *)exp)->function, indent+4, "Call address: ");
		for (std::list<Expression *>::iterator arg = ((CallExpression *)exp)->arguments.begin(); 
				arg != ((CallExpression *)exp)->arguments.end(); arg++)
			 PrintExpression(*arg, indent+4, "Argument: ");
		break;
	case IR_STAT_EXP_SEQ: 
		printf("Statement and expression\n");
		PrintStatement(((StatExpSequence *)exp)->stat, indent+4, "Pre-statement: ");
		PrintExpression(((StatExpSequence *)exp)->exp, indent+4, "Value: ");
		break;
	default:
		printf("WTF\n");
	}
}

void PrintStatement(Statement *statm, int indent, const char *prefix)
{
	preprint(indent, prefix);
	switch (statm->kind) {
	case IR_MOVE:
		printf("Move (assign)\n");
		PrintExpression(((MoveStatement *)statm)->to, indent+4, "To: ");
		PrintExpression(((MoveStatement *)statm)->from, indent+4, "From: ");
		break;
	case IR_EXP_IGNORE_RESULT:
		printf("Expression, ignore result\n");
		PrintExpression(((ExpressionStatement *)statm)->exp, indent+4, "To: ");
		break;
	case IR_JUMP:
		printf("Unconditional jump\n");
		PrintExpression(((JumpStatement *)statm)->dest, indent+4, "Address: ");
		break;
	case IR_COND_JUMP:
		printf("Conditional jump\n");
		preprint(indent+4, "Comparison: ");
		printf("%s\n", COMPARNAMES[((CondJumpStatement*)statm)->comparison]);
		PrintExpression(((CondJumpStatement*)statm)->left, indent+4, "Left operand: ");
		PrintExpression(((CondJumpStatement*)statm)->right, indent+4, "Right operand: ");
		preprint(indent+4, "Label if true: ");
		if (((CondJumpStatement*)statm)->true_dest == NULL)
			printf("UNKNOWN\n");
		else
			printf("%s\n", ((CondJumpStatement*)statm)->true_dest->getName().c_str());
		preprint(indent+4, "Label if false: ");
		if (((CondJumpStatement*)statm)->false_dest == NULL)
			printf("UNKNOWN\n");
		else
			printf("%s\n", ((CondJumpStatement*)statm)->false_dest->getName().c_str());
		break;
	case IR_STAT_SEQ:
		printf("Sequence\n");
		for (std::list<Statement *>::iterator p = ((StatementSequence*)statm)->statements.begin(); 
				p != ((StatementSequence*)statm)->statements.end(); p++)
			 PrintStatement(*p, indent+4);
		break;
	case IR_LABEL:
		printf("Label here: %s\n", ((LabelPlacementStatement *)statm)->label->getName().c_str());
		break;
	default:
		printf("WTF\n");
	}
}

void PrintCode(Code *code, int indent)
{
	switch (code->kind) {
		case CODE_EXPRESSION:
			PrintExpression(((ExpressionCode *)code)->exp, indent);
			break;
		case CODE_STATEMENT:
		case CODE_JUMP_WITH_PATCHES:
			PrintStatement(((StatementCode *)code)->statm, indent);
			break;
	}
}

void IREnvironment::printBlobs()
{
	for (std::list<Blob>::iterator blob = blobs.begin();
			blob != blobs.end(); blob++) {
		printf("Label here: %s\n", (*blob).label->getName().c_str());
		if ((*blob).data.size() == 0)
			printf("(0 bytes)");
		else {
			for (int i = 0; i < (*blob).data.size(); i++) {
				if (i != 0)
					fputc(' ', stdout);
				printf("0x%.2x", (*blob).data[i]);
			}
		}
		printf(" \"");
		for (int i = 0; i < (*blob).data.size(); i++)
			printf("%c", (*blob).data[i]);
		printf("\"\n");
	}
}

};
