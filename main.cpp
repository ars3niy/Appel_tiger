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

std::string inputname;

std::string GetExtension(const std::string &filename)
{
	const char *s = filename.c_str();
	const char *t = strrchr(s, '.');
	if (t == NULL)
		return "";
	else
		return t+1;
}

std::string StripExtension(const std::string &filename)
{
	const char *s = filename.c_str();
	const char *t = strrchr(s, '.');
	if (t == NULL)
		return filename;
	else {
		std::string basename = filename;
		basename.resize(t-s);
		return basename;
	}
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
	//Syntax::PrintTree(tree);
	IR::IREnvironment IR_env;
	IR::X86_64FrameManager framemanager(&IR_env);
	Semantic::Translator translator(&IR_env, &framemanager);

	IR::AbstractFrame *body_frame;
	Semantic::Type *type;
	IR::Statement *program_body;
	translator.translateProgram(tree, program_body, body_frame);
	Syntax::DestroySyntaxTree(tree);
	
	if (Error::getErrorCount() != 0)
		return;
	
#ifdef DEBUG
	FILE *f = fopen("intermediate", "w");
	translator.printFunctions(f);
	IR::PrintStatement(f, program_body);
	IR_env.printBlobs(f);
	fclose(f);
#endif
	
	translator.canonicalizeFunctions();
	translator.canonicalizeProgram(program_body);
	
#ifdef DEBUG
	f = fopen("canonical", "w");
	translator.printFunctions(f);
	IR::PrintStatement(f, program_body);
	IR_env.printBlobs(f);
	fclose(f);
#endif
	
	Asm::X86_64Assembler assembler(&IR_env);
	std::list<Asm::Instructions> code;
	std::list<CodeInfo> chunks;
#ifdef DEBUG
	f = fopen("liveness", "w");
#endif
	
	for (Semantic::Function func: translator.getFunctions())
		if (func.body != NULL) {
			code.push_back(Asm::Instructions());
			chunks.push_back(CodeInfo());
			assembler.translateFunctionBody(func.body, func.label,
				func.frame, code.back());
#ifdef DEBUG
			Optimize::PrintLivenessInfo(f, code.back(), func.frame);
#endif
			chunks.back().code = &code.back();
			chunks.back().frame = func.frame;
			chunks.back().funclabel = func.label;
		}
	code.push_back(Asm::Instructions());
	assembler.translateProgram(program_body, body_frame, code.back());
#ifdef DEBUG
	//Optimize::PrintLivenessInfo(f, code.back(), body_frame);
	fclose(f);
#endif

#ifdef DEBUG
	f = fopen("assembler_raw", "w");
	assembler.outputCode(f, code, NULL);
	assembler.outputBlobs(f, IR_env.getBlobs());
	fclose(f);
#endif
		
	const std::vector<IR::VirtualRegister *> &machine_registers =
		assembler.getAvailableRegisters();
	//IR::VirtualRegister *fp_register = assembler.getFramePointerRegister();
	IR::RegisterMap virtual_register_map;
	for (CodeInfo &chunk: chunks) {
		IR::RegisterMap vreg_map;
		Optimize::AssignRegisters(*chunk.code,
			assembler,
			chunk.frame,
			machine_registers,
			vreg_map);

		MergeVirtualRegisterMaps(virtual_register_map, vreg_map);
		
		assembler.implementFunctionFrameSize(chunk.funclabel,
			chunk.frame, *chunk.code);
	}
	
	IR::RegisterMap vreg_map;
	Optimize::AssignRegisters(code.back(),
		assembler,
		body_frame,
		machine_registers,
		vreg_map);
	MergeVirtualRegisterMaps(virtual_register_map, vreg_map);
	assembler.implementProgramFrameSize(body_frame, code.back());
	
	std::string basename = StripExtension(inputname);
	std::string asm_name = basename + ".s";

	FILE *asm_out = fopen(asm_name.c_str(), "w");
	assembler.outputCode(asm_out, code, &virtual_register_map);
	assembler.outputBlobs(asm_out, IR_env.getBlobs());
	fclose(asm_out);
	
	std::string obj_name = basename + ".o";
	std::string asm_cmd = "as -o " + obj_name + " " + asm_name;
	if (system(asm_cmd.c_str()) != 0)
		Error::fatalError("Failed to run assembler on " + asm_name);
	
#ifndef DEBUG
	unlink(asm_name.c_str());
#endif
}

