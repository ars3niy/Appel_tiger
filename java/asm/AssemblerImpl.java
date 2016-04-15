package asm;

import java.util.Vector;
import java.util.LinkedList;
import java.io.PrintWriter;
import error.Error;

public abstract class AssemblerImpl extends util.DebugPrinter implements Assembler {
	protected ir.IREnvironment IRenvironment;
	
	private static final int
		IR_INTEGER = 0,
		IR_LABELADDR = 1,
		IR_REGISTER = 2,
		IR_BINARYOP = 3,
		IR_MEMORY = 4,
		IR_FUN_CALL = 5,
		IR_STAT_EXP_SEQ = 6,
		IR_EXPR_MAX = 7,
		
		IR_MOVE = 0,
		IR_EXP_IGNORE_RESULT = 1,
		IR_JUMP = 2,
		IR_COND_JUMP = 3,
		IR_STAT_SEQ = 4,
		IR_LABEL = 5,
		IR_STAT_MAX = 6;

	private TemplateStorage[] expr_templates = new TemplateStorage[IR_EXPR_MAX];
	private TemplateStorage[] statm_templates = new TemplateStorage[IR_STAT_MAX];
	
	private TemplateStorage getTemplatesStorage(final ir.Expression expr) {
		if (expr instanceof ir.IntegerExp)
			return expr_templates[IR_INTEGER];
		else if (expr instanceof ir.LabelAddressExp)
			return expr_templates[IR_LABELADDR];
		else if (expr instanceof ir.RegisterExp)
			return expr_templates[IR_REGISTER];
		else if (expr instanceof ir.BinaryOpExp)
			return expr_templates[IR_BINARYOP];
		else if (expr instanceof ir.MemoryExp)
			return expr_templates[IR_MEMORY];
		else if (expr instanceof ir.CallExpression)
			return expr_templates[IR_FUN_CALL];
		else
			return null;
	}
	
	private TemplateStorage getTemplatesStorage(final ir.Statement statm) {
		if (statm instanceof ir.MoveStatm)
			return statm_templates[IR_MOVE];
		else if (statm instanceof ir.JumpStatm)
			return statm_templates[IR_JUMP];
		else if (statm instanceof ir.CondJumpStatm)
			return statm_templates[IR_COND_JUMP];
		else if (statm instanceof ir.LabelPlacementStatm)
			return statm_templates[IR_LABEL];
		else
			return null;
	}
	
	private LinkedList<InstructionTemplate> getTemplatesList(ir.Expression expr) {
		TemplateStorage storage = getTemplatesStorage(expr);
		if (expr instanceof ir.BinaryOpExp)
			return ((TemplateOpMap)storage).getList(((ir.BinaryOpExp)expr).operation);
		else
			return ((TemplateList)storage).list;
	}
	
	private LinkedList<InstructionTemplate> getTemplatesList(ir.Statement statm) {
		TemplateStorage storage = getTemplatesStorage(statm);
		if (statm instanceof ir.CondJumpStatm)
			return ((TemplateOpMap)storage).getList(((ir.CondJumpStatm)statm).comparison);
		else
			return ((TemplateList)storage).list;
	}
	
	protected void addTemplate(final ir.Expression expr) {
		getTemplatesList(expr).add(new InstructionTemplate(expr));
	}
	
	protected void addTemplate(final ir.Statement statm) {
		getTemplatesList(statm).add(new InstructionTemplate(statm));
	}
	
	protected abstract void translateExpressionTemplate(final ir.Expression templ,
		final frame.AbstractFrame frame, final ir.VirtualRegister result_storage,
		Instructions result);
	protected abstract void translateStatementTemplate(final ir.Statement templ,
		Instructions result);
	protected abstract void translateBlob(final ir.Blob blob, Instructions result);
	protected abstract Instruction translateLabel(final ir.Label label);
	
