#ifndef _TRANSLATOR_H
#define _TRANSLATOR_H

#include "syntaxtree.h"
#include "layeredmap.h"
#include "declarations.h"

#include <string>
#include <map>
#include <list>

namespace Semantic {

class TranslatedExpression;
class DeclarationsEnvironmentPrivate;

class DeclarationsEnvironment {
private:
	DeclarationsEnvironmentPrivate *impl;
public:
	DeclarationsEnvironment();
	~DeclarationsEnvironment();
	
	void translateExpression(Syntax::Tree expression,
		TranslatedExpression *&translated, Type *&type,
		Syntax::Tree lastloop = NULL);
};

class TranslatedExpression {
};

}

#endif
