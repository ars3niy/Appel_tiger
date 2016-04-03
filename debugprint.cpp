#include "intermediate.h"
#include "syntaxtree.h"
#include "debugprint.h"
#include <stdarg.h>

DebugPrinter::DebugPrinter(const char* filename)
{
	f = fopen(filename, "a");
}

DebugPrinter::~DebugPrinter()
{
	fclose(f);
}

void DebugPrinter::debug(const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	vfprintf(f, msg, ap);
	fputc('\n', f);
	va_end(ap);
}

namespace IR {

void preprint(FILE *out, int indent, const char *prefix = "")
{
	for (int i = 0; i < indent; i++)
		fputc(' ', out);
	fprintf(out, "%s", prefix);
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

void PrintExpression(FILE *out, Expression *exp, int indent, const char *prefix = "")
{
	preprint(out, indent, prefix);
	switch (exp->kind) {
	case IR_INTEGER:
		fprintf(out, "%d\n", ((IntegerExpression *)exp)->value);
		break;
	case IR_LABELADDR:
		fprintf(out, "Label address %s\n",
			((LabelAddressExpression *)exp)->label->getName().c_str());
		break;
	case IR_REGISTER:
		if (((RegisterExpression *)exp)->reg->getName() == "")
			fprintf(out, "Register %d\n", ((RegisterExpression *)exp)->reg->getIndex());
		else
			fprintf(out, "Register %s\n", ((RegisterExpression *)exp)->reg->getName().c_str());
		break;
	case IR_BINARYOP: 
		fprintf(out, "%s\n", BINARYOPNAMES[((BinaryOpExpression *)exp)->operation]);
		PrintExpression(out, ((BinaryOpExpression *)exp)->left, indent+4, "Left: ");
		PrintExpression(out, ((BinaryOpExpression *)exp)->right, indent+4, "Right: ");
		break;
	case IR_MEMORY:
		fprintf(out, "Memory value\n");
		PrintExpression(out, ((MemoryExpression *)exp)->address, indent+4, "Address: ");
		break;
	case IR_FUN_CALL: 
		fprintf(out, "Function call\n");
		PrintExpression(out, ((CallExpression *)exp)->function, indent+4, "Call address: ");
		for (std::list<Expression *>::iterator arg = ((CallExpression *)exp)->arguments.begin(); 
				arg != ((CallExpression *)exp)->arguments.end(); arg++)
			 PrintExpression(out, *arg, indent+4, "Argument: ");
		break;
	case IR_STAT_EXP_SEQ: 
		fprintf(out, "Statement and expression\n");
		PrintStatement(out, ((StatExpSequence *)exp)->stat, indent+4, "Pre-statement: ");
		PrintExpression(out, ((StatExpSequence *)exp)->exp, indent+4, "Value: ");
		break;
	default:
		fprintf(out, "WTF\n");
	}
}

void PrintStatement(FILE *out, Statement *statm, int indent, const char *prefix)
{
	preprint(out, indent, prefix);
	switch (statm->kind) {
	case IR_MOVE:
		fprintf(out, "Move (assign)\n");
		PrintExpression(out, ((MoveStatement *)statm)->to, indent+4, "To: ");
		PrintExpression(out, ((MoveStatement *)statm)->from, indent+4, "From: ");
		break;
	case IR_EXP_IGNORE_RESULT:
		fprintf(out, "Expression, ignore result\n");
		PrintExpression(out, ((ExpressionStatement *)statm)->exp, indent+4, "To: ");
		break;
	case IR_JUMP:
		fprintf(out, "Unconditional jump\n");
		PrintExpression(out, ((JumpStatement *)statm)->dest, indent+4, "Address: ");
		break;
	case IR_COND_JUMP:
		fprintf(out, "Conditional jump\n");
		preprint(out, indent+4, "Comparison: ");
		fprintf(out, "%s\n", COMPARNAMES[((CondJumpStatement*)statm)->comparison]);
		PrintExpression(out, ((CondJumpStatement*)statm)->left, indent+4, "Left operand: ");
		PrintExpression(out, ((CondJumpStatement*)statm)->right, indent+4, "Right operand: ");
		preprint(out, indent+4, "Label if true: ");
		if (((CondJumpStatement*)statm)->true_dest == NULL)
			fprintf(out, "UNKNOWN\n");
		else
			fprintf(out, "%s\n", ((CondJumpStatement*)statm)->true_dest->getName().c_str());
		preprint(out, indent+4, "Label if false: ");
		if (((CondJumpStatement*)statm)->false_dest == NULL)
			fprintf(out, "UNKNOWN\n");
		else
			fprintf(out, "%s\n", ((CondJumpStatement*)statm)->false_dest->getName().c_str());
		break;
	case IR_STAT_SEQ:
		fprintf(out, "Sequence\n");
		for (std::list<Statement *>::iterator p = ((StatementSequence*)statm)->statements.begin(); 
				p != ((StatementSequence*)statm)->statements.end(); p++)
			 PrintStatement(out, *p, indent+4);
		break;
	case IR_LABEL:
		fprintf(out, "Label here: %s\n", ((LabelPlacementStatement *)statm)->label->getName().c_str());
		break;
	default:
		fprintf(out, "WTF\n");
	}
}

void PrintCode(FILE *out, Code *code, int indent)
{
	switch (code->kind) {
		case CODE_EXPRESSION:
			PrintExpression(out, ((ExpressionCode *)code)->exp, indent+4);
			break;
		case CODE_STATEMENT:
		case CODE_JUMP_WITH_PATCHES:
			PrintStatement(out, ((StatementCode *)code)->statm, indent+4);
			break;
	}
}

void IREnvironment::printBlobs(FILE *out)
{
	for (std::list<Blob>::iterator blob = blobs.begin();
			blob != blobs.end(); blob++) {
		fprintf(out, "Label here: %s\n", (*blob).label->getName().c_str());
		if ((*blob).data.size() == 0)
			fprintf(out, "(0 bytes)");
		else {
			for (int i = 0; i < (*blob).data.size(); i++) {
				if (i != 0)
					fputc(' ', out);
				fprintf(out, "0x%.2x", (*blob).data[i]);
			}
		}
		fprintf(out, " \"");
		for (int i = 0; i < (*blob).data.size(); i++)
			fprintf(out, "%c", (*blob).data[i]);
		fprintf(out, "\"\n");
	}
}

}

namespace Syntax {

const char *TokenName(yytokentype token) {
	switch (token) {
		case SYM_EQUAL: return "equal";
		case SYM_PLUS: return "plus";
		case SYM_MINUS: return "minus"; 
		case SYM_ASTERISK: return "times";
		case SYM_SLASH: return "divide";
		case SYM_LESS: return "less";
		case SYM_GREATER: return "greater";
		case SYM_LESSEQUAL: return "less or equal";
		case SYM_GREATEQUAL: return "greater or equal";
		case SYM_NONEQUAL: return "nonequal";
		case SYM_AND: return "and";
		case SYM_OR: return "or";
		case SYM_ASSIGN: return "assign";
		case UNARYMINUS: return "minus";
		default: return "WTF";
	}
}

void preprint(FILE *out, int indent, const char *prefix = "", int linenumber = -1)
{
	for (int i = 0; i < indent; i++) fputc(' ', stdout);
	if (linenumber > 0)
		fprintf(out, "[%d] ", linenumber);
	fprintf(out, "%s", prefix);
}

void print(FILE *out, Syntax::Tree tree, int indent = 0, const char *prefix = "")
{
	if (tree->type != Syntax::EXPRESSIONLIST)
		preprint(out, indent, prefix, tree->linenumber);
	switch (tree->type) {
		case Syntax::NIL:
			fprintf(out, "nil\n");
			break;
		case Syntax::BREAK:
			fprintf(out, "break\n");
			break;
		case Syntax::INTVALUE:
			fprintf(out, "%d\n", ((Syntax::IntValue *)tree)->value);
			break;
		case Syntax::IDENTIFIER:
			fprintf(out, "%s\n", ((Syntax::Identifier *)tree)->name.c_str());
			break;
		case Syntax::STRINGVALUE:
			fprintf(out, "\"%s\"\n", ((Syntax::StringValue *)tree)->value.c_str());
			break;
		case Syntax::BINARYOP:
			fprintf(out, "%s\n", TokenName(((Syntax::BinaryOp *)tree)->operation));
			print(out, ((Syntax::BinaryOp *)tree)->left, indent+4, "Left: ");
			print(out, ((Syntax::BinaryOp *)tree)->right, indent+4, "Right: ");
			break;
		case Syntax::EXPRESSIONLIST: {
			Syntax::ExpressionList *exp = (Syntax::ExpressionList *)tree;
			for (std::list<Syntax::Tree>::iterator i = exp->expressions.begin();
					i != exp->expressions.end(); i++)
				print(out, *i, indent);
			break;
		}
		case Syntax::SEQUENCE:
			fprintf(out, "Sequence\n");
			print(out, ((Syntax::Sequence *)tree)->content, indent+4, "Element: ");
			break;
		case Syntax::ARRAYINDEXING:
			fprintf(out, "Array index\n");
			print(out, ((Syntax::ArrayIndexing *)tree)->array, indent+4, "Array: ");
			print(out, ((Syntax::ArrayIndexing *)tree)->index, indent+4, "Index: ");
			break;
#if 0
	virtual void ArrayInstantiation::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		fprintf(out, "Array value\n");
		arraydef->print(indent+4);
		value->print(indent+4, "Value: ");
	}
	virtual void If::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		fprintf(out, "If\n");
		condition->print(indent+4, "Condition: ");
		action->print(indent+4, "Action: ");
	}
	virtual void IfElse::(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		fprintf(out, "If-Else\n");
		condition->print(indent+4, "Condition: ");
		action->print(indent+4, "Action: ");
		elseaction->print(indent+4, "Else: ");
	}
	virtual void While::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		fprintf(out, "While\n");
		condition->print(indent+4, "Condition: ");
		action->print(indent+4, "Action: ");
	}
	virtual void For::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		fprintf(out, "For\n");
		variable->print(indent+4, "Variable: ");
		start->print(indent+4, "From: ");
		stop->print(indent+4, "To: ");
		action->print(indent+4, "Action: ");
	}
	virtual void Scope::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		fprintf(out, "Let\n");
		declarations->print(indent+4);
		preprint(indent+4, "In\n");
		action->print(indent+8);
	}
	virtual void RecordField::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		fprintf(out, "Record field access\n");
		record->print(indent+4, "Record: ");
		field->print(indent+4, "Field: ");
	}
	virtual void FuctioCall::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		fprintf(out, "Function call\n");
		function->print(indent+4, "Name: ");
		preprint(indent+4);
		fprintf(out, "Arguments\n");
		arguments->print(indent+8);
	}
	virtual void RecordInstantiation::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		fprintf(out, "Record value\n");
		type->print(indent+4, "Type: ");
		preprint(indent+4);
		fprintf(out, "Fields\n");
		fieldvalues->print(indent+8);
	}
	virtual void TypeDeclaration::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		fprintf(out, "Type declaration\n");
		name->print(indent+4, "New type name: ");
		definition->print(indent+4, "Type: ");
	}
	virtual void ArrayTypeDefinition::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		fprintf(out, "Array type\n");
		elementtype->print(indent+4, "Element type: ");
	}
	virtual void RecordTypeDefinition::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		fprintf(out, "Record type\n");
		preprint(indent+4);
		fprintf(out, "Fields\n");
		fields->print(indent+8);
	}
	virtual void ParameterDeclaration::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		fprintf(out, "%s : %s\n", name->name.c_str(), type->name.c_str());
	}
	virtual void VarieblDeclaration::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		fprintf(out, "Variable declaration\n");
		name->print(indent+4, "Name: ");
		if (type == NULL) {
			preprint(indent+4, "Type: ");
			fprintf(out, "Automatic\n");
		} else
			type->print(indent+4, "Type: ");
		value->print(indent+4, "Value: ");
	}
	virtual void Function::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		fprintf(out, "Function\n");
		name->print(indent+4, "Name: ");
		if (type == NULL) {
			preprint(indent+4);
			fprintf(out, "No return value\n");
		} else
			type->print(indent+4, "Type: ");
		preprint(indent+4);
		fprintf(out, "Parameters\n");
		parameters->print(indent+8);
		body->print(indent+4, "Body: ");
	}
#endif
		default:
			fprintf(out, "WTF\n");
	}
}

}
