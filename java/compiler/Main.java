package compiler;

import gnu.getopt.Getopt;

import java.util.LinkedList;

import java.io.FileReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.PrintWriter;
import java.lang.ProcessBuilder;
import java.lang.Process;

import error.Error;

class CodeInfo {
	asm.Instructions code;
	ir.Label funclabel;
	frame.AbstractFrame frame;
};

public class Main {
	private static String GetExtension(String filename)
	{
		int i = filename.lastIndexOf('.');
		if (i < 0)
			return "";
		else
			return filename.substring(i+1);
	}
	
	private static String StripExtension(String filename)
	{
		int i = filename.lastIndexOf('.');
		if (i < 0)
			return filename;
		else
			return filename.substring(0, i);
	}
	
	private static void PrintIR(translate.Translator translator,
			ir.IREnvironment IR_env, String filename) {
		PrintWriter f = null;
		try {
			f = new PrintWriter(new File(filename));
			for (ir.Function func: translator.getFunctions())
				if (func.body != null)
					util.DebugPrinter.PrintFunction(f, func);
			util.DebugPrinter.PrintFunction(f, translator.getProgramBody());
			for (ir.Blob blob: IR_env.getBlobs())
				util.DebugPrinter.PrintBlob(f, blob);
		} catch (IOException e) {
			System.out.println("Could not write IR: " + e);
		} finally {
            try {
                f.close();
            } catch (Exception e) {
            }
        }
	}
	
	private static void OutputAsm(asm.Assembler assembler,
			LinkedList<asm.Instructions> code, ir.IREnvironment IR_env,
			ir.RegisterMap register_map, String filename) {
		PrintWriter f = null;
		try {
			f = new PrintWriter(new File(filename));
			assembler.outputCode(f, code, register_map);
			assembler.outputBlobs(f, IR_env.getBlobs());
		} catch (IOException e) {
			System.out.println("Could not write assembler output to " +
				filename + ": " + e);
		} finally {
            try {
                f.close();
            } catch (Exception e) {
            }
        }
	}
	
	private static int system_impl(ProcessBuilder builder) {
		builder.redirectOutput(ProcessBuilder.Redirect.INHERIT);
		builder.redirectError(ProcessBuilder.Redirect.INHERIT);
		Process process = null;
		try {
			process = builder.start();
			return process.waitFor();
		} catch (InterruptedException e) {
			if (process != null)
				process.destroy();
			return 1;
		} catch (IOException e) {
			return 1;
		}
	}
	
	private static int system(String[] cmd) {
		return system_impl(new ProcessBuilder(cmd));
	}
	
	private static int system(LinkedList<String> cmd) {
		return system_impl(new ProcessBuilder(cmd));
	}
	
	public static void TranslateProgram(parse.Node program, String inputname,
			String objname) {
		ir.IREnvironmentImpl IR_env = new ir.IREnvironmentImpl();
		x86_64.FrameManager framemanager = new x86_64.FrameManager(IR_env);
		translate.Translator translator = new translate.Translator(IR_env, framemanager);

		translator.translateProgram(program);
		Error.current.eof();
		
		if (Error.current.errorCount() != 0) {
			System.out.printf("%d errors, stopping\n", Error.current.errorCount());
			return;
		}
		
		if (util.DebugPrinter.ENABLE)
			PrintIR(translator, IR_env, "intermediate");
		
		translator.canonicalize();
		
		if (util.DebugPrinter.ENABLE)
			PrintIR(translator, IR_env, "canonical");

		asm.Assembler assembler = new x86_64.Assembler(IR_env);
		LinkedList<asm.Instructions> code = new LinkedList<>();
		LinkedList<CodeInfo> functions = new LinkedList<>();
		
		for (ir.Function func: translator.getFunctions())
			if (func.body != null) {
				code.add(new asm.Instructions());
				functions.add(new CodeInfo());
				assembler.translateFunction(func.body, func.frame, code.getLast());
				functions.getLast().code = code.getLast();
				functions.getLast().frame = func.frame;
				functions.getLast().funclabel = func.label;
			}
		code.add(new asm.Instructions());
		ir.Function program_ir = translator.getProgramBody();
		assembler.translateProgram(program_ir.body, program_ir.frame, code.getLast());
		
		if (util.DebugPrinter.ENABLE)
			OutputAsm(assembler, code, IR_env, null, "assembler_raw");
		
		ir.RegisterMap register_map = IR_env.initRegisterMap();
		for (CodeInfo func: functions) {
			ir.RegisterMap func_map = IR_env.initRegisterMap();
			regalloc.Allocator.AssignRegisters(func.code, assembler, func.frame,
					assembler.getAvailableRegisters(), func_map);
			register_map.merge(func_map);
			assembler.implementFrameSize(func.frame, func.code);
			assembler.finishFunction(func.funclabel, func.code);
		}
		
		ir.RegisterMap body_map = IR_env.initRegisterMap();
		regalloc.Allocator.AssignRegisters(code.getLast(), assembler, program_ir.frame,
				assembler.getAvailableRegisters(), body_map);
		register_map.merge(body_map);
		assembler.implementFrameSize(program_ir.frame, code.getLast());
		assembler.finishProgram(code.getLast());
		
		String asm_name = StripExtension(inputname) + ".s";
		OutputAsm(assembler, code, IR_env, register_map, asm_name);
		
		if (objname != null) {
			String[] asm_cmd = {"as", "-o", objname, asm_name};
			if (system(asm_cmd) != 0)
				Error.current.fatalError("Failed to run assembler on " + asm_name);
			
			if (! util.DebugPrinter.ENABLE) {
				File f = new File(asm_name);
				f.delete();
			}
		}
	}
	
