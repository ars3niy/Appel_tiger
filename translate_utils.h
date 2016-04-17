#ifndef _TRANSLATE_UTILS_H
#define _TRANSLATE_UTILS_H

#include "idmap.h"
#include "syntaxtree.h"
#include "types.h"
#include <vector>
#include <list>

namespace Semantic {

class VariablesAccessInfoPrivate;

class VariablesAccessInfo {
private:
	VariablesAccessInfoPrivate *impl;
	std::list<int> func_stack;
	
	void handleCall(Syntax::Tree function_exp, ObjectId current_function_id);
public:
	VariablesAccessInfo();
	~VariablesAccessInfo();
	
	void processExpression(Syntax::Tree expression, ObjectId current_function_id);
	void processDeclaration(Syntax::Tree declaration, ObjectId current_function_id);
	bool isAccessedByAddress(Syntax::Tree definition);
	bool functionNeedsParentFp(Syntax::Function *definition);
	bool isFunctionParentFpAccessedByChildren(Syntax::Function *definition);
};

}

#endif
