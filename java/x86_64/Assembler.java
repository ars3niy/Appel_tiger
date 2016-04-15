package x86_64;

import java.util.Vector;
import java.util.LinkedList;
import java.util.ListIterator;

import asm.Instruction;
import asm.Instructions;
import error.Error;
import frame.VarLocation;

public class Assembler extends asm.AssemblerImpl {
	
	private static final int
		RAX = 0,
		RDX = 1,
		RCX = 2,
		RBX = 3,
		RSI = 4,
		RDI = 5,
		R8 = 6,
		R9 = 7,
		R10 = 8,
		R11 = 9,
		R12 = 10,
		R13 = 11,
		R14 = 12,
		R15 = 13,
		RBP = 14,
		RSP = 15,
		FP = 16, // fake
		LAST_REGISTER = 17;
	
	private static int[] paramreg_list = {RDI, RSI, RDX, RCX, R8, R9};
	private static int RESULT_REG = RAX;
	private static int FPLINK_REG = R10;
	private static int[] callersave_list = {RDI, RSI, RDX, RCX, R8, R9, RESULT_REG, FPLINK_REG, R11};
	private static int[] calleesave_list = {RBX, RBP, R12, R13, R14, R15};
	
	private Vector<ir.Expression> memory_exp = new Vector<>();
	private ir.Expression exp_int = new ir.IntegerExp(0);
	private ir.Expression exp_label = new ir.LabelAddressExp(null);
	private ir.Expression exp_register = new ir.RegisterExp(null);
	
	private ir.Expression[] exp_mul_index = new ir.Expression[8];
	private ir.Expression[] exp_mul_index_plus = new ir.Expression[8];
	private ir.VirtualRegister[] machine_registers = new ir.VirtualRegister[LAST_REGISTER];
	
	private ir.VirtualRegister[] available_registers = new ir.VirtualRegister[RSP];
	private ir.VirtualRegister[] callersave_registers = new ir.VirtualRegister[callersave_list.length];
	private ir.VirtualRegister[] calleesave_registers = new ir.VirtualRegister[calleesave_list.length];

	public Assembler(ir.IREnvironment ir_env) {
		super(ir_env);
		
		String[] register_names = {
			"%rax",
			"%rdx",
			"%rcx",
			"%rbx",
			"%rsi",
			"%rdi",
			"%r8",
			"%r9",
			"%r10",
			"%r11",
			"%r12",
			"%r13",
			"%r14",
			"%r15",
			"%rbp",
			"%rsp",
			"fp",
		};
		
		for (int r = 0; r < LAST_REGISTER; r++) {
			machine_registers[r] = IRenvironment.addRegister(register_names[r]);
			if ((r < RSP))
				available_registers[r] = machine_registers[r];
		}
		
		for (int i = 0; i < callersave_list.length; i++)
			callersave_registers[i] = machine_registers[callersave_list[i]];
		
		for (int i = 0; i < calleesave_list.length; i++)
			calleesave_registers[i] = machine_registers[calleesave_list[i]];

		memory_exp.add(exp_int);
		memory_exp.add(exp_label);
		memory_exp.add(exp_register);
		// reg + const, const + reg
		memory_exp.add(new ir.BinaryOpExp(ir.BinaryOpExp.PLUS, exp_register, exp_int));
		memory_exp.add(new ir.BinaryOpExp(ir.BinaryOpExp.PLUS, exp_int, exp_register));
		// reg - const
		memory_exp.add(new ir.BinaryOpExp(ir.BinaryOpExp.MINUS, exp_register, exp_int));

		addTemplate(exp_int);
		addTemplate(exp_label);
		addTemplate(exp_register);
		for (ir.Expression exp: memory_exp)
			addTemplate(new ir.MemoryExp(exp));
		
		make_arithmetic(ir.BinaryOpExp.PLUS);
		make_arithmetic(ir.BinaryOpExp.MINUS);
		// imul
		make_arithmetic(ir.BinaryOpExp.MUL);
		// don't forget cqo!! and idivq
		make_arithmetic(ir.BinaryOpExp.DIV);
		
		make_comparison(ir.CondJumpStatm.EQUAL);
		make_comparison(ir.CondJumpStatm.NONEQUAL);
		make_comparison(ir.CondJumpStatm.LESS);
		make_comparison(ir.CondJumpStatm.LESSEQUAL);
		make_comparison(ir.CondJumpStatm.GREATER);
		make_comparison(ir.CondJumpStatm.GREATEQUAL);
		make_comparison(ir.CondJumpStatm.ULESS);
		make_comparison(ir.CondJumpStatm.ULESSEQUAL);
		make_comparison(ir.CondJumpStatm.UGREATER);
		make_comparison(ir.CondJumpStatm.UGREATEQUAL);
		make_assignment();
		
		addTemplate(new ir.CallExpression(new ir.LabelAddressExp(null), null));
		addTemplate(new ir.CallExpression(new ir.RegisterExp(null), null));
		addTemplate(new ir.JumpStatm(new ir.LabelAddressExp(null)));
		addTemplate(new ir.JumpStatm(new ir.RegisterExp(null)));
	}
	
