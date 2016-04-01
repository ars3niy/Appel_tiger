#ifndef _DECLARATIONS_H
#define _DECLARATIONS_H

#include "types.h"
#include "layeredmap.h"
#include "syntaxtree.h"
#include "idmap.h"
#include "translate_utils.h"
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
	IR::Code *value;
	IR::AbstractVarLocation *implementation;
	bool isAccessedByAddress;
	
	Variable(const std::string &_name, Type *_type,
		IR::Code *_value, IR::AbstractVarLocation *impl) :
		Declaration(DECL_VARIABLE), name(_name), type(_type), value(_value),
		implementation(impl), isAccessedByAddress(false) {}
};

class Function;

class FunctionArgument: public Variable {
public:
	Function *function;
	
	FunctionArgument(Function *owner, const std::string &_name, Type *_type,
		IR::AbstractVarLocation *impl) :
		Variable(_name, _type, NULL, impl), function(owner)
	{
		kind = DECL_ARGUMENT;
	}
};

class LoopVariable: public Variable {
public:
	LoopVariable(const std::string &_name, Type *_type,
		IR::AbstractVarLocation *impl
	) : Variable(_name, _type, NULL, impl)
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
	IR::Code *body;
	IR::AbstractFrame *frame;
	IR::Label *label;
	bool needs_parent_fp;

	Function(const std::string &_name, Type *_return_type,
		Syntax::Tree _raw, IR::Code *_body,
		IR::AbstractFrame *_frame, IR::Label *_label,
		bool _needs_parent_fp) :
		name(_name), Declaration(DECL_FUNCTION), return_type(_return_type), 
		raw_body(_raw), body(_body), frame(_frame), label(_label),
		needs_parent_fp(_needs_parent_fp)
	{}
	FunctionArgument *addArgument(const std::string &name, Type *type,
		IR::AbstractVarLocation *impl)
	{
		arguments.push_back(FunctionArgument(this, name, type, impl));
		return &(arguments.back());
	}
};

}

#endif