std::string ourname;

void link(const std::list<std::string> &filenames, const std::string &out_name)
{
	std::string input_list;
	for (const std::string &obj_name: filenames) {
		input_list += " ";
		input_list += obj_name;
	}
	std::string link_cmd = "ld -o " + out_name +
		input_list +
		" " + GetDirPath(ourname) + "/libtigerlibrary.a "
		"-dynamic-linker /lib64/ld-linux-x86-64.so.2 "
		"/usr/lib64/crt1.o /usr/lib64/crti.o "
		"-lc /usr/lib64/crtn.o";
	int ret = system(link_cmd.c_str());

	if (ret != 0) {
		Error::global_error("Linker error");
	}
}

int main(int argc, char **argv)
{
	bool compile_only = false;
	const char *USAGE =
		"Usage: compiler [options] <input-files>\n\n"
		       "Options:\n"
		       "  -c           Compile but do not link\n";
			   "  -C COMMAND   Specify C compiler (default: cc)\n"
			   "  -o FILENAME  Specify executable file name (default: first input without extension)\n";
	int opt;
	std::string c_compiler = "cc";
	std::string out_name = "";
	
	while ((opt = getopt(argc, argv, "cC:o:")) >= 0) {
		switch (opt) {
			case 'c':
				compile_only = true;
				break;
			case 'C':
				c_compiler = optarg;
				break;
			case 'o':
				out_name = optarg;
				break;
			default:
				fputs(USAGE, stdout);
				return 1;
		}
	}
	if (optind >= argc) {
		fputs(USAGE, stdout);
		return 1;
	}
	ourname = argv[0];
	
	std::list<std::string> objfiles_translated, objfiles_input;
	
	bool had_tiger_input = false;
	if (out_name == "")
		out_name = StripExtension(argv[optind]);
	
	for (int i = optind; i < argc; i++) {
		inputname = argv[i];
		
		std::string extension = GetExtension(inputname);
		std::string obj_name = StripExtension(inputname) + ".o";
		
		if (extension == "tig") {
			if (had_tiger_input)
				Error::error("Cannot have second tiger input " + inputname);
			else {
				had_tiger_input = true;
				Error::setFileName(inputname);
				yyin = fopen(inputname.c_str(), "r");
				if (yyin == NULL)
					Error::global_error("Cannot open " + inputname + ": " +
						strerror(errno));
				else {
					yyparse();
					fclose(yyin);
					objfiles_translated.push_back(obj_name);
					objfiles_input.push_back(obj_name);
				}
			}
		} else if (extension == "c") {
			std::string cmd = c_compiler + " -c " + inputname +
				" -o " + obj_name;
			int ret = system(cmd.c_str());
			if (ret != 0)
				Error::error("C compiler failed on " + inputname);
			else {
				objfiles_translated.push_back(obj_name);
				objfiles_input.push_back(obj_name);
			}
		} else if (extension == "o")
			objfiles_input.push_back(inputname);
		else
			Error::global_error("Unrecognized input extension for " + inputname);
		
		if (Error::getErrorCount() != 0)
			break;
	}
	
	if (! compile_only) {
		if (Error::getErrorCount() == 0)
			link(objfiles_input, out_name);
		
		for (const std::string &s: objfiles_translated)
			unlink(s.c_str());
	}
	
	if (Error::getErrorCount() == 0)
		return 0;
	else
		return 1;
}