	private void make_arithmetic(int ir_code)
	{
		// don't forget cqo
		addTemplate(new ir.BinaryOpExp(ir_code, exp_register, exp_int));
		addTemplate(new ir.BinaryOpExp(ir_code, exp_register, exp_register));
		for (ir.Expression exp: memory_exp) {
			addTemplate(new ir.BinaryOpExp(ir_code, exp_register,
				new ir.MemoryExp(exp)));
			addTemplate(new ir.BinaryOpExp(ir_code, exp_int,
				new ir.MemoryExp(exp)));
			addTemplate(new ir.BinaryOpExp(ir_code,
				new ir.MemoryExp(exp), exp_register));
			addTemplate(new ir.BinaryOpExp(ir_code,
				new ir.MemoryExp(exp), exp_int));
		}
		for (ir.Expression exp1: memory_exp)
			for (ir.Expression exp2: memory_exp)
				addTemplate(new ir.BinaryOpExp(ir_code,
					new ir.MemoryExp(exp1),
					new ir.MemoryExp(exp2)));
	}

	private void make_comparison(int ir_code)
	{
		addTemplate(new ir.CondJumpStatm(ir_code, exp_register, exp_int, null, null));
		addTemplate(new ir.CondJumpStatm(ir_code, exp_register, exp_register, null, null));
		for (ir.Expression exp: memory_exp) {
			addTemplate(new ir.CondJumpStatm(ir_code, exp_register,
				new ir.MemoryExp(exp), null, null));
			addTemplate(new ir.CondJumpStatm(ir_code, exp_int,
				new ir.MemoryExp(exp), null, null));
			addTemplate(new ir.CondJumpStatm(ir_code,
				new ir.MemoryExp(exp), exp_register, null, null));
			addTemplate(new ir.CondJumpStatm(ir_code,
				new ir.MemoryExp(exp), exp_int, null, null));
		}
	}

	private void make_assignment()
	{
		addTemplate(new ir.MoveStatm(exp_register, exp_int));
		addTemplate(new ir.MoveStatm(exp_register, exp_label));
		addTemplate(new ir.MoveStatm(exp_register, exp_register));
		for (ir.Expression exp: memory_exp) {
			addTemplate(new ir.MoveStatm(exp_register,
				new ir.MemoryExp(exp)));
			addTemplate(new ir.MoveStatm(exp_register, exp));
			addTemplate(new ir.MoveStatm(
				new ir.MemoryExp(exp), exp_register));
			addTemplate(new ir.MoveStatm(
				new ir.MemoryExp(exp), exp_int));
			addTemplate(new ir.MoveStatm(
				new ir.MemoryExp(exp), exp_label));
		}
	}

	void addInstruction(Instructions result,
			final String prefix, ir.Expression operand,
			final String suffix, ir.VirtualRegister output0) {
		addInstruction(result, prefix, operand, suffix, output0, null, null, null);
	}

	void addInstruction(Instructions result,
			final String prefix, ir.Expression operand,
			final String suffix, ir.VirtualRegister output0,
			ir.VirtualRegister extra_input,
			ir.VirtualRegister extra_output) {
		addInstruction(result, prefix, operand, suffix, output0, extra_input,
			extra_output, null);
	}

	void addInstruction(Instructions result,
			final String prefix, ir.Expression operand,
			final String suffix, ir.VirtualRegister output0,
			ir.VirtualRegister extra_input,
			ir.VirtualRegister extra_output,
			ListIterator<Instruction> insert_before) {
		Vector<ir.VirtualRegister> inputs = new Vector<>();
		String[] notation = new String[1];
		makeOperand(operand, inputs, notation);
		if (extra_input != null)
			inputs.add(extra_input);
		notation[0] = prefix + notation[0] + suffix;
		Vector<ir.VirtualRegister> outputs = new Vector<>();
		if (output0 != null) {
			outputs.add(output0);
			if (extra_output != null)
				outputs.add(extra_output);
		}
		ir.VirtualRegister[] suxx = new ir.VirtualRegister[0];
		Instruction newinst = new Instruction(notation[0], inputs.toArray(suxx),
				outputs.toArray(suxx),
				(prefix == "movq ") && (operand instanceof ir.RegisterExp));
		if (insert_before != null)
			insert_before.add(newinst);
		else
			result.add(newinst);
		if (insert_before != null)
			debug(String.format("Spill: [somewhere, Java sucks] . prepend %s",
				  newinst.notation));
	}
	
	void addInstruction(Instructions result,
			final String prefix, ir.Expression operand0,
			final String suffix, ir.Expression operand1) {
		Vector<ir.VirtualRegister> inputs = new Vector<>();
		String[] s0 = new String[1];
		makeOperand(operand0, inputs, s0);
		String[] s1 = new String[1];
		makeOperand(operand1, inputs, s1);
		ir.VirtualRegister[] empty = new ir.VirtualRegister[0];
		result.add(new Instruction(prefix + s0[0] + suffix + s1[0],
			inputs.toArray(empty), empty, false));
	}
	
