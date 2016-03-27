#include <iostream>
#include <string>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <syntaxtree.h>
#include "translator.h"
#include "errormsg.h"

extern "C" {
extern FILE *yyin;
}

namespace PrintTree {

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

void preprint(int indent, const char *prefix = "", int linenumber = -1)
{
	for (int i = 0; i < indent; i++) fputc(' ', stdout);
	if (linenumber > 0)
		printf("[%d] ", linenumber);
	printf("%s", prefix);
}

void print(Syntax::Tree tree, int indent = 0, const char *prefix = "")
{
	if (tree->type != Syntax::EXPRESSIONLIST)
		preprint(indent, prefix, tree->linenumber);
	switch (tree->type) {
		case Syntax::NIL:
			printf("nil\n");
			break;
		case Syntax::BREAK:
			printf("break\n");
			break;
		case Syntax::INTVALUE:
			printf("%d\n", ((Syntax::IntValue *)tree)->value);
			break;
		case Syntax::IDENTIFIER:
			printf("%s\n", ((Syntax::Identifier *)tree)->name.c_str());
			break;
		case Syntax::STRINGVALUE:
			printf("\"%s\"\n", ((Syntax::StringValue *)tree)->value.c_str());
			break;
		case Syntax::BINARYOP:
			printf("%s\n", TokenName(((Syntax::BinaryOp *)tree)->operation));
			print(((Syntax::BinaryOp *)tree)->left, indent+4, "Left: ");
			print(((Syntax::BinaryOp *)tree)->right, indent+4, "Right: ");
			break;
		case Syntax::EXPRESSIONLIST: {
			Syntax::ExpressionList *exp = (Syntax::ExpressionList *)tree;
			for (std::list<Syntax::Tree>::iterator i = exp->expressions.begin();
					i != exp->expressions.end(); i++)
				print(*i, indent);
			break;
		}
		case Syntax::SEQUENCE:
			printf("Sequence\n");
			print(((Syntax::Sequence *)tree)->content, indent+4, "Element: ");
			break;
		case Syntax::ARRAYINDEXING:
			printf("Array index\n");
			print(((Syntax::ArrayIndexing *)tree)->array, indent+4, "Array: ");
			print(((Syntax::ArrayIndexing *)tree)->index, indent+4, "Index: ");
			break;
#if 0
	virtual void ArrayInstantiation::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("Array value\n");
		arraydef->print(indent+4);
		value->print(indent+4, "Value: ");
	}
	virtual void If::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("If\n");
		condition->print(indent+4, "Condition: ");
		action->print(indent+4, "Action: ");
	}
	virtual void IfElse::(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("If-Else\n");
		condition->print(indent+4, "Condition: ");
		action->print(indent+4, "Action: ");
		elseaction->print(indent+4, "Else: ");
	}
	virtual void While::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("While\n");
		condition->print(indent+4, "Condition: ");
		action->print(indent+4, "Action: ");
	}
	virtual void For::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("For\n");
		variable->print(indent+4, "Variable: ");
		start->print(indent+4, "From: ");
		stop->print(indent+4, "To: ");
		action->print(indent+4, "Action: ");
	}
	virtual void Scope::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("Let\n");
		declarations->print(indent+4);
		preprint(indent+4, "In\n");
		action->print(indent+8);
	}
	virtual void RecordField::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("Record field access\n");
		record->print(indent+4, "Record: ");
		field->print(indent+4, "Field: ");
	}
	virtual void FuctioCall::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("Function call\n");
		function->print(indent+4, "Name: ");
		preprint(indent+4);
		printf("Arguments\n");
		arguments->print(indent+8);
	}
	virtual void RecordInstantiation::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("Record value\n");
		type->print(indent+4, "Type: ");
		preprint(indent+4);
		printf("Fields\n");
		fieldvalues->print(indent+8);
	}
	virtual void TypeDeclaration::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("Type declaration\n");
		name->print(indent+4, "New type name: ");
		definition->print(indent+4, "Type: ");
	}
	virtual void ArrayTypeDefinition::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("Array type\n");
		elementtype->print(indent+4, "Element type: ");
	}
	virtual void RecordTypeDefinition::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("Record type\n");
		preprint(indent+4);
		printf("Fields\n");
		fields->print(indent+8);
	}
	virtual void ParameterDeclaration::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("%s : %s\n", name->name.c_str(), type->name.c_str());
	}
	virtual void VarieblDeclaration::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("Variable declaration\n");
		name->print(indent+4, "Name: ");
		if (type == NULL) {
			preprint(indent+4, "Type: ");
			printf("Automatic\n");
		} else
			type->print(indent+4, "Type: ");
		value->print(indent+4, "Value: ");
	}
	virtual void Function::print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("Function\n");
		name->print(indent+4, "Name: ");
		if (type == NULL) {
			preprint(indent+4);
			printf("No return value\n");
		} else
			type->print(indent+4, "Type: ");
		preprint(indent+4);
		printf("Parameters\n");
		parameters->print(indent+8);
		body->print(indent+4, "Body: ");
	}
#endif
		default:
			printf("WTF\n");
	}
}

}

void ProcessTree(Syntax::Tree tree)
{
	//PrintTree(tree);
	Semantic::DeclarationsEnvironment translator;
	Semantic::TranslatedExpression *translated;
	Semantic::Type *type;
	translator.translateExpression(tree, translated, type);
}

int main(int argc, char **argv) {
	if (argc != 2) {
		printf("Usage: compiler <input-file>\n");
		return 1;
	}
	yyin = fopen(argv[1], "r");
	if (yyin == NULL) {
		printf("Cannot open %s: %s\n", argv[1], strerror(errno));
		return 1;
	}
	
	yyparse();
	
	fclose(yyin);
	if (Error::getErrorCount() == 0)
		return 0;
	else
		return 1;
}
