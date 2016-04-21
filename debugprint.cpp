#include "intermediate.h"
#include "syntaxtree.h"
#include "debugprint.h"
#include "frame.h"
#include <stdarg.h>
#include <map>

std::string IntToStr(int x)
{
	char buf[30];
	sprintf(buf, "%d", x);
	return std::string(buf);
}

#ifdef DEBUG
std::map<std::string, FILE*> open_files;
#endif

DebugPrinter::DebugPrinter(const char* filename)
{
#ifdef DEBUG
	if (open_files.find(std::string(filename)) == open_files.end()) {
		debug_output = fopen(filename, "w");
		open_files[std::string(filename)] = debug_output;
	} else
		debug_output = open_files[std::string(filename)];
#endif
}

DebugPrinter::~DebugPrinter()
{
}

void DebugPrinter::debug(const char *msg, ...)
{
#ifdef DEBUG
	va_list ap;
	va_start(ap, msg);
	vfprintf(debug_output, msg, ap);
	fputc('\n', debug_output);
	fflush(debug_output);
	va_end(ap);
#endif
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

void PrintExpression(FILE *out, Expression exp, int indent, const char *prefix = "")
{
	preprint(out, indent, prefix);
	if (! exp) {
		printf("NULL\n");
		return;
	}
	switch (exp->kind) {
	case IR_INTEGER:
		fprintf(out, "%ld\n", ToIntegerExpression(exp)->getSigned());
		break;
	case IR_LABELADDR:
		if (ToLabelAddressExpression(exp)->label == NULL)
			fprintf(out, "Label address (NULL)\n");
		else
			fprintf(out, "Label address %s\n",
				ToLabelAddressExpression(exp)->label->getName().c_str());
		break;
	case IR_REGISTER:
		if (ToRegisterExpression(exp)->reg == NULL)
			fprintf(out, "Register (NULL)\n");
		else
			if (ToRegisterExpression(exp)->reg->getName() == "")
				fprintf(out, "Register %d\n", ToRegisterExpression(exp)->reg->getIndex());
			else
				fprintf(out, "Register %s\n", ToRegisterExpression(exp)->reg->getName().c_str());
		break;
	case IR_VAR_LOCATION:
		fprintf(out, "Variable %s\n", ToVarLocationExp(exp)->variable->getName().c_str());
		break;
	case IR_BINARYOP: 
		fprintf(out, "%s\n", BINARYOPNAMES[ToBinaryOpExpression(exp)->operation]);
		PrintExpression(out, ToBinaryOpExpression(exp)->left, indent+4, "Left: ");
		PrintExpression(out, ToBinaryOpExpression(exp)->right, indent+4, "Right: ");
		break;
	case IR_MEMORY:
		fprintf(out, "Memory value\n");
		PrintExpression(out, ToMemoryExpression(exp)->address, indent+4, "Address: ");
		break;
	case IR_FUN_CALL: 
		fprintf(out, "Function call\n");
		if (ToCallExpression(exp)->callee_parentfp != NULL)
			PrintExpression(out, ToCallExpression(exp)->callee_parentfp, indent+4, "Parent FP parameter: ");
		PrintExpression(out, ToCallExpression(exp)->function, indent+4, "Call address: ");
		for (Expression arg: ToCallExpression(exp)->arguments)
			 PrintExpression(out, arg, indent+4, "Argument: ");
		break;
	case IR_STAT_EXP_SEQ: 
		fprintf(out, "Statement and expression\n");
		PrintStatement(out, ToStatExpSequence(exp)->stat, indent+4, "Pre-statement: ");
		PrintExpression(out, ToStatExpSequence(exp)->exp, indent+4, "Value: ");
		break;
	default:
		fprintf(out, "WTF\n");
	}
}

void PrintStatement(FILE *out, Statement statm, int indent, const char *prefix)
{
	preprint(out, indent, prefix);
	if (! statm) {
		printf("NULL\n");
		return;
	}
	switch (statm->kind) {
	case IR_MOVE:
		fprintf(out, "Move (assign)\n");
		PrintExpression(out, ToMoveStatement(statm)->to, indent+4, "To: ");
		PrintExpression(out, ToMoveStatement(statm)->from, indent+4, "From: ");
		break;
	case IR_EXP_IGNORE_RESULT:
		fprintf(out, "Expression, ignore result\n");
		PrintExpression(out, ToExpressionStatement(statm)->exp, indent+4, "To: ");
		break;
	case IR_JUMP:
		fprintf(out, "Unconditional jump\n");
		PrintExpression(out, ToJumpStatement(statm)->dest, indent+4, "Address: ");
		break;
	case IR_COND_JUMP:
		fprintf(out, "Conditional jump\n");
		preprint(out, indent+4, "Comparison: ");
		fprintf(out, "%s\n", COMPARNAMES[ToCondJumpStatement(statm)->comparison]);
		PrintExpression(out, ToCondJumpStatement(statm)->left, indent+4, "Left operand: ");
		PrintExpression(out, ToCondJumpStatement(statm)->right, indent+4, "Right operand: ");
		preprint(out, indent+4, "Label if true: ");
		if (ToCondJumpStatement(statm)->true_dest == NULL)
			fprintf(out, "UNKNOWN\n");
		else
			fprintf(out, "%s\n", ToCondJumpStatement(statm)->true_dest->getName().c_str());
		preprint(out, indent+4, "Label if false: ");
		if (ToCondJumpStatement(statm)->false_dest == NULL)
			fprintf(out, "UNKNOWN\n");
		else
			fprintf(out, "%s\n", ToCondJumpStatement(statm)->false_dest->getName().c_str());
		break;
	case IR_STAT_SEQ:
		fprintf(out, "Sequence\n");
		for (Statement p: ToStatementSequence(statm)->statements)
			 PrintStatement(out, p, indent+4);
		break;
	case IR_LABEL:
		if (ToLabelPlacementStatement(statm)->label == NULL)
			fprintf(out, "Label placement\n");
		else
			fprintf(out, "Label here: %s\n", ToLabelPlacementStatement(statm)->label->getName().c_str());
		break;
	default:
		fprintf(out, "WTF\n");
	}
}

void PrintCode(FILE *out, Code code, int indent)
{
	switch (code->kind) {
		case CODE_EXPRESSION:
			PrintExpression(out, std::static_pointer_cast<ExpressionCode>(code)->exp, indent+4);
			break;
		case CODE_STATEMENT:
		case CODE_JUMP_WITH_PATCHES:
			PrintStatement(out, std::static_pointer_cast<StatementCode>(code)->statm, indent+4);
			break;
	}
}

void IREnvironment::printBlobs(FILE *out)
{
	for (Blob &blob: blobs) {
		fprintf(out, "Label here: %s\n", blob.label->getName().c_str());
		if (blob.data.size() == 0)
			fprintf(out, "(0 bytes)");
		else {
			fputc(' ', out);
			for (unsigned char c: blob.data) {
				fprintf(out, "0x%.2x", c);
			}
		}
		fprintf(out, " \"");
		for (unsigned char c: blob.data)
			fprintf(out, "%c", c);
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
		preprint(out, indent, prefix, tree->linenumber.linenumber);
	switch (tree->type) {
		case Syntax::NIL:
			fprintf(out, "nil\n");
			break;
		case Syntax::BREAK:
			fprintf(out, "break\n");
			break;
		case Syntax::INTVALUE:
			fprintf(out, "%d\n", std::static_pointer_cast<Syntax::IntValue>(tree)->value);
			break;
		case Syntax::IDENTIFIER:
			fprintf(out, "%s\n", std::static_pointer_cast<Syntax::Identifier>(tree)->name.c_str());
			break;
		case Syntax::STRINGVALUE:
			fprintf(out, "\"%s\"\n", std::static_pointer_cast<Syntax::StringValue>(tree)->value.c_str());
			break;
		case Syntax::BINARYOP:
			fprintf(out, "%s\n", TokenName(std::static_pointer_cast<Syntax::BinaryOp>(tree)->operation));
			print(out, std::static_pointer_cast<Syntax::BinaryOp>(tree)->left, indent+4, "Left: ");
			print(out, std::static_pointer_cast<Syntax::BinaryOp>(tree)->right, indent+4, "Right: ");
			break;
		case Syntax::EXPRESSIONLIST: {
			std::shared_ptr<Syntax::ExpressionList> exp = std::static_pointer_cast<Syntax::ExpressionList>(tree);
			for (Tree i: exp->expressions)
				print(out, i, indent);
			break;
		}
		case Syntax::SEQUENCE:
			fprintf(out, "Sequence\n");
			print(out, std::static_pointer_cast<Syntax::Sequence>(tree)->content, indent+4, "Element: ");
			break;
		case Syntax::ARRAYINDEXING:
			fprintf(out, "Array index\n");
			print(out, std::static_pointer_cast<Syntax::ArrayIndexing>(tree)->array, indent+4, "Array: ");
			print(out, std::static_pointer_cast<Syntax::ArrayIndexing>(tree)->index, indent+4, "Index: ");
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