	protected abstract String getCodeSectionHeader();
	protected abstract String getBlobSectionHeader();
	protected abstract void functionPrologue_preframe(final frame.AbstractFrame frame,
		Instructions result,
		Vector<ir.VirtualRegister> regs_saving_calleesave);
	protected abstract void functionEpilogue_preframe(final frame.AbstractFrame frame, 
		final ir.VirtualRegister result_storage,
		final Vector<ir.VirtualRegister> regs_saving_calleesave,
		Instructions result);
	protected abstract void functionPrologue_postframe(Instructions result);
	protected abstract void functionEpilogue_postframe(Instructions result);
	protected abstract void programPrologue(Instructions result);
	protected abstract void programEpilogue(Instructions result);
	protected abstract void framePrologue(final frame.AbstractFrame frame,
		Instructions result);
	protected abstract void frameEpilogue(final frame.AbstractFrame frame,
		Instructions result);
	protected abstract void implementFramePointer(final frame.AbstractFrame frame,
		Instructions result);

	private class TemplateChildInfo {
		ir.VirtualRegister value_storage;
		final ir.Expression expression;
		
		TemplateChildInfo(final ir.VirtualRegister _value, final ir.Expression _expr) {
			value_storage = _value;
			expression = _expr;
		}
	};
	
	private InstructionTemplate FindExpressionTemplate(final ir.Expression expression) {
		LinkedList<InstructionTemplate>templates = getTemplatesList(expression);
		int best_nodecount = -1;
		InstructionTemplate templ = null;
		for (InstructionTemplate cur_templ:templates) {
			assert cur_templ.code instanceof ir.ExpCode;
			ir.Expression templ_exp = ((ir.ExpCode)cur_templ.code).exp;
			int[] nodecount = {0};
			if (MatchExpression(expression, templ_exp, nodecount)) {
				if (nodecount[0] > best_nodecount) {
					best_nodecount = nodecount[0];
					templ = cur_templ;
				}
			}
		}
		return templ;
	}
	
	private InstructionTemplate FindStatementTemplate(final ir.Statement statement) {
		LinkedList<InstructionTemplate>templates = getTemplatesList(statement);
		int best_nodecount = -1;
		InstructionTemplate templ = null;

		for (InstructionTemplate cur_templ:templates) {
			assert (cur_templ.code instanceof ir.StatmCode) &&
				! (cur_templ.code instanceof ir.CondJumpPatchesCode);
			ir.Statement templ_statm = ((ir.StatmCode)cur_templ.code).statm;
			int[] nodecount = {0};
			if (MatchStatement(statement, templ_statm, nodecount)) {
				if (nodecount[0] > best_nodecount) {
					best_nodecount = nodecount[0];
					templ = cur_templ;
				}
			}
		}
		return templ;
	}
	
	private boolean MatchExpression(final ir.Expression expression, final ir.Expression templ,
			int[] nodecount) {
		return MatchExpression(expression, templ, nodecount, null, null);
	}
	