	void makeOperand(ir.Expression expression,
			Vector<ir.VirtualRegister> add_inputs,
			String[] notation) {
		if (expression instanceof ir.IntegerExp)
			notation[0] = "$" + String.valueOf(((ir.IntegerExp)expression).value);
		else if (expression instanceof ir.LabelAddressExp) 
			notation[0] = "$" + ((ir.LabelAddressExp)expression).label().getName();
		else if (expression instanceof ir.RegisterExp) {
			notation[0] = Instruction.Input(add_inputs.size());
			add_inputs.add(((ir.RegisterExp)expression).reg);
		} else if (expression instanceof ir.MemoryExp) {
			ir.Expression addr = ((ir.MemoryExp)expression).address;
			if (addr instanceof ir.RegisterExp) {
				notation[0] = "(" + Instruction.Input(add_inputs.size()) + ")";
				add_inputs.add(((ir.RegisterExp)addr).reg);
			} else if (addr instanceof ir.IntegerExp) { 
				notation[0] = "(" +
					((ir.LabelAddressExp)addr).label().getName() + ")";
			} else if (addr instanceof ir.BinaryOpExp) {
				ir.BinaryOpExp binop = (ir.BinaryOpExp)addr;
				if (binop.operation == ir.BinaryOpExp.PLUS) {
					if (binop.left instanceof ir.IntegerExp) {
						ir.MemoryExp base =	new ir.MemoryExp(binop.right);
						int offset = ((ir.IntegerExp)binop.left).value;
						makeOperand(base, add_inputs, notation);
						if (offset != 0)
							notation[0] = String.valueOf(offset) + notation[0];
					} else if (binop.right instanceof ir.IntegerExp) {
						ir.MemoryExp base = new ir.MemoryExp(binop.left);
						int offset = ((ir.IntegerExp)binop.right).value;
						makeOperand(base, add_inputs, notation);
						if (offset != 0)
							notation[0] = String.valueOf(offset) + notation[0];
					} else
						Error.current.fatalError("Too complicated x86_64 assembler memory address");
				} else if (binop.operation == ir.BinaryOpExp.MINUS) {
					if (binop.right instanceof ir.IntegerExp) {
						ir.MemoryExp base =	new ir.MemoryExp(binop.left);
						int offset = -((ir.IntegerExp)binop.right).value;
						makeOperand(base, add_inputs, notation);
						if (offset != 0)
							notation[0] = String.valueOf(offset) + notation[0];
					} else
						Error.current.fatalError("Too complicated x86_64 assembler memory address");
				} else
					Error.current.fatalError("Too complicated x86_64 assembler memory address");
			} else {
				Error.current.fatalError("Too complicated x86_64 assembler memory address");
			}
		} else {
			Error.current.fatalError("Strange assembler operand");
		}
	}
	
	void placeCallArguments(final LinkedList<ir.Expression> arguments,
			Instructions result) {
		
		int arg_count = 0;
		ListIterator<ir.Expression> p_arg = arguments.listIterator();
		while (p_arg.hasNext()) {
			ir.Expression arg = p_arg.next();
			assert arg instanceof ir.RegisterExp;
			ir.VirtualRegister[] inputs = {((ir.RegisterExp)arg).reg};
			ir.VirtualRegister[] outputs = {machine_registers[paramreg_list[arg_count]]};
			result.add(new Instruction(
				"movq " + Instruction.Input(0) + ", " + Instruction.Output(0),
				inputs, outputs, true));
			arg_count++;
			if (arg_count == paramreg_list.length)
				break;
		}
		if (arg_count == paramreg_list.length) {
			ListIterator<ir.Expression> p_extra_arg =
					arguments.listIterator(arguments.size());
			while (p_extra_arg.previousIndex() > p_arg.previousIndex()) {
				ir.Expression extra_arg = p_extra_arg.previous();
				assert extra_arg instanceof ir.RegisterExp;
				ir.VirtualRegister[] inputs = {((ir.RegisterExp)extra_arg).reg};
				ir.VirtualRegister[] outputs = {};
				result.add(new Instruction(
					"push " + Instruction.Input(0),
					inputs, outputs, false));
			}
		}
	}
	
	void removeCallArguments(final LinkedList<ir.Expression> arguments,
			Instructions result) {
		int stack_arg_count = arguments.size()-6;
		if (stack_arg_count > 0)
	 		result.add(new Instruction("add " +
				String.valueOf(stack_arg_count*8) + ", %rsp"));
	}

	protected void translateExpressionTemplate(final ir.Expression templ,
			final frame.AbstractFrame frame, final ir.VirtualRegister value_storage,
			Instructions result) {
		if (    (templ instanceof ir.IntegerExp) ||
				(templ instanceof ir.LabelAddressExp) ||
				(templ instanceof ir.RegisterExp) ||
				(templ instanceof ir.MemoryExp) ) {
			if (value_storage != null) {
				addInstruction(result, "movq ", templ,
					", " + Instruction.Output(0), value_storage);
				//debug("Simple expression type %d, translated as %s", templ.kind,
				//	result.back().notation);
			}
		} else if (templ instanceof ir.BinaryOpExp) {
			if (value_storage != null) {
				ir.BinaryOpExp bin_op = (ir.BinaryOpExp)templ;
				//debug("Translating binary operation %d translated as:", bin_op.operation);
				switch (bin_op.operation) {
					case ir.BinaryOpExp.PLUS:
						addInstruction(result, "movq ", bin_op.left,
							", " + Instruction.Output(0), value_storage);
						//debug("\t%s", result.back().notation);
						addInstruction(result, "addq ", bin_op.right,
							", " + Instruction.Output(0), value_storage);
						//debug("\t%s", result.back().notation);
						break;
					case ir.BinaryOpExp.MINUS:
						addInstruction(result, "movq ", bin_op.left,
							", " + Instruction.Output(0), value_storage);
						//debug("\t%s", result.back().notation);
						addInstruction(result, "subq ", bin_op.right,
							", " + Instruction.Output(0), value_storage);
						//debug("\t%s", result.back().notation);
						break;
					case ir.BinaryOpExp.MUL:
						addInstruction(result, "movq ", bin_op.left,
							", " + Instruction.Output(0), machine_registers[RAX]);
						//debug("\t%s", result.back().notation);
						if (    (bin_op.right instanceof ir.IntegerExp) ||
								(bin_op.right instanceof ir.LabelAddressExp)) {
							ir.VirtualRegister tmp_reg = IRenvironment.addRegister();
							addInstruction(result, "movq ", bin_op.right,
								", " + Instruction.Output(0), tmp_reg);
							//debug("\t%s", result.back().notation);
							ir.VirtualRegister[] inputs =
								{machine_registers[RAX], tmp_reg};
							ir.VirtualRegister[] outputs =
								{machine_registers[RAX], machine_registers[RDX]};
							result.add(new Instruction(
								"imulq " + Instruction.Input(1),
								inputs, outputs, false));
							//debug("\t%s", result.back().notation);
						} else {
							addInstruction(result, "imulq ", bin_op.right,
								"", machine_registers[RAX],
								machine_registers[RAX], machine_registers[RDX]);
							//debug("\t%s", result.back().notation);
						}
						ir.VirtualRegister[] inputs = {machine_registers[RAX]};
						ir.VirtualRegister[] outputs = {value_storage};
						result.add(new Instruction("movq " +
							Instruction.Input(0) + ", " + Instruction.Output(0),
							inputs, outputs, true));
						//debug("\t%s", result.back().notation);
						break;
					case ir.BinaryOpExp.DIV:
						addInstruction(result, "movq ", bin_op.left,
							", " + Instruction.Output(0), machine_registers[RAX]);
						//debug("\t%s", result.back().notation);
						ir.VirtualRegister[] inputs2 = {machine_registers[RAX]};
						ir.VirtualRegister[] outputs2 =
							{machine_registers[RAX], machine_registers[RDX]};
						result.add(new Instruction("cqo", inputs2, outputs2, false));
						//debug("\t%s", result.back().notation);
						if ((bin_op.right instanceof ir.IntegerExp) ||
							(bin_op.right instanceof ir.LabelAddressExp)) {
							ir.VirtualRegister tmp_reg = IRenvironment.addRegister();
							addInstruction(result, "movq ", bin_op.right,
								", " + Instruction.Output(0), tmp_reg);
							//debug("\t%s", result.back().notation);
							ir.VirtualRegister[] inputs3 =
								{machine_registers[RAX], machine_registers[RDX], tmp_reg};
							result.add(new Instruction(
								"idivq " + Instruction.Input(2),
								inputs3, outputs2, false));
						//debug("\t%s", result.back().notation);
						} else {
							addInstruction(result, "idivq ", bin_op.right,
								"", machine_registers[RAX],
								machine_registers[RAX], machine_registers[RDX]);
							//debug("\t%s", result.back().notation);
						}
						ir.VirtualRegister[] inputs4 = {machine_registers[RAX]};
						ir.VirtualRegister[] outputs4 = {value_storage};
						result.add(new Instruction("movq " +
							Instruction.Input(0) + ", " + Instruction.Output(0),
							inputs4, outputs4,	true));
						//debug("\t%s", result.back().notation);
						break;
					default:
						Error.current.fatalError("X86_64Assembler::translateExpressionTemplate unexpected binary operation");
				}
			}
		} else if (templ instanceof ir.CallExpression) {
			ir.CallExpression call = (ir.CallExpression)templ;
			if (call.callee_parentfp != null) {
				translateExpression(call.callee_parentfp, frame, 
					machine_registers[FPLINK_REG], result);
			}
			placeCallArguments(call.arguments, result);
			assert call.function instanceof ir.LabelAddressExp;
			ir.VirtualRegister[] inputs = {};
			result.add(new Instruction("call " + 
				((ir.LabelAddressExp)call.function).label().getName(),
				inputs, callersave_registers, false));
			removeCallArguments(call.arguments, result);
			
			ir.VirtualRegister[] inputs1 = {machine_registers[RESULT_REG]};
			ir.VirtualRegister[] outputs1 = {value_storage};
			if (value_storage != null)
				result.add(new Instruction("movq " + Instruction.Input(0) +
					", " + Instruction.Output(0), inputs1, outputs1, true));
		} else
			Error.current.fatalError("X86_64Assembler::translateExpressionTemplate unexpected expression kind");
	}
	
