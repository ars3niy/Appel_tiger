#ifndef _DECLARATIONS_H
#define _DECLARATIONS_H

#include "layeredmap.h"
#include "syntaxtree.h"
#include "idmap.h"
#include <map>
#include <list>

namespace Semantic {
	
class TranslatedExpression;

enum BaseType {
	TYPE_ERROR,
	TYPE_VOID,
	TYPE_NIL,
	TYPE_INT,
	TYPE_STRING,
	TYPE_ARRAY,
	TYPE_RECORD,
	TYPE_NAMEREFERENCE
};

class Type {
public:
	BaseType basetype;
	bool is_loop_variable;
	
	Type(BaseType _basetype) : basetype(_basetype), is_loop_variable(false) {}
	Type *resolve();
};

class ForwardReferenceType: public Type {
public:
	std::string name;
	Type *meaning;
	Syntax::Tree reference_position;
	
	ForwardReferenceType(const std::string _name, Syntax::Tree _position) :
		Type(TYPE_NAMEREFERENCE), name(_name), meaning(NULL),
		reference_position(_position) {}
};

class ArrayType: public Type {
public:
	/**
	 * Unique within the TypesEnvironment for all arrays and records
	 * Used for checking if two Type * are the same type
	 */
	ObjectId id;
	
	Type *elemtype;
	
	ArrayType(ObjectId _id, Type *_elemtype) :
		Type(TYPE_ARRAY), id(_id), elemtype(_elemtype) {}
};

class RecordField {
public:
	std::string name;
	Type *type;
	
	RecordField(const std::string &_name, Type *_type) : name(_name), type(_type) {}
};

class RecordType: public Type {
public:
	/**
	 * Unique within the TypesEnvironment for all arrays and records
	 * Used for checking if two Type * are the same type
	 */
	ObjectId id;
	
	typedef std::list<RecordField> FieldsList;
	typedef std::map<std::string, RecordField*> FieldsMap;
	FieldsMap fields;
	FieldsList field_list;
	
	RecordType(ObjectId _id) : Type(TYPE_RECORD), id(_id) {}
	void addField(const std::string &name, Type *type)
	{
		field_list.push_back(RecordField(name, type));
		fields.insert(std::make_pair(name, &(field_list.back())));
	}
};

enum DeclarationType {
	DECL_VARIABLE,
	DECL_LOOP_VARIABLE,
	DECL_ARGUMENT,
	DECL_FUNCTION
};

class Declaration {
public:
	DeclarationType kind;
	
	Declaration(DeclarationType _kind) : kind(_kind) {}
};

class Variable: public Declaration {
public:
	std::string name;
	Type *type;
	TranslatedExpression *value;
	
	Variable(const std::string &_name, Type *_type, TranslatedExpression *_value) :
		Declaration(DECL_VARIABLE), name(_name), type(_type), value(_value) {}
};

class Function;

class FunctionArgument: public Variable {
public:
	Function *function;
	
	FunctionArgument(Function *owner, const std::string &_name, Type *_type) :
		Variable(_name, _type, NULL), function(owner)
	{
		kind = DECL_ARGUMENT;
	}
};

class LoopVariable: public Variable {
public:
	LoopVariable(const std::string &_name, Type *_type) :
		Variable(_name, _type, NULL)
	{
		kind = DECL_LOOP_VARIABLE;
	}
};

class Function: public Declaration {
public:
	std::string name;
	Type *return_type;
	std::list<FunctionArgument> arguments;
	Syntax::Tree raw_body;
	TranslatedExpression *body;

	Function(const std::string &_name, Type *_return_type,
		Syntax::Tree _raw, TranslatedExpression *_body) :
		Declaration(DECL_FUNCTION), return_type(_return_type), 
		raw_body(_raw), body(_body) {}
	FunctionArgument *addArgument(const std::string &name, Type *type)
	{
		arguments.push_back(FunctionArgument(this, name, type));
		return &(arguments.back());
	}
};

}

#endif