	private boolean MatchExpression(final ir.Expression expression, final ir.Expression templ,
			int[] nodecount,
			LinkedList<TemplateChildInfo> children,
			ir.Expression[] template_instantiation) {
		if ((templ instanceof ir.RegisterExp) && !(expression instanceof ir.RegisterExp)) {
			// Matched register template against non-register expression
			// Non-register expression gets computed separately and
			// "glued" to the template
			if (children != null) {
				children.add(new TemplateChildInfo(null, expression));
				if (template_instantiation != null) {
					ir.VirtualRegister value_storage = IRenvironment.addRegister();
					children.getLast().value_storage = value_storage;
					template_instantiation[0] = new ir.RegisterExp(value_storage);
				}
			}
			return true;
		}
		
		//if (templ.kind != expression.kind)
		//	return false;
		
		nodecount[0]++;
		assert !(templ instanceof ir.StatExpSequence);
		
		if (templ instanceof ir.IntegerExp) {
			if (! (expression instanceof ir.IntegerExp))
				return false;
			if (template_instantiation != null)
				template_instantiation[0] = new ir.IntegerExp(
					((ir.IntegerExp)expression).value);
			return (((ir.IntegerExp)templ).value == 0) ||
				(((ir.IntegerExp)templ).value ==
				 ((ir.IntegerExp)expression).value);
		} else if (templ instanceof ir.LabelAddressExp) {
			if (! (expression instanceof ir.LabelAddressExp))
				return false;
			if (template_instantiation != null)
				template_instantiation[0] = new ir.LabelAddressExp(
					((ir.LabelAddressExp)expression).label());
			return true;
		} else if (templ instanceof ir.RegisterExp) {
			if (! (expression instanceof ir.RegisterExp))
				return false;
			if (template_instantiation != null)
				template_instantiation[0] = new ir.RegisterExp(
					((ir.RegisterExp)expression).reg);
			return true;
		} else if (templ instanceof ir.CallExpression) {
			if (! (expression instanceof ir.CallExpression))
				return false;
			ir.CallExpression call_expr = (ir.CallExpression)expression;
			ir.CallExpression call_inst = null;
			ir.Expression[] func_inst = null;
			if (template_instantiation != null) {
				call_inst = new ir.CallExpression(null, call_expr.callee_parentfp);
				func_inst = new ir.Expression[1];
				template_instantiation[0] = call_inst;
			}
			if (children != null) {
				for (ir.Expression arg: call_expr.arguments) {
					children.add(new TemplateChildInfo(null, arg));
					if (call_inst != null) {
						ir.VirtualRegister arg_reg = IRenvironment.addRegister();
						children.getLast().value_storage = arg_reg;
						call_inst.addArgument(new ir.RegisterExp(arg_reg));
					}
				}
			}
			boolean res = MatchExpression(
				call_expr.function, ((ir.CallExpression)templ).function,
				nodecount, children, func_inst);
			if (call_inst != null)
				call_inst.function = func_inst[0];
			return res;
		} else if (templ instanceof ir.BinaryOpExp) {
			if (! (expression instanceof ir.BinaryOpExp))
				return false;
			ir.BinaryOpExp binop_expr = (ir.BinaryOpExp)expression;
			ir.BinaryOpExp binop_templ = (ir.BinaryOpExp)templ;
			
			if (binop_expr.operation != binop_templ.operation)
				return false;
			ir.Expression[] left_inst = null, right_inst = null;
			ir.BinaryOpExp binop_inst = null;
			if (template_instantiation != null) {
				binop_inst = new ir.BinaryOpExp(binop_expr.operation, null, null);
				template_instantiation[0] = binop_inst;
				left_inst = new ir.Expression[1];
				right_inst = new ir.Expression[1];
			}
			boolean res = MatchExpression(binop_expr.left, binop_templ.left,
					nodecount, children, left_inst) && 
				MatchExpression(binop_expr.right, binop_templ.right,
					nodecount, children, right_inst);
			if (binop_inst != null) {
				binop_inst.left = left_inst[0];
				binop_inst.right = right_inst[0];
			}
			return res;
		} else if (templ instanceof ir.MemoryExp) {
			if (! (expression instanceof ir.MemoryExp))
				return false;
			ir.Expression[] addr_inst = null;
			ir.MemoryExp mem_inst = null;
			if (template_instantiation != null) {
				mem_inst = new ir.MemoryExp(null);
				template_instantiation[0] = mem_inst;
				addr_inst = new ir.Expression[1];
			}
			boolean res = MatchExpression(((ir.MemoryExp)expression).address,
				((ir.MemoryExp)templ).address, nodecount, children, addr_inst);
			if (mem_inst != null)
				mem_inst.address = addr_inst[0];
			return res;
		} else {
			Error.current.fatalError("Strange expression template kind");
			return false;
		}
	}

	private boolean MatchMoveDestination(final ir.Expression expression, final ir.Expression templ,
			int[] nodecount,
			LinkedList<TemplateChildInfo> children,
			ir.Expression[] template_instantiation) {
		if ((templ instanceof ir.RegisterExp) & !(expression instanceof ir.RegisterExp))
			return false;
		return MatchExpression(expression, templ, nodecount, children, template_instantiation);
	}
	