	private static final String[] cond_jumps = {
		"je",
		"jne",
		"jl",
		"jle",
		"jg",
		"jge",
		"jb",
		"jbe",
		"ja",
		"jae",
	};

	protected void translateStatementTemplate(final ir.Statement templ,
			Instructions result) {
		if (templ instanceof ir.JumpStatm) {
			ir.JumpStatm jump = (ir.JumpStatm)templ;
			assert jump.dest instanceof ir.LabelAddressExp;
			ir.VirtualRegister[] inputs = {};
			result.add(new Instruction("jmp " +
				((ir.LabelAddressExp)jump.dest).label().getName(),
				inputs, inputs, false, ((ir.LabelAddressExp)jump.dest).labelRef()));
		} else if (templ instanceof ir.CondJumpStatm) {
			ir.CondJumpStatm jump = (ir.CondJumpStatm)templ;
			addInstruction(result, "cmp ", jump.right, ", ", jump.left);
			ir.VirtualRegister[] inputs = {};
			ir.Label[] dest = {null, jump.trueDest()};
			result.add(new Instruction(cond_jumps[jump.comparison] +
				" " + jump.trueDest().getName(), inputs, inputs, false,	dest));
		} else if (templ instanceof ir.MoveStatm) {
			ir.MoveStatm move = (ir.MoveStatm)templ;
			ir.Expression from = move.from;
			if (move.from instanceof ir.BinaryOpExp) {
				assert move.to instanceof ir.RegisterExp;
				ir.MemoryExp fake_addr = new ir.MemoryExp(move.from);
				addInstruction(result, "leaq ", fake_addr, ", "  + Instruction.Output(0),
					((ir.RegisterExp)move.to).reg);
			} else if (move.from instanceof ir.MemoryExp) {
				assert move.to instanceof ir.RegisterExp;
				addInstruction(result, "movq ", move.from, ", " + Instruction.Output(0),
					((ir.RegisterExp)move.to).reg);
			} else {
				if (move.to instanceof ir.RegisterExp)
					addInstruction(result, "movq ", move.from, ", " + Instruction.Output(0),
						((ir.RegisterExp)move.to).reg);
				else {
					assert move.to instanceof ir.MemoryExp;
					Vector<ir.VirtualRegister> registers = new Vector<>();
					String[] operand0 = new String[1];
					String[] operand1 = new String[1];
					makeOperand(move.from, registers, operand0);
					makeOperand(move.to, registers, operand1);
					ir.VirtualRegister[] empty = {};
					result.add(new Instruction("movq " + operand0[0] + ", " +
						operand1[0], registers.toArray(empty), empty, false));
				}
			}
		} else
			Error.current.fatalError("X86_64Assembler::translateStatementTemplate unexpected statement kind");
	}
	
	protected void translateBlob(final ir.Blob blob, Instructions result) {
		result.add(new Instruction(blob.label.getName() + ":"));
		String content = "";
		for (byte c: blob.data)
			content += String.format(".byte 0x%x\n", (int)c);
		result.add(new Instruction(content));
	}
	
	protected Instruction translateLabel(final ir.Label label) {
		return new Instruction(label.getName() + ":");
	}
	
	protected String getCodeSectionHeader() {
		return ".text\n.global main\n";
	}
	
	protected String getBlobSectionHeader() {
		return ".section\t.rodata\n";
	}
	
	protected void functionPrologue_preframe(final frame.AbstractFrame frame,
			Instructions result,
			Vector<ir.VirtualRegister> regs_saving_calleesave) {
		ir.VirtualRegister[] empty = {};
		result.add(new Instruction("# callee-save registers",
			empty, calleesave_registers, false));
		regs_saving_calleesave.setSize(calleesave_list.length);
		for (int i = 0; i < calleesave_list.length; i++) {
			regs_saving_calleesave.set(i, IRenvironment.addRegister());
			ir.VirtualRegister[] inputs = {calleesave_registers[i]};
			ir.VirtualRegister[] outputs = {regs_saving_calleesave.elementAt(i)};
			result.add(new Instruction("movq " + Instruction.Input(0) +
				", " + Instruction.Output(0),
				inputs, outputs, true));
		}
		
		final LinkedList<frame.VarLocation> parameters =
			frame.getParameters();
		int param_index = 0;
		for (frame.VarLocation param: parameters) {
			if (param_index < paramreg_list.length) {
				assert (param.isRegister());
				ir.VirtualRegister storage_reg =
					param.getRegister();
				ir.VirtualRegister[] inputs =
					{machine_registers[paramreg_list[param_index]]};
				ir.VirtualRegister[] outputs = {storage_reg};
				result.add(new Instruction("movq " +
					Instruction.Input(0) + ", " + Instruction.Output(0),
					inputs, outputs, true));
			} else
				assert param.isRegister();
			param_index++;
		}
		frame.VarLocation parent_fp = frame.getParentFpForUs();
		if (parent_fp != null) {
			assert parent_fp.isRegister();
			ir.VirtualRegister storage_reg =
				parent_fp.getRegister();
			ir.VirtualRegister[] inputs = {machine_registers[FPLINK_REG]};
			ir.VirtualRegister[] outputs = {storage_reg};
			result.add(new Instruction("movq " +
				Instruction.Input(0) + ", " + Instruction.Output(0),
				inputs, outputs, true));
		}
	}
	
