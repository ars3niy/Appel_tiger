#ifndef _SYNTAXTREE_H
#define _SYNTAXTREE_H

#define YYSTYPE_IS_DECLARED
namespace Syntax {
	class Node;
	typedef Node *Tree;
}
typedef Syntax::Tree YYSTYPE;
#include <parser.hpp>

#include <errormsg.h>

#include <string>
#include <list>
#include <assert.h>
#include <stdio.h>


namespace Syntax {

enum NodeType {
	INTVALUE,
	STRINGVALUE,
	IDENTIFIER,
	NIL,
	BINARYOP,
	EXPRESSIONLIST,
	SEQUENCE,
	ARRAYINDEXING,
	ARRAYINSTANTIATION,
	IF,
	IFELSE,
	WHILE,
	FOR,
	BREAK,
	SCOPE,
	RECORDFIELD,
	FUNCTIONCALL,
	RECORDINSTANTIATION,
	TYPEDECLARATION,
	ARRAYTYPEDEFINITION,
	RECORDTYPEDEFINITION,
	PARAMETERDECLARATION,
	VARDECLARATION,
	FUNCTION
};

class Node {
protected:
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
public:
	NodeType type;
	int linenumber;
	Node(NodeType _type) : type(_type), linenumber(Error::getLineNumber()) {}
	
	void preprint(int indent, const char *prefix = "")
	{
		for (int i = 0; i < indent; i++) fputc(' ', stdout);
		printf("[%d] %s", linenumber, prefix);
	}
	
	virtual void print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		switch (type) {
			case NIL:
				printf("nil\n");
				break;
			case BREAK:
				printf("break\n");
				break;
			default:
				printf("WTF\n");
		}
	}
};

class IntValue: public Node {
public:
	int value;
	
	IntValue(int _value) : Node(INTVALUE), value(_value) {}
	virtual void print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("%d\n", value);
	}
};

class Identifier: public Node {
public:
	std::string name;
	
	Identifier(const char *_name) : Node(IDENTIFIER), name(_name) {}
	virtual void print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("%s\n", name.c_str());
	}
};

class StringValue: public Node {
public:
	std::string value;
	
	StringValue(const std::string &_value = "") : Node(STRINGVALUE), value(_value) {}
	virtual void print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("\"%s\"\n", value.c_str());
	}
};

class BinaryOp: public Node {
public:
	yytokentype operation;
	Tree left, right;
	
	BinaryOp(yytokentype _operation, Tree _left, Tree _right) :
		Node(BINARYOP), operation(_operation), left(_left), right(_right) {}
	virtual void print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("%s\n", TokenName(operation));
		left->print(indent+4, "Left: ");
		right->print(indent+4, "Right: ");
	}
};

class ExpressionList: public Node {
public:
	std::list<Tree> expressions;
	
	ExpressionList() : Node(EXPRESSIONLIST) {}
	void prepend(Tree expression)
	{
		expressions.push_front(expression);
	}
	virtual void print(int indent = 0, const char *prefix = "")
	{
		for (std::list<Tree>::iterator i = expressions.begin();
				 i != expressions.end(); i++)
			(*i)->print(indent);
	}
};

class Sequence: public Node {
public:
	ExpressionList *content;
	
	Sequence(Tree _content) : Node(SEQUENCE), content((ExpressionList *)_content)
	{
		assert(_content->type == EXPRESSIONLIST);
	}
	virtual void print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("Sequence\n");
		content->print(indent+4, "Element: ");
	}
};

class ArrayIndexing: public Node {
public:
	Tree array, index;
	
	ArrayIndexing(Tree _array, Tree _index) :
		Node(ARRAYINDEXING), array(_array), index(_index) {}
	virtual void print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("Array index\n");
		array->print(indent+4, "Array: ");
		index->print(indent+4, "Index: ");
	}
};

class ArrayInstantiation: public Node {
public:
	ArrayIndexing *arraydef;
	Tree value;
	
	ArrayInstantiation(Tree _def, Tree _value) :
		Node(ARRAYINSTANTIATION), arraydef((ArrayIndexing *)_def),
		value(_value)
	{
		assert(_def->type == ARRAYINDEXING);
	}
	
	bool arrayIsSingleId() {return arraydef->array->type == IDENTIFIER;} 
	virtual void print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("Array value\n");
		arraydef->print(indent+4);
		value->print(indent+4, "Value: ");
	}
};

class If: public Node {
public:
	Tree condition, action;
	
	If(Tree _condition, Tree _action) :
		Node(IF), condition(_condition), action(_action) {}
	virtual void print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("If\n");
		condition->print(indent+4, "Condition: ");
		action->print(indent+4, "Action: ");
	}
};

class IfElse: public Node {
public:
	Tree condition, action, elseaction;
	
	IfElse(Tree _condition, Tree _action, Tree _else) :
		Node(IFELSE), condition(_condition), action(_action), elseaction(_else) {}
	virtual void print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("If-Else\n");
		condition->print(indent+4, "Condition: ");
		action->print(indent+4, "Action: ");
		elseaction->print(indent+4, "Else: ");
	}
};

class While: public Node {
public:
	Tree condition, action;
	
	While(Tree _condition, Tree _action) :
		Node(WHILE), condition(_condition), action(_action) {}
	virtual void print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("While\n");
		condition->print(indent+4, "Condition: ");
		action->print(indent+4, "Action: ");
	}
};

class For: public Node {
public:
	Identifier *variable;
	Tree start, stop, action;
	
	For(Tree _variable, Tree _start, Tree _stop, Tree _action) :
		Node(FOR), variable((Identifier *)_variable),
		start(_start), stop(_stop), action(_action)
	{
		assert(variable->type == IDENTIFIER);
	}
	virtual void print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("For\n");
		variable->print(indent+4, "Variable: ");
		start->print(indent+4, "From: ");
		stop->print(indent+4, "To: ");
		action->print(indent+4, "Action: ");
	}
};

class Scope: public Node {
public:
	ExpressionList *declarations;
	ExpressionList *action;
	
	Scope(Tree _declarations, Tree _action) :
		Node(SCOPE),
		declarations((ExpressionList *)_declarations),
		action((ExpressionList *)_action)
	{
		assert(_declarations->type == EXPRESSIONLIST);
		assert(_action->type == EXPRESSIONLIST);
	}
	virtual void print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("Let\n");
		declarations->print(indent+4);
		preprint(indent+4, "In\n");
		action->print(indent+8);
	}
};

class RecordField: public Node {
public:
	Tree record;
	Identifier *field;
	
	RecordField(Tree _record, Tree _field) :
		Node(RECORDFIELD),
		record(_record),
		field((Identifier *)_field)
	{
		assert(_field->type == IDENTIFIER);
	}
	virtual void print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("Record field access\n");
		record->print(indent+4, "Record: ");
		field->print(indent+4, "Field: ");
	}
};

class FunctionCall: public Node {
public:
	Identifier *function;
	ExpressionList *arguments;
	
	FunctionCall(Tree _function, Tree _arguments) :
		Node(FUNCTIONCALL),
		function((Identifier *)_function),
		arguments((ExpressionList *)_arguments)
	{
		assert(_function->type == IDENTIFIER);
		assert(_arguments->type == EXPRESSIONLIST);
	}
	virtual void print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("Function call\n");
		function->print(indent+4, "Name: ");
		preprint(indent+4);
		printf("Arguments\n");
		arguments->print(indent+8);
	}
};

class RecordInstantiation: public Node {
public:
	Identifier *type;
	ExpressionList *fieldvalues;
	