	private boolean MatchStatement(final ir.Statement statement, final ir.Statement templ,
			int[] nodecount) {
		return MatchStatement(statement, templ, nodecount, null, null);
	}
	
	private boolean MatchStatement(final ir.Statement statement, final ir.Statement templ,
			int[] nodecount, LinkedList<TemplateChildInfo> children,
			ir.Statement[] template_instantiation) {
		
		nodecount[0]++;
		
		if (statement instanceof ir.MoveStatm) {
			assert templ instanceof ir.MoveStatm;
			ir.MoveStatm move_statm = (ir.MoveStatm)statement;
			ir.MoveStatm move_templ = (ir.MoveStatm)templ;
			ir.Expression[] to_inst = null, from_inst = null;
			ir.MoveStatm move_inst = null;
			if (template_instantiation != null) {
				move_inst = new ir.MoveStatm(null, null);
				template_instantiation[0] = move_inst;
				to_inst = new ir.Expression[1];
				from_inst = new ir.Expression[1];
			}
			boolean res = MatchMoveDestination(move_statm.to, move_templ.to, 
					nodecount, children, to_inst) &
				MatchExpression(move_statm.from, move_templ.from, 
					nodecount, children, from_inst);
			if (move_inst != null) {
				move_inst.to = to_inst[0];
				move_inst.from = from_inst[0];
			}
			return res;
		} else if (statement instanceof ir.JumpStatm) {
			assert templ instanceof ir.JumpStatm;
			ir.Expression[] dest_inst = null;
			ir.JumpStatm jump_inst = null;
			if (template_instantiation != null) {
				jump_inst =	new ir.JumpStatm((ir.Expression)null);
				template_instantiation[0] = jump_inst;
				dest_inst = new ir.Expression[1];
			}
			boolean res = MatchExpression(((ir.JumpStatm)statement).dest,
				((ir.JumpStatm)templ).dest, nodecount, children, dest_inst);
			if (jump_inst != null)
				jump_inst.dest = dest_inst[0];
			return res;
		} else if (statement instanceof ir.CondJumpStatm) {
			assert templ instanceof ir.CondJumpStatm;
			ir.CondJumpStatm cj_statm = (ir.CondJumpStatm)statement;
			ir.CondJumpStatm cj_templ = (ir.CondJumpStatm)templ;
			
			// getTemplatesList takes care of this
			assert (cj_statm.comparison == cj_templ.comparison);

			ir.Expression[] left_inst = null, right_inst = null;
			ir.CondJumpStatm cj_inst = null;
			if (template_instantiation != null) {
				cj_inst = new ir.CondJumpStatm(cj_statm.comparison, null, null,
					cj_statm.trueDest(), cj_statm.falseDest());
				template_instantiation[0] = cj_inst;
				left_inst = new ir.Expression[1];
				right_inst = new ir.Expression[1];
			}
			boolean res = MatchExpression(cj_statm.left, cj_templ.left, 
					nodecount, children, left_inst) &
				MatchExpression(cj_statm.right, cj_templ.right, 
					nodecount, children, right_inst);
			if (cj_inst != null) {
				cj_inst.left = left_inst[0];
				cj_inst.right = right_inst[0];
			}
			return res;
		} else {
			Error.current.fatalError("Strange statement template kind");
			return false;
		}
	}

	protected void translateExpression(final ir.Expression expression,
			final frame.AbstractFrame frame, final ir.VirtualRegister value_storage,
			Instructions result) {
		if (expression instanceof ir.StatExpSequence) {
			ir.StatExpSequence statexp = (ir.StatExpSequence)expression;
			translateStatement(statexp.stat, frame, result);
			translateExpression(statexp.exp, frame, value_storage, result);
		} else {
			InstructionTemplate templ =	FindExpressionTemplate(expression);
			if (templ == null)
				Error.current.fatalError("Failed to find expression template");
			
			LinkedList<TemplateChildInfo> children = new LinkedList<>();
			ir.Expression[] template_instantiation = new ir.Expression[1];
			assert (templ.code instanceof ir.ExpCode);
			int nodecount[] = {0};
			boolean success = MatchExpression(expression,
				((ir.ExpCode)templ.code).exp,
				nodecount, children, template_instantiation);
			assert success;
			for (TemplateChildInfo child: children)
				translateExpression(child.expression, frame, child.value_storage, result);
			translateExpressionTemplate(template_instantiation[0], frame,
				value_storage, result);
		}
	}
	
	protected void translateStatement(final ir.Statement statement,
			final frame.AbstractFrame frame, Instructions result) {
		if (statement instanceof ir.StatmSequence) {
			ir.StatmSequence seq = (ir.StatmSequence)statement;
			for (ir.Statement statm: seq.statements)
				 translateStatement(statm, frame, result);
		} else if (statement instanceof ir.ExpressionStatm) {
			translateExpression(((ir.ExpressionStatm)statement).exp, 
				frame, null, result);
		} else if (statement instanceof ir.LabelPlacementStatm) {
			result.add(translateLabel(((ir.LabelPlacementStatm)statement).label));
			result.getLast().label = ((ir.LabelPlacementStatm)statement).label;
		} else {
			InstructionTemplate templ = FindStatementTemplate(statement);
			if (templ == null)
				Error.current.fatalError("Failed to find statement template");
			
			LinkedList<TemplateChildInfo> children = new LinkedList<>();
			ir.Statement[] template_instantiation = new ir.Statement[1];
			
			assert (templ.code instanceof ir.StatmCode);
			int[] nodecount = {0};
			boolean success = MatchStatement(statement,
				((ir.StatmCode)templ.code).statm,
				nodecount, children, template_instantiation);
			assert success;
			for (TemplateChildInfo child: children) {
				translateExpression(child.expression, frame, child.value_storage,
					result);
			}
			translateStatementTemplate(template_instantiation[0], result);
		}
	}

	public AssemblerImpl(ir.IREnvironment ir_env) {
		super("assembler.log");
		IRenvironment = ir_env;

		expr_templates = new TemplateStorage[IR_EXPR_MAX];
		for (int i = 0; i < IR_EXPR_MAX; i++) {
			if (i == IR_BINARYOP)
				expr_templates[i] = new TemplateOpMap(ir.BinaryOpExp.MAX);
			else
				expr_templates[i] = new TemplateList();
		}
		
		statm_templates = new TemplateStorage[IR_STAT_MAX];
		for (int i = 0; i < IR_STAT_MAX; i++) {
			if (i == IR_COND_JUMP)
				statm_templates[i] = new TemplateOpMap(ir.CondJumpStatm.COMPMAX);
			else
				statm_templates[i] = new TemplateList();
		}
	}
	
	public void translateFunction(ir.Code code, frame.AbstractFrame frame,
			Instructions result) {
		if (code == null)
			// External function, ignore
			return;
		if (code instanceof ir.ExpCode) {
			ir.VirtualRegister result_storage = IRenvironment.addRegister();
			Vector<ir.VirtualRegister> prologue_regs = new Vector<>();
			functionPrologue_preframe(frame, result, prologue_regs);
			translateExpression(((ir.ExpCode)code).exp,
				frame, result_storage, result);
			functionEpilogue_preframe(frame, result_storage, prologue_regs, result);
		} else if ((code instanceof ir.StatmCode) && !(code instanceof ir.CondJumpPatchesCode)) {
			Vector<ir.VirtualRegister> prologue_regs = new Vector<>();
			functionPrologue_preframe(frame, result, prologue_regs);
			translateStatement(((ir.StatmCode)code).statm, frame, result);
			functionEpilogue_preframe(frame, null, prologue_regs, result);
		} else {
			Error.current.fatalError("Assembler::translateFunctionBody strange body code");
		}
	}
	
	public void implementFrameSize(frame.AbstractFrame frame,
			Instructions result) {
		framePrologue(frame, result);
		frameEpilogue(frame, result);
		implementFramePointer(frame, result);
	}
	
