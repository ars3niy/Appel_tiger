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
#include <idmap.h>

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
public:
	NodeType type;
	int linenumber;
	Node(NodeType _type) : type(_type), linenumber(Error::getLineNumber()) {}
};

class IntValue: public Node {
public:
	int value;
	
	IntValue(int _value) : Node(INTVALUE), value(_value) {}
};

class Identifier: public Node {
public:
	std::string name;
	
	Identifier(const char *_name) : Node(IDENTIFIER), name(_name) {}
};

class StringValue: public Node {
public:
	std::string value;
	
	StringValue(const std::string &_value = "") : Node(STRINGVALUE), value(_value) {}
};

class BinaryOp: public Node {
public:
	yytokentype operation;
	Tree left, right;
	
	BinaryOp(yytokentype _operation, Tree _left, Tree _right) :
		Node(BINARYOP), operation(_operation), left(_left), right(_right)
	{
		switch (_operation) {
			case SYM_PLUS:
			case SYM_MINUS:
			case SYM_ASTERISK:
			case SYM_SLASH:
			case SYM_EQUAL:
			case SYM_NONEQUAL:
			case SYM_LESS:
			case SYM_LESSEQUAL:
			case SYM_GREATER:
			case SYM_GREATEQUAL:
			case SYM_ASSIGN:
				return;
			default:
				Error::fatalError("Unexpected operation token", left->linenumber);
		}
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
};

class Sequence: public Node {
public:
	ExpressionList *content;
	
	Sequence(Tree _content) : Node(SEQUENCE), content((ExpressionList *)_content)
	{
		assert(_content->type == EXPRESSIONLIST);
	}
};

class ArrayIndexing: public Node {
public:
	Tree array, index;
	
	ArrayIndexing(Tree _array, Tree _index) :
		Node(ARRAYINDEXING), array(_array), index(_index) {}
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
};

class If: public Node {
public:
	Tree condition, action;
	
	If(Tree _condition, Tree _action) :
		Node(IF), condition(_condition), action(_action) {}
};

class IfElse: public Node {
public:
	Tree condition, action, elseaction;
	
	IfElse(Tree _condition, Tree _action, Tree _else) :
		Node(IFELSE), condition(_condition), action(_action), elseaction(_else) {}
};

class While: public Node {
public:
	Tree condition, action;
	
	While(Tree _condition, Tree _action) :
		Node(WHILE), condition(_condition), action(_action) {}
};

class For: public Node {
public:
	Identifier *variable;
	Tree start, stop, action;
	/**
	 * Id for the loop variable
	 */
	ObjectId variable_id;
	
	For(Tree _variable, Tree _start, Tree _stop, Tree _action, ObjectId _var_id) :
		Node(FOR), variable((Identifier *)_variable),
		start(_start), stop(_stop), action(_action), variable_id(_var_id)
	{
		assert(variable->type == IDENTIFIER);
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
};

class ArrayTypeDefinition: public Node {
public:
	Identifier *elementtype;
	
	ArrayTypeDefinition(Tree _type) : Node(ARRAYTYPEDEFINITION),
		elementtype((Identifier *)_type)
	{
		assert(_type->type == IDENTIFIER);
	}
};

class RecordTypeDefinition: public Node {
public:
	ExpressionList *fields;
	
	RecordTypeDefinition(Tree _fields) : Node(RECORDTYPEDEFINITION),
		fields((ExpressionList *)_fields)
	{
		assert(_fields->type == EXPRESSIONLIST);
	}
};

class ParameterDeclaration: public Node {
public:
	Identifier *name, *type;
	/**
	 * Unique within the program tree for all ParameterDeclarations,
	 * VariableDeclarations, For's and FunctionDeclaration
	 * Used to share information about the varible between multiple passes
	 * of the syntax tree
	 */
	ObjectId id;
	
	ParameterDeclaration(int _id, Tree _name, Tree _type) :
		Node(PARAMETERDECLARATION), id(_id),
		name((Identifier *)_name), type((Identifier *)_type)
	{
		assert(_name->type == IDENTIFIER);
		assert(_type->type == IDENTIFIER);
	}
};

class VariableDeclaration: public Node {
public:
	Identifier *name, *type;
	Tree value;
	/**
	 * Unique within the program tree for all ParameterDeclarations,
	 * VariableDeclarations, For's and FunctionDeclaration
	 * Used to share information about the varible between multiple passes
	 * of the syntax tree
	 */
	ObjectId id;
	
	VariableDeclaration(int _id, Tree _name, Tree _value, Tree _type = NULL) :
		Node(VARDECLARATION), id(_id), name((Identifier *)_name),
		value(_value), type((Identifier *)_type)
	{
		assert(_name->type == IDENTIFIER);
		assert((_type == NULL) || (_type->type == IDENTIFIER));
	}
};

class Function: public Node {
public:
	Identifier *name, *type;
	ExpressionList *parameters;
	Tree body;
	/**
	 * Unique within the program tree for all ParameterDeclarations,
	 * VariableDeclarations, For's and FunctionDeclaration
	 * Used to share information about the varible between multiple passes
	 * of the syntax tree
	 */
	ObjectId id;
	
	Function(int _id, Tree _name, Tree _type, Tree _parameters, Tree _body) :
		id(_id), Node(FUNCTION), name((Identifier *)_name),
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
};

/**
 * Recursively delete the entire tree
 */
void DestroySyntaxTree(Tree &tree);

}

#endif
