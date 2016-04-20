#ifndef _DECLARATIONS_H
#define _DECLARATIONS_H

#include "types.h"
#include "layeredmap.h"
#include "syntaxtree.h"
#include "idmap.h"
#include "frame.h"
#include <map>
#include <list>

namespace Semantic {
	
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
	IR::Code value;
	IR::VarLocation *implementation;
	
	Variable(const std::string &_name, Type *_type,
		IR::Code _value, IR::VarLocation *impl) :
		Declaration(DECL_VARIABLE), name(_name), type(_type), value(_value),
		implementation(impl) {}
};

class Function;

class FunctionArgument: public Variable {
public:
	Function *function;
	
	FunctionArgument(Function *owner, const std::string &_name, Type *_type,
		IR::VarLocation *impl) :
		Variable(_name, _type, NULL, impl), function(owner)
	{
		kind = DECL_ARGUMENT;
	}
};

class LoopVariable: public Variable {
public:
	LoopVariable(const std::string &_name, Type *_type,
		IR::VarLocation *impl
	) : Variable(_name, _type, NULL, impl)
	{
		kind = DECL_LOOP_VARIABLE;
	}
};

class Function: public Declaration {
public:
	int id;
	std::string name;
	Type *return_type;
	std::list<FunctionArgument> arguments;
	Syntax::Tree raw_body;
	IR::Function implementation;

	Function(const std::string &_name, Type *_return_type,
		Syntax::Tree _raw, IR::Code _body,
		IR::AbstractFrame *_frame, IR::Label *_label) :
		Declaration(DECL_FUNCTION), 
		id(_frame ? _frame->getId() : -1), name(_name), return_type(_return_type), 
		raw_body(_raw), implementation(_body, _frame, _label)
	{}
	
	FunctionArgument *addArgument(const std::string &name, Type *type,
		IR::VarLocation *impl)
	{
		arguments.push_back(FunctionArgument(this, name, type, impl));
		return &(arguments.back());
	}
};

}

#endif
