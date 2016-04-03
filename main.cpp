#include "translate_utils.h"
#include "translator.h"
#include "errormsg.h"
#include "x86_64_frame.h"
#include "x86_64assembler.h"
#include "regallocator.h"
#include "syntaxtree.h"

#include <iostream>
#include <string>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <stdlib.h>
#include <unistd.h>

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

std::string inputname, ourname;

std::string StripTigExtension(const std::string &input)
{
	std::string basename = inputname;
	if ((inputname.size() > 4) &&
			(inputname.substr(inputname.size()-4, 4) == ".tig"))
		basename.resize(inputname.size()-4);
	return basename;
}

std::string GetDirPath(const std::string &path)
{
	char *s = (char *)malloc(path.size()+1);
	strcpy(s, path.c_str());
	std::string result = dirname(s);
	free(s);
	return result;
}

struct CodeInfo {
	Asm::Instructions *code;
	IR::Label *funclabel;
	IR::AbstractFrame *frame;
};

void ProcessTree(Syntax::Tree tree)
{
	system("rm -f *.log");
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
		std::list<CodeInfo> chunks;
		f = fopen("liveness", "w");
		
		for (std::list<Semantic::Function>::const_iterator func =
				translator.getFunctions().begin(); 
				func != translator.getFunctions().end(); func++)
			if ((*func).body != NULL) {
				code.push_back(Asm::Instructions());
				chunks.push_back(CodeInfo());
				assembler.translateFunctionBody((*func).body, (*func).label,
					(*func).frame, code.back());
				Optimize::PrintLivenessInfo(f, code.back(), (*func).frame);
				chunks.back().code = &code.back();
				chunks.back().frame = (*func).frame;
				chunks.back().funclabel = (*func).label;
			}
		code.push_back(Asm::Instructions());
		assembler.translateProgram(program_body, body_frame, code.back());
		Optimize::PrintLivenessInfo(f, code.back(), body_frame);
		fclose(f);

		f = fopen("assembler_raw", "w");
		assembler.outputCode(f, code, NULL);
		assembler.outputBlobs(f, IR_env.getBlobs());
		fclose(f);
		
			
		const std::vector<IR::VirtualRegister *> &machine_registers =
			assembler.getAvailableRegisters();
		//IR::VirtualRegister *fp_register = assembler.getFramePointerRegister();
		IR::RegisterMap virtual_register_map;
		for (std::list<CodeInfo>::iterator chunk = chunks.begin();
				chunk != chunks.end(); chunk++) {
			IR::RegisterMap vreg_map;
			printf("Optimiving %s\n", (*chunk).funclabel->getName().c_str());
			Optimize::AssignRegisters(*(*chunk).code,
				assembler,
				(*chunk).frame,
				machine_registers,
				vreg_map);
			printf("done\n");

			MergeVirtualRegisterMaps(virtual_register_map, vreg_map);
			
			assembler.implementFunctionFrameSize((*chunk).funclabel,
				(*chunk).frame, *(*chunk).code);
		}
		
		IR::RegisterMap vreg_map;
		Optimize::AssignRegisters(code.back(),
			assembler,
			body_frame,
			machine_registers,
			vreg_map);
		MergeVirtualRegisterMaps(virtual_register_map, vreg_map);
		assembler.implementProgramFrameSize(body_frame, code.back());
		
		std::string basename = StripTigExtension(inputname);
		
		std::string asm_name = basename + ".s";
	
		f = fopen(asm_name.c_str(), "w");
		assembler.outputCode(f, code, &virtual_register_map);
		assembler.outputBlobs(f, IR_env.getBlobs());
		fclose(f);
		
// 		fwrite(assembler.debug_output.c_str(), assembler.debug_output.size(),
// 		   1, stdout);
	
		std::string obj_name = basename + ".o";
		std::string asm_cmd = "as -o " + obj_name + " " + asm_name;
		if (system(asm_cmd.c_str()) != 0)
			Error::fatalError("Failed to run assembler");
		std::string link_cmd = "ld -o " + basename + ".elf " + obj_name +
			" " + GetDirPath(ourname) + "/libtigerlibrary.a "
			"-dynamic-linker /lib64/ld-linux-x86-64.so.2 "
			"/usr/lib64/crt1.o /usr/lib64/crti.o "
			"-lc /usr/lib64/crtn.o";
		int ret = system(link_cmd.c_str());
		unlink(obj_name.c_str());
		if (ret != 0)
			Error::fatalError("Linker error");
	}
}

int main(int argc, char **argv) {
	if (argc != 2) {
		printf("Usage: compiler <input-file>\n");
		return 1;
	}
	ourname = argv[0];
	inputname = argv[1];
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