	public void translateProgram(ir.Code code,
			frame.AbstractFrame frame, Instructions result) {
		translateStatement(IRenvironment.codeToStatement(code), frame, result);
	}
	
	public void finishFunction(ir.Label label, Instructions result) {
		functionPrologue_postframe(result);
		functionEpilogue_postframe(result);
		result.addFirst(translateLabel(label));
	}
	
	public void finishProgram(Instructions result) {
		programPrologue(result);
		programEpilogue(result);
	}
	
	private static ir.VirtualRegister MapRegister(ir.RegisterMap map,
			ir.VirtualRegister reg) {
		if (map == null)
			return reg;
		else
			return map.get(reg);
	}
		
	public void outputCode(PrintWriter output,
			final LinkedList<Instructions> code,
			final ir.RegisterMap regmap) {
		String header = getCodeSectionHeader();
		output.print(header);
		if ((header.length() > 0) && (header.charAt(header.length()-1) != '\n'))
			output.println();
		
		for (final Instructions chunk: code) {
			int line = -1;
			for (final Instruction inst: chunk) {
				line++;
				final String s = inst.notation;
				
				if (inst.is_reg_to_reg_assign) {
					assert(inst.inputs.length == 1);
					assert(inst.outputs.length == 1);
					if (MapRegister(regmap, inst.inputs[0]).getIndex() ==
							MapRegister(regmap, inst.outputs[0]).getIndex())
						continue;
				}
				int len = 0;
				if ((s.length() > 0) & (s.charAt(s.length()-1) != ':')) {
					output.print("    ");
					len += 4;
				}
			
				int i = 0;
				while (i < s.length()) {
					if (s.charAt(i) != '&') {
						output.print(s.charAt(i));
						len++;
						i++;
					} else {
						i++;
						if (i+1 < s.length()) {
							int arg_index = s.charAt(i+1) - '0';
							ir.VirtualRegister[] registers;
							
							if (s.charAt(i) == 'i')
								registers = inst.inputs;
							else if (s.charAt(i) == 'o')
								registers = inst.outputs;
							else {
								Error.current.fatalError("Misformed instruction");
								break;
							}
							
							if (arg_index > registers.length)
								Error.current.fatalError("Misformed instruction");
							else {
								ir.VirtualRegister reg =
										MapRegister(regmap, registers[arg_index]);
								output.print(reg.getName());
								len += reg.getName().length();
							}
							i += 2;
						}
					}
				}
				for (i = 0; i < 35-len; i++)
					output.print(' ');
				output.printf(" # %2d ", line);
				if (inst.is_reg_to_reg_assign)
					output.print("MOVE ");
				boolean use_reg = false;
				if (inst.inputs.length > 0) {
					use_reg = true;
					output.print("<- ");
					for (i = 0; i < inst.inputs.length; i++)
						output.printf("%s ", inst.inputs[i].getName());
				}
				if (inst.outputs.length > 0) {
					use_reg = true;
					output.print("-> ");
					for (i = 0; i < inst.outputs.length; i++)
						output.printf("%s ", inst.outputs[i].getName());
				}
				if (! use_reg)
					output.print("no register use");
				output.println();
			}
		}
	}
	
	public void outputBlobs(PrintWriter output, final LinkedList<ir.Blob> blobs) {
		Instructions content = new Instructions();
		for (final ir.Blob blob: blobs)
			translateBlob(blob, content);
		
		String header =	getBlobSectionHeader();
		output.print(header);
		if ((header.length() > 0) & (header.charAt(header.length()-1) != '\n'))
			output.println();
		
		for (Instruction inst: content)
			output.println(inst.notation);
	}
	
	/**
	 * Frame pointer register must NOT be one of them
	 */
	public abstract ir.VirtualRegister[] getAvailableRegisters();
	
	public abstract void spillRegister(frame.AbstractFrame frame, Instructions code,
		final ir.VirtualRegister reg);
}