	protected void functionEpilogue_preframe(final frame.AbstractFrame frame, 
			final ir.VirtualRegister result_storage,
			final Vector<ir.VirtualRegister> regs_saving_calleesave,
			Instructions result) {
		if (result_storage != null) {
			ir.VirtualRegister[] inputs = {result_storage};
			ir.VirtualRegister[] outputs = {machine_registers[RESULT_REG]};
			result.add(new Instruction("movq " + Instruction.Input(0) + ", " +
				Instruction.Output(0), inputs, outputs,
				true));
		}

		assert(regs_saving_calleesave.size() == calleesave_registers.length);
		for (int i = 0; i < calleesave_registers.length; i++) {
			ir.VirtualRegister[] inputs = {regs_saving_calleesave.elementAt(i)};
			ir.VirtualRegister[] outputs = {calleesave_registers[i]};
			result.add(new Instruction("movq " + Instruction.Input(0) +
				", " + Instruction.Output(0),
				inputs, outputs, true));
		}
		ir.VirtualRegister[] empty = {};
		result.add(new Instruction("# callee-save registers",
			calleesave_registers, empty, false));
	}
	
	protected void functionPrologue_postframe(Instructions result) {
	}
	
	protected void functionEpilogue_postframe(Instructions result) {
		result.add(new Instruction("ret"));
	}
	
	protected void programPrologue(Instructions result) {
		result.addFirst(new Instruction("main:"));
	}
	
	protected void programEpilogue(Instructions result) {
		result.add(new Instruction("movq $0, %rax"));
		result.add(new Instruction("ret"));
	}
	
	protected void framePrologue(final frame.AbstractFrame frame,
			Instructions result) {
		int framesize = ((x86_64.Frame)frame).getFrameSize();
		if (framesize > 0)
			result.addFirst(new Instruction("subq $" + String.valueOf(framesize) +
				", %rsp"));
	}
	
	protected void frameEpilogue(final frame.AbstractFrame frame,
		Instructions result) {
		int framesize = ((x86_64.Frame)frame).getFrameSize();
		if (framesize > 0)
			result.add(new Instruction("addq $" + String.valueOf(framesize) +
					", %rsp"));
	}
	
	/**
	 * Replace &iX (X = inputreg_index) with (semantically) &iX + offset
	 */
	private String addOffset(String command, int inputreg_index, int offset)
	{
		String p = Instruction.Input(inputreg_index);
		assert p.length() == 3;
		int pos = command.indexOf(p);
		char[] s = command.toCharArray();
		
		assert((pos > 0) && (pos + p.length() < command.length()));
		assert((s[pos-1] == ' ') || (s[pos-1] == '('));
		String result;
		
		if (s[pos-1] == ' ') {
			assert command.startsWith("movq");
			result = "leaq " + String.valueOf(offset) + "(" + p + ")" +
				command.substring(pos + p.length(), command.length());
		} else {
			assert(s[pos + p.length()] == ')');
			int before_offset = pos;
			while ((before_offset > 0) && (s[before_offset] != ' '))
				before_offset--;
			assert(before_offset > 0);
			int existing_offset = 0;
			if (before_offset < pos-1)
				existing_offset = Integer.valueOf(command.substring(before_offset+1,
					pos-1));
			String offset_s;
			if (existing_offset + offset == 0)
				offset_s = "";
			else
				offset_s = String.valueOf(existing_offset+offset);
			result = command.substring(0, before_offset+1) +
				offset_s + command.substring(pos-1, command.length());
		}
		debug(String.format("%s +%d . %s", command, offset, result));
		return result;
	}

	protected void implementFramePointer(final frame.AbstractFrame frame,
			Instructions result) {
		for (Instruction inst: result) {
			debug(String.format("Inserting frame pointer: %s", inst.notation));
			for (ir.VirtualRegister output: inst.outputs)
				if (output.getIndex() == frame.getFramePointer().getRegister().getIndex())
					Error.current.fatalError("Cannot write to frame pointer");
			for (int i = 0; i < inst.inputs.length; i++)
				if (inst.inputs[i].getIndex() == frame.getFramePointer().getRegister().getIndex()) {
					inst.inputs[i] = machine_registers[RSP];
					inst.notation = addOffset(inst.notation, i,
						((x86_64.Frame)frame).getFrameSize());
					break;
				}
		}
	}
	
	public ir.VirtualRegister[] getAvailableRegisters() {
		return available_registers;
	}
	