	RecordInstantiation(Tree _type, Tree _values) :
		Node(RECORDINSTANTIATION),
		type((Identifier *)_type),
		fieldvalues((ExpressionList *)_values)
	{
		assert(_type->type == IDENTIFIER);
		assert(_values->type == EXPRESSIONLIST);
		for (std::list<Tree>::iterator i = fieldvalues->expressions.begin();
			 i != fieldvalues->expressions.end();
			 i++) {
			assert((*i)->type == BINARYOP);
			assert(((BinaryOp *)(*i))->operation == SYM_ASSIGN);
		}
	}
	virtual void print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("Record value\n");
		type->print(indent+4, "Type: ");
		preprint(indent+4);
		printf("Fields\n");
		fieldvalues->print(indent+8);
	}
};

class TypeDeclaration: public Node {
public:
	Identifier *name;
	Tree definition;
	
	TypeDeclaration(Tree _name, Tree _definition) :
		Node(TYPEDECLARATION), name((Identifier *)_name),
		definition(_definition)
	{
		assert(_name->type == IDENTIFIER);
		assert((_definition->type == IDENTIFIER) ||
		       (_definition->type == ARRAYTYPEDEFINITION) ||
		       (_definition->type == RECORDTYPEDEFINITION));
	}
	virtual void print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("Type declaration\n");
		name->print(indent+4, "New type name: ");
		definition->print(indent+4, "Type: ");
	}
};

class ArrayTypeDefinition: public Node {
public:
	Identifier *elementtype;
	
	ArrayTypeDefinition(Tree _type) : Node(ARRAYTYPEDEFINITION),
		elementtype((Identifier *)_type)
	{
		assert(_type->type == IDENTIFIER);
	}
	virtual void print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("Array type\n");
		elementtype->print(indent+4, "Element type: ");
	}
};

class RecordTypeDefinition: public Node {
public:
	ExpressionList *fields;
	
	RecordTypeDefinition(Tree _fields) : Node(RECORDTYPEDEFINITION),
		fields((ExpressionList *)_fields)
	{
		assert(_fields->type == EXPRESSIONLIST);
		for (std::list<Tree>::iterator i = fields->expressions.begin();
			 i != fields->expressions.end(); i++)
			assert((*i)->type == PARAMETERDECLARATION);
	}
	virtual void print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("Record type\n");
		preprint(indent+4);
		printf("Fields\n");
		fields->print(indent+8);
	}
};

class ParameterDeclaration: public Node {
public:
	Identifier *name, *type;
	
	ParameterDeclaration(Tree _name, Tree _type) :
		Node(PARAMETERDECLARATION),
		name((Identifier *)_name), type((Identifier *)_type)
	{
		assert(_name->type == IDENTIFIER);
		assert(_type->type == IDENTIFIER);
	}
	virtual void print(int indent = 0, const char *prefix = "")
	{
		preprint(indent, prefix);
		printf("%s : %s\n", name->name.c_str(), type->name.c_str());
	}
};

class VariableDeclaration: public Node {
public:
	Identifier *name, *type;
	Tree value;
	
	VariableDeclaration(Tree _name, Tree _value, Tree _type = NULL) :
		Node(VARDECLARATION), name((Identifier *)_name),
		value(_value), type((Identifier *)_type)
	{
		assert(_name->type == IDENTIFIER);
		assert((_type == NULL) || (_type->type == IDENTIFIER));
	}
	virtual void print(int indent = 0, const char *prefix = "")
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
};

class Function: public Node {
public:
	Identifier *name, *type;
	ExpressionList *parameters;
	Tree body;
	
	Function(Tree _name, Tree _type, Tree _parameters, Tree _body) :
		Node(FUNCTION), name((Identifier *)_name),
		type((Identifier *)_type),
		parameters((ExpressionList *)_parameters),
		body(_body)
	{
		assert(_name->type == IDENTIFIER);
		assert((_type == NULL) || (_type->type == IDENTIFIER));
		assert(_parameters->type == EXPRESSIONLIST);
		for (std::list<Tree>::iterator i = parameters->expressions.begin();
			 i != parameters->expressions.end(); i++)
			assert((*i)->type == PARAMETERDECLARATION);
	}
	virtual void print(int indent = 0, const char *prefix = "")
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
};

}

#endif
