#ifndef _TYPES_H
#define _TYPES_H

#include "syntaxtree.h"

#include <map>
#include <list>
#include <string>

namespace Semantic {

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
	int offset;
	
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
	int data_size;
	
	RecordType(ObjectId _id) : Type(TYPE_RECORD), id(_id), data_size(0) {}
	RecordField *addField(const std::string &name, Type *type)
	{
		field_list.push_back(RecordField(name, type));
		fields.insert(std::make_pair(name, &(field_list.back())));
		return &(field_list.back());
	}
};

}

#endif