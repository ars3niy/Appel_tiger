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
	
	IR::Statement *translateProgram(Syntax::Tree expression);
	void printFunctions(FILE *out);
	void canonicalizeProgram(IR::Statement *&statement);
	void canonicalizeFunctions();
};

}

#endif