	private static int findRegister(String notation, boolean input, int index)
	{
		String part;
		if (input)
			part = Instruction.Input(index);
		else
			part = Instruction.Output(index);
		return notation.indexOf(part);
	}

	private void replaceRegisterUsage(ListIterator<Instruction> p_inst,
			Instruction inst,
			ir.VirtualRegister reg, ir.MemoryExp replacement) {
		for (int i = 0; i < inst.inputs.length; i++)
			if (inst.inputs[i].getIndex() == reg.getIndex()) {
				boolean need_splitting = true;
				final String original = inst.notation;
				if ((inst.outputs.length > 0) &&
						(inst.outputs[0].getIndex() != reg.getIndex()) &&
						(inst.inputs.length == 1)) {
					int pos = findRegister(original, true, 0);
					assert(pos > 0);
					need_splitting = original.charAt(pos-1) == '(';
				}
				if (! need_splitting) {
					String[] storage_notation = new String[1];
					Vector<ir.VirtualRegister> newinputs = new Vector<>();
					makeOperand(replacement, newinputs, storage_notation);
					inst.inputs = newinputs.toArray(inst.inputs);
					int pos = findRegister(original, true, 0);
					String new_command = original.substring(0, pos) +
						storage_notation[0] + original.substring(pos+3,
						original.length());
					debug(String.format("spilling %s -> %s",
						original, new_command));
					inst.notation = new_command;
					inst.is_reg_to_reg_assign = false;
				} else {
					ir.VirtualRegister temp = IRenvironment.addRegister();
					p_inst.previous();
					addInstruction(null, "movq ", replacement, ", " + Instruction.Output(0), temp,
						null, null, p_inst);
					p_inst.next();
					inst.inputs[i] = temp;
				}
				return;
			}
	}
	
	private void replaceRegisterAssignment(ListIterator<Instruction> p_inst,
			Instruction inst,
			ir.VirtualRegister reg, ir.MemoryExp replacement) {
		for (int i = 0; i < inst.outputs.length; i++)
			if (inst.outputs[i].getIndex() == reg.getIndex()) {
				boolean need_splitting;
				final String original = inst.notation;
				if (inst.inputs.length > 1)
					need_splitting = true;
				else if (inst.inputs.length == 0)
					need_splitting = false;
				else {
					int pos = findRegister(original, true, 0);
					assert(pos > 0);
					need_splitting = original.charAt(pos-1) == '(';
				}
				if (! need_splitting) {
					String[] storage_notation = new String[1];
					Vector<ir.VirtualRegister> newinputs = new Vector<>();
					newinputs.setSize(inst.inputs.length);
					for (int ii = 0; ii < inst.inputs.length; ii++)
						newinputs.set(ii, inst.inputs[ii]);
					makeOperand(replacement, newinputs, storage_notation);
					inst.inputs = newinputs.toArray(inst.inputs);
					int pos = findRegister(original, false, 0);
					assert(pos == original.length()-3);
					String new_command = original.substring(0, pos) +
						storage_notation[0];
					debug(String.format("spilling %s -> %s", original, new_command));
					inst.notation = new_command;
					inst.is_reg_to_reg_assign = false;
				} else {
					ir.VirtualRegister temp = IRenvironment.addRegister();
					inst.outputs[i] = temp;

					Vector<ir.VirtualRegister> registers = new Vector<>();
					registers.add(temp);
					String operand0 = Instruction.Input(0);
					String[] operand1 = new String[1];
					makeOperand(replacement, registers, operand1);
					String new_command = "movq " + operand0 + ", " + operand1[0];
					p_inst.add(new Instruction(new_command, registers.toArray(inst.inputs), 
							new ir.VirtualRegister[0], false));
					debug(String.format("spilling %s -> append %s", original,
						new_command));
				}
			}
	}
	
	private boolean isRegisterBeingMovedTo(final Instruction inst,
			ir.VirtualRegister reg, ir.MemoryExp dest) {
		if (! inst.notation.startsWith("movq "))
			return false;
		if ((inst.inputs.length != 2) || (inst.outputs.length != 0) ||
				(inst.inputs[0].getIndex() != reg.getIndex()))
			return false;
		String s = Instruction.Input(0);
		// movq INPUT0, OFF(INPUT1)
		if (! inst.notation.substring(5, 5+s.length()).equals(s))
			return false;
		assert inst.notation.substring(5+s.length(), 5+s.length()+2) == ", ";
		
		int arg1_start = 5+s.length()+2;
		while (inst.notation.charAt(arg1_start) == ' ')
			arg1_start++;
		String arg1 = inst.notation.substring(arg1_start, inst.notation.length());
		int paren = arg1.indexOf('(');
		if (paren < 0)
			return false;
		int inst_offset;
		if (paren == 0)
			inst_offset = 0;
		else
			inst_offset = Integer.valueOf(inst.notation.substring(0, paren));
		int closeparen = arg1.indexOf(')');
		assert closeparen == inst.notation.length()-1;
		assert inst.notation.substring(paren+1, closeparen) == Instruction.Input(1);
		
		int offset = 0;
		ir.VirtualRegister base = null;
		if (dest.address instanceof ir.RegisterExp)
			base = ((ir.RegisterExp)dest.address).reg;
		else if (dest.address instanceof ir.BinaryOpExp) {
			ir.BinaryOpExp binop = (ir.BinaryOpExp)dest.address;
			if ((binop.operation != ir.BinaryOpExp.PLUS) &&
					(binop.operation != ir.BinaryOpExp.MINUS))
				return false;
			if ((binop.left instanceof ir.RegisterExp) && (binop.right instanceof ir.IntegerExp)) {
				base = ((ir.RegisterExp)binop.left).reg;
				offset = ((ir.IntegerExp)binop.right).value;
			} else if ((binop.left instanceof ir.IntegerExp) && (binop.right instanceof ir.RegisterExp)) {
				base = ((ir.RegisterExp)binop.right).reg;
				offset = ((ir.IntegerExp)binop.left).value;
			} else
				return false;
			if (binop.operation == ir.BinaryOpExp.MINUS)
				offset = -offset;
		} else
			return false;
		return ((inst.inputs[1].getIndex() == base.getIndex()) &&
				(inst_offset == offset));
	}
	

