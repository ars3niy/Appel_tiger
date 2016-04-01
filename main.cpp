#include "translate_utils.h"
#include "translator.h"
#include "errormsg.h"
#include "x86_64_frame.h"
#include "x86_64assembler.h"

#include <iostream>
#include <string>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <syntaxtree.h>

extern "C" {
extern FILE *yyin;
}

void ProcessTree(Syntax::Tree tree)
{
	//Syntax::PrintTree(tree);
	IR::IREnvironment IR_env;
	IR::X86_64FrameManager framemanager(&IR_env);
	Semantic::Translator translator(&IR_env, &framemanager);

	IR::AbstractFrame *body_frame;
	Semantic::Type *type;
	IR::Statement *program_body;
	translator.translateProgram(tree, program_body, body_frame);
	Syntax::DestroySyntaxTree(tree);
	if (Error::getErrorCount() == 0) {
		FILE *f = fopen("intermediate", "w");
		translator.printFunctions(f);
		IR::PrintStatement(f, program_body);
		IR_env.printBlobs(f);
		fclose(f);
		
		translator.canonicalizeProgram(program_body);
		translator.canonicalizeFunctions();
		
		f = fopen("canonical", "w");
		translator.printFunctions(f);
		IR::PrintStatement(f, program_body);
		IR_env.printBlobs(f);
		fclose(f);
		
		Asm::X86_64Assembler assembler(&IR_env);
		Asm::Instructions code;
		
		f = fopen("assembler", "w");
		for (std::list<Semantic::Function>::const_iterator func =
				translator.getFunctions().begin();
				func != translator.getFunctions().end(); func++)
			assembler.translateFunctionBody((*func).body, (*func).frame, code);
		assembler.translateProgram(program_body, body_frame, code);
		assembler.outputCode(f, code);
		assembler.outputBlobs(f, IR_env.getBlobs());
		
		fwrite(assembler.debug_output.c_str(), assembler.debug_output.size(),
			1, stdout);
	}
}

int main(int argc, char **argv) {
	if (argc != 2) {
		printf("Usage: compiler <input-file>\n");
		return 1;
	}
	yyin = fopen(argv[1], "r");
	if (yyin == NULL) {
		printf("Cannot open %s: %s\n", argv[1], strerror(errno));
		return 1;
	}
	
	yyparse();
	
	fclose(yyin);
	if (Error::getErrorCount() == 0)
		return 0;
	else
		return 1;
}