	enum Mode {
		TRANSLATE,
		ASSEMBLE,
		FULL
	};
	
	private static int link(final LinkedList<String> filenames, String out_name,
			final String libdir) {
		LinkedList<String> command = new LinkedList<>();
		command.add("ld");
		command.add("-o");
		command.add(out_name);
		command.addAll(filenames);
		command.add(libdir + File.separator + "libtigerlibrary.a");
		command.add("-dynamic-linker");
		command.add("/lib64/ld-linux-x86-64.so.2");
		command.add("/usr/lib64/crt1.o");
		command.add("/usr/lib64/crti.o");
		command.add("-lc");
		command.add("/usr/lib64/crtn.o");
		return system(command);
	}

	public static void main(String[] args) {
		
		String USAGE =
			"Usage: compiler [options] <input-files>\n\n" +
			       "Options:\n" +
			       "  -c           Compile but do not link\n" +
			       "  -S           Translate to assembler, do not assemble or link\n" +
				   "  -C COMMAND   Specify C compiler (default: cc)\n" +
				   "  -o FILENAME  Specify executable file name (default: first input without extension)\n";
		
		Mode mode=Mode.FULL;
		String c_compiler = "cc";
		String out_name = "";
		String ourname = "dummy";
		int opt;
		Getopt opts = new Getopt("compiler", args, "cSC:o:x:");
		
		while ((opt = opts.getopt()) >= 0) {
			switch (opt) {
				case 'c':
					mode = Mode.ASSEMBLE;
					break;
				case 'S':
					mode = Mode.TRANSLATE;
					break;
				case 'C':
					c_compiler = opts.getOptarg();
					break;
				case 'o':
					out_name = opts.getOptarg();
					break;
				case 'x':
					ourname = opts.getOptarg();
					break;
				default:
					System.out.print(USAGE);
					System.exit(1);
			}
		}
		File we = new File(ourname);
		String libdir = we.getAbsolutePath();
		libdir = libdir.substring(0, libdir.lastIndexOf(File.separator));
		
		if (opts.getOptind() >= args.length) {
			System.out.print(USAGE);
			System.exit(1);
		}
		
		LinkedList<String> objfiles_translated = new LinkedList<>();
		LinkedList<String> objfiles_input = new LinkedList<>();
		
		boolean had_tiger_input = false;
		if (out_name == "")
			out_name = StripExtension(args[opts.getOptind()]);
		
		boolean success = true;
		
		for (int i = opts.getOptind(); i < args.length; i++) {
			String inputname = args[i];
			
			String extension = GetExtension(inputname);
			String obj_name = StripExtension(inputname) + ".o";
			Error err = new Error(inputname);
			if (extension.equals("tig")) {
				if (had_tiger_input)
					err.error("Cannot have second tiger output " + inputname + "\n");
				else {
					had_tiger_input = true;
					try {
						FileReader reader = new FileReader(inputname);
						parse.Yylex lexer = new parse.Yylex(reader, err);
						parse.Parser parser = new parse.Parser();
						parse.Node result = null;
						
						try {
							result = parser.yyparse(lexer);
						} catch (parse.Parser.yyException e) {
							err.fatalError("Parser error: " + e);
						}
						
						if (err.errorCount() == 0) {
							TranslateProgram(result, inputname,
								mode == Mode.TRANSLATE ? null : obj_name);
							objfiles_translated.add(obj_name);
							objfiles_input.add(obj_name);
						}
						
					} catch (FileNotFoundException e) {
						System.out.println("Cannot open " + inputname + ": " + e.getMessage());
						success = false;
					} catch (IOException e) {
						System.out.println("Error reading " + inputname + ": " + e.getMessage());
						success = false;
					}
				}
			} else if (extension.equals("c")) {
				String[] cmd;
				if (mode == Mode.TRANSLATE) {
					String[] fuck = {c_compiler, "-S", inputname};
					cmd = fuck;
				} else {
					String[] fuck = {c_compiler, "-c", inputname, "-o", obj_name};
					cmd = fuck;
				}
				
				objfiles_translated.add(obj_name);
				objfiles_input.add(obj_name);
				int ret = system(cmd);
				if (ret != 0) {
					System.out.println("C compiler failed on " + inputname);
					success = false;
				}
			} else if (extension.equals("o")) {
				objfiles_input.add(inputname);
			} else {
				System.out.print("Unrecognized input extension for " + inputname + "\n");
				success = false;
			}
			
			if (err.errorCount() != 0)
				success = false;
			
			if (! success)
				break;
		}
		
		if (mode == Mode.FULL) {
			if (success) {
				int ret = link(objfiles_input, out_name, libdir);
				if (ret != 0) {
					System.out.println("Linker error");
					success = false;
				}
			}
			
			for (String s: objfiles_translated) {
				File f = new File(s);
				f.delete();
			}
		}

		if (! success)
			System.exit(1);
	}

}
