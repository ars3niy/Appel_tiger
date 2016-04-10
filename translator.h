#ifndef _TRANSLATOR_H
#define _TRANSLATOR_H

#include "syntaxtree.h"
#include "layeredmap.h"
#include "declarations.h"
#include "translate_utils.h"

#include <string>
#include <map>
#include <list>

namespace Semantic {

class TranslatorPrivate;

class Translator {
private:
	TranslatorPrivate *impl;
public:
	Translator(IR::IREnvironment *ir_inv, IR::AbstractFrameManager * _framemanager);
	~Translator();
	
	void translateProgram(Syntax::Tree expression,
		IR::Statement &result, IR::AbstractFrame *&frame);
	void printFunctions(FILE *out);
	void canonicalizeProgram(IR::Statement &statement);
	void canonicalizeFunctions();
	const std::list<Function> &getFunctions();
};

}

#endif