	public void spillRegister(frame.AbstractFrame frame, Instructions code,
			final ir.VirtualRegister reg) {
		if (reg.isPrespilled()) {
			VarLocation stored_location = reg.getPrespilledLocation();
			assert stored_location.isPredefined();
			debug("Spilling parent FP parameter register " + reg.getName());
			ir.MemoryExp storage_exp = (ir.MemoryExp)
				stored_location.createCode(stored_location.getOwnerFrame());
			
			boolean seen_usage = false, seen_assignment = false;
			for (ListIterator<Instruction> p_inst = code.listIterator();
					p_inst.hasNext(); ) {
				Instruction inst = p_inst.next();
				boolean is_assigned = false;
				for (ir.VirtualRegister output: inst.outputs)
					if (output.getIndex() == reg.getIndex()) {
						is_assigned = true;
						seen_assignment = true;
						assert(inst.is_reg_to_reg_assign);
						assert(! seen_usage);
						assert(! seen_assignment);
						assert(inst.inputs.length == 1);
						assert(inst.outputs.length == 1);
						String[] storage_notation = new String[1];

						Vector<ir.VirtualRegister> newinputs = new Vector<>();
						newinputs.setSize(inst.inputs.length);
						for (int ii = 0; ii < inst.inputs.length; ii++)
							newinputs.set(ii, inst.inputs[ii]);
						makeOperand(storage_exp, newinputs, storage_notation);
						inst.inputs = newinputs.toArray(inst.inputs);
						
						final String s = inst.notation;
						assert(s.substring(s.length()-3, s.length()) == "&o0");
						String replacement = s.substring(0, s.length()-3) +
							storage_notation[0];
						debug(String.format("Spilling %s -> %s", s,
							  replacement));
						inst.notation = replacement;
						inst.is_reg_to_reg_assign = false;
					}
				
				boolean is_used = false;
				for (ir.VirtualRegister input: inst.inputs)
					if (input.getIndex() == reg.getIndex()) {
						assert(! is_assigned);
						assert(seen_assignment);
						is_used = true;
						seen_usage = true;
						break;
					}
				if (is_used) {
					if (isRegisterBeingMovedTo(inst, reg, storage_exp)) {
						debug("Deleting instruction that moves register " +
								reg.getName() + " to its spilled location");
						p_inst.remove();
					} else
						replaceRegisterUsage(p_inst, inst, reg, storage_exp);
				}
			}
		} else {
			VarLocation stored_location =
				frame.createMemoryVariable(8, ".store." + reg.getName());
			ir.MemoryExp storage_exp = (ir.MemoryExp)
				stored_location.createCode(stored_location.getOwnerFrame());
			
			for (ListIterator<Instruction> p_inst = code.listIterator();
					p_inst.hasNext(); ) {
				Instruction inst = p_inst.next();
				debug(String.format("Spilling: %s", inst.notation));
				boolean is_assigned = false, is_used = false;
				for (ir.VirtualRegister output: inst.outputs)
					if (output.getIndex() == reg.getIndex())
						is_assigned = true;
				for (ir.VirtualRegister input: inst.inputs)
					if (input.getIndex() == reg.getIndex())
						is_used = true;
				if (is_used) {
					if (! is_assigned)
						replaceRegisterUsage(p_inst, inst, reg, storage_exp);
					else {
						ir.VirtualRegister temp = IRenvironment.addRegister();
						p_inst.previous();
						addInstruction(null, "movq ", storage_exp,
							", " + Instruction.Output(0), temp,
							null, null, p_inst);
						p_inst.next();
						for (int i = 0; i < inst.inputs.length; i++)
							if (inst.inputs[i].getIndex() == reg.getIndex())
								inst.inputs[i] = temp;
						replaceRegisterAssignment(p_inst, inst, reg, storage_exp);
					}
				} else if (is_assigned) {
					replaceRegisterAssignment(p_inst, inst, reg, storage_exp);
				}
			}
		}
	}
}
