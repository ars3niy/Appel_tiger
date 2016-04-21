#ifndef _TRANSLATOR_H
#define _TRANSLATOR_H

#include "syntaxtree.h"
#include "layeredmap.h"
#include "declarations.h"

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
	void canonicalizeProgramAndFunctions(IR::Statement &program_body,
		IR::AbstractFrame *body_frame);
	const std::list<Function> &getFunctions();
};

}

#endif
