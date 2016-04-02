#include "translate_utils.h"
#include "translator.h"
#include "errormsg.h"
#include "x86_64_frame.h"
#include "x86_64assembler.h"
#include "regallocator.h"

#include <iostream>
#include <string>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <syntaxtree.h>

extern "C" {
extern FILE *yyin;
}

void MergeVirtualRegisterMaps(IR::RegisterMap &merge_to,
	const IR::RegisterMap &merge_from)
{
	if (merge_to.size() < merge_from.size())
		merge_to.resize(merge_from.size(), NULL);
	for (int i = 0; i < merge_from.size(); i++)
		if (merge_from[i] != NULL) {
			if (merge_to[i] != NULL)
				assert(merge_from[i]->getIndex() == merge_to[i]->getIndex());
			merge_to[i] = merge_from[i];
		}
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
		std::list<Asm::Instructions> code;
		
		for (std::list<Semantic::Function>::const_iterator func =
				translator.getFunctions().begin(); 
				func != translator.getFunctions().end(); func++) {
			code.push_back(Asm::Instructions());
			assembler.translateFunctionBody((*func).body, (*func).frame, code.back());
		}
		code.push_back(Asm::Instructions());
		assembler.translateProgram(program_body, body_frame, code.back());
		f = fopen("assembler_raw", "w");
		assembler.outputCode(f, code, NULL);
		assembler.outputBlobs(f, IR_env.getBlobs());
		fclose(f);
		
		fwrite(assembler.debug_output.c_str(), assembler.debug_output.size(),
			1, stdout);
		
		f = fopen("liveness", "w");
		Optimize::PrintLivenessInfo(f, code.back());
		fclose(f);
		
		const std::vector<IR::VirtualRegister *> &machine_registers =
			assembler.getAvailableRegisters();
		IR::RegisterMap virtual_register_map;
		
		for (std::list<Asm::Instructions>::iterator codechunk = code.begin();
				codechunk != code.end(); codechunk++) {
			IR::RegisterMap vreg_map;
			std::list<IR::VirtualRegister *> unassigned_vreg;
			Optimize::AssignRegisters(*codechunk, machine_registers,
				vreg_map, unassigned_vreg);
			MergeVirtualRegisterMaps(virtual_register_map, vreg_map);
		}
		f = fopen("assembler_final", "w");
		assembler.outputCode(f, code, &virtual_register_map);
		assembler.outputBlobs(f, IR_env.getBlobs());
		fclose(f);
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
