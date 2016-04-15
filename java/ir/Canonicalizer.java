package ir;

import java.util.LinkedList;
import java.util.ListIterator;
import error.Error;
import java.util.TreeMap;

public class Canonicalizer extends util.DebugPrinter {
	private IREnvironment IRenv;
	
	private static boolean isNop(Statement statm)
	{
		return (statm instanceof ExpressionStatm) && (
			(((ExpressionStatm)statm).exp instanceof IntegerExp) ||
			(((ExpressionStatm)statm).exp instanceof LabelAddressExp));
	}

	private static boolean canSwapExpAndStatement(Expression exp, Statement statm)
	{
		return (exp instanceof IntegerExp) || (exp instanceof LabelAddressExp) || isNop(statm);
	}

	private static boolean canSwapExps(Expression exp1, Expression exp2)
	{
		return (exp1 instanceof IntegerExp) || (exp2 instanceof LabelAddressExp) ||
			(exp2 instanceof IntegerExp) || (exp2 instanceof LabelAddressExp);
	}

	private Expression canonicalizeMemoryExp(MemoryExp mem_exp) {
		if (mem_exp.address instanceof StatExpSequence) {
			StatExpSequence stat_exp = (StatExpSequence)mem_exp.address;
			mem_exp.address = stat_exp.exp;
			stat_exp.exp = mem_exp;
			return stat_exp;
		} else
			return mem_exp;
	}

	private static void growStatementSequence(StatmSequence sequence, Statement statement)
	{
		if (statement instanceof StatmSequence) {
			for (Statement newstatm: ((StatmSequence)statement).statements)
				sequence.addStatement(newstatm);
		} else
			sequence.addStatement(statement);
	}

	private class DoubleOperand {
		public StatmSequence statements;
		public Expression left,  right;
	}
	
	private DoubleOperand pullStatementsOutOfTwoOperands(Expression left,
			Expression right) {
		StatmSequence pre_statements = new StatmSequence();
		
		if (left instanceof StatExpSequence) {
			StatExpSequence stat_exp = (StatExpSequence)left;
			
			// stat_exp.exp cannot be IR_STAT_EXP_SEQ because
			// canonicalize that has been called on left earlier
			// gets rid of nested IR_STAT_EXP_SEQ
			assert !(stat_exp.exp instanceof StatExpSequence);
			left = stat_exp.exp;
			growStatementSequence(pre_statements, stat_exp.stat);
		}

		if (right instanceof StatExpSequence) {
			StatExpSequence stat_exp = (StatExpSequence)right;
			if (canSwapExpAndStatement(left, stat_exp.stat)) {
				right = stat_exp.exp;
				growStatementSequence(pre_statements, stat_exp.stat);
			} else {
				VirtualRegister save_left = IRenv.addRegister();
				pre_statements.addStatement(new MoveStatm(
					new RegisterExp(save_left), left));
				growStatementSequence(pre_statements, stat_exp.stat);
				left = new RegisterExp(save_left);
				// left is free of IR_STAT_EXP_SEQ (see above)
				// and so is stat_exp.exp for the same reason
				assert !(stat_exp.exp instanceof StatExpSequence);
				right = stat_exp.exp;
			}
		}

		DoubleOperand result = new DoubleOperand();
		result.statements = pre_statements;
		result.left = left;
		result.right = right;
		return result;
	}
	
	private Expression canonicalizeBinaryOpExp(BinaryOpExp op_exp) {
		DoubleOperand r = pullStatementsOutOfTwoOperands(op_exp.left,
			op_exp.right);
		op_exp.left = r.left;
		op_exp.right = r.right;
		if (! r.statements.statements.isEmpty())
			return new StatExpSequence(r.statements, op_exp);
		else
			return op_exp;
	}

	private Expression canonicalizeCallExp(CallExpression call_exp,
			Expression parentExpression, Statement parentStatement) {
		debug(String.format("Canonicalize call func=%s parent exp=%s parent statm=%s",
			((LabelAddressExp)call_exp.function).label().getName(),
			(parentExpression != null) ? "something" : "null",
			(parentStatement != null) ? "something" : "null"));
		StatmSequence pre_statements = new StatmSequence();
		boolean[] arg_saved = new boolean[call_exp.arguments.size()];
		for (int i = 0; i < arg_saved.length; i++)
			arg_saved[i] = false;
		
		for (ListIterator<Expression> parg = call_exp.arguments.listIterator();
				parg.hasNext(); ) {
			Expression arg = parg.next();
			if (arg instanceof StatExpSequence) {
				StatExpSequence stat_exp = (StatExpSequence)arg;
				int prev_index = 0;
				for (ListIterator<Expression> p_prev_arg = call_exp.arguments.listIterator();
						p_prev_arg.nextIndex() < parg.previousIndex(); ) {
					Expression prev_arg = p_prev_arg.next();
					if (! arg_saved[prev_index] &&
							! canSwapExpAndStatement(prev_arg, stat_exp.stat)) {
						// Cannot move the statement past this expression, need to
						// save the expression with implies pulling the expression
						// past all previously unsaved expressions
						
						// The statement from arg is non-trivial since we
						// have failed to move it past something. 
						// However, we have previously tried to the statement
						// past all unsaved expressions to the left of pre_arg
						// and succeeded. With the current conservative "can move
						// statement past" algorithm, it means that all unsaved 
						// expressions to the left of pre_arg are trivial and we
						// can move pre_arg past them
						int pre_prev_index = 0;
						for (ListIterator<Expression> p_pre_prev_arg =
								call_exp.arguments.listIterator();
								p_pre_prev_arg.nextIndex() < p_prev_arg.previousIndex(); ) {
							Expression pre_prev_arg = p_pre_prev_arg.next();
							if (! arg_saved[pre_prev_index])
								assert canSwapExps(pre_prev_arg, prev_arg);
							pre_prev_index++;
						}
						
						// Now finally save the argument
						arg_saved[prev_index] = true;
						VirtualRegister save_arg = IRenv.addRegister();
						pre_statements.addStatement(new MoveStatm(
							new RegisterExp(save_arg), prev_arg));
						p_prev_arg.set(new RegisterExp(save_arg));
					}
					prev_index++;
				}
				growStatementSequence(pre_statements, stat_exp.stat);
				// stat_exp.exp and thus the new argument expression is free from
				// IR_STAT_EXP_SEQ because recursize call to canonicalizeExpression
				// has got rid of nested IR_STAT_EXP_SEQ before calling us
				parg.set(stat_exp.exp);
			}
		}
		
		Expression transformed_call_exp;
		// Unless we are a child of statement "ignore expression result" or
		// "Move function result to a constant destination" or child of nothing,
		// replace call with moving return value to a register plus presenting
		// the register
		if (
			((parentStatement == null) && (parentExpression == null)) ||
			((parentStatement != null) && (
				(parentStatement instanceof ExpressionStatm) ||
				((parentStatement instanceof MoveStatm) && (
					(
						((MoveStatm)parentStatement).to instanceof RegisterExp
					) ||
					( 
						(
							((MoveStatm)parentStatement).to instanceof MemoryExp
						) &&
						(
							(
								(MemoryExp) 
								((MoveStatm)parentStatement).to
							)
							.address instanceof LabelAddressExp 
						)
					)
				))
			))
		) {
			// Already a child of acceptable parent
			transformed_call_exp = call_exp;
		} else {
			debug("Transforming call into move statement");
			VirtualRegister save_result = IRenv.addRegister();
			pre_statements.addStatement(new MoveStatm(
				new RegisterExp(save_result), call_exp
			));
			transformed_call_exp = new RegisterExp(save_result);
		}
			
		if (! pre_statements.statements.isEmpty())
			return new StatExpSequence(pre_statements, transformed_call_exp);
		else
			return transformed_call_exp;
	}

	private void combineStatExpSequences(StatExpSequence exp) {
		if (exp.exp instanceof StatExpSequence) {
			StatExpSequence child_stat_exp = (StatExpSequence)exp.exp;
			assert !(child_stat_exp.exp instanceof StatExpSequence);
			if (!(exp.stat instanceof StatmSequence)) {
				StatmSequence stat_sequence = new StatmSequence();
				stat_sequence.addStatement(exp.stat);
				exp.stat = stat_sequence;
			}
			
			growStatementSequence((StatmSequence)exp.stat,
				child_stat_exp.stat);
			
			exp.exp = child_stat_exp.exp;
		}
	}
	
	private Statement canonicalizeMoveStatement(MoveStatm move_statm) {
		if (move_statm.to instanceof MemoryExp) {
			DoubleOperand r =
				pullStatementsOutOfTwoOperands(((MemoryExp)move_statm.to).address,
					move_statm.from);
			((MemoryExp)move_statm.to).address = r.left;
			move_statm.from = r.right;
			if (! r.statements.statements.isEmpty()) {
				r.statements.addStatement(move_statm);
				return r.statements;
			} else
				return move_statm;
		} else if (move_statm.to instanceof RegisterExp) {
			if (move_statm.from instanceof StatExpSequence) {
				StatmSequence sequence = new StatmSequence();
				StatExpSequence stat_exp_seq = (StatExpSequence)move_statm.from;
				growStatementSequence(sequence, stat_exp_seq.stat);
				move_statm.from = stat_exp_seq.exp;
				growStatementSequence(sequence, move_statm);
				return sequence;
			} else
				return move_statm;
		} else {
			Error.current.fatalError("canonicalizeMoveStatement: strange destination");
			return move_statm;
		}
	}

	private Statement canonicalizeExpressionStatement(ExpressionStatm exp_statement) {
		if (exp_statement.exp instanceof StatExpSequence) {
			StatmSequence sequence = new StatmSequence();
			StatExpSequence stat_exp_seq = (StatExpSequence)exp_statement.exp;
			growStatementSequence(sequence, stat_exp_seq.stat);
			exp_statement.exp = stat_exp_seq.exp;
			growStatementSequence(sequence, exp_statement);
			return sequence;
		} else
			return exp_statement;
	}

	private Statement canonicalizeJumpStatement(JumpStatm jump_statement) {
		if (jump_statement.dest instanceof StatExpSequence) {
			StatmSequence sequence = new StatmSequence();
			StatExpSequence stat_exp_seq = (StatExpSequence)jump_statement.dest;
			growStatementSequence(sequence, stat_exp_seq.stat);
			jump_statement.dest = stat_exp_seq.exp;
			growStatementSequence(sequence, jump_statement);
			return sequence;
		} else
			return jump_statement;
	}

	private Statement canonicalizeCondJumpStatement(CondJumpStatm cjump_statm) {
		DoubleOperand r = pullStatementsOutOfTwoOperands(cjump_statm.left,
			cjump_statm.right);
		cjump_statm.left = r.left;
		cjump_statm.right = r.right;
		
		if (! r.statements.statements.isEmpty()) {
			r.statements.addStatement(cjump_statm);
			return r.statements;
		} else
			return cjump_statm;
	}

	private void canonicalizeStatmSequence(StatmSequence seq) {
		ListIterator<Statement> child = seq.statements.listIterator();
		while (child.hasNext()) {
			Statement statm = child.next();
			Statement canonical = canonicalizeStatement(statm);
			child.set(canonical);
			if (canonical instanceof StatmSequence) {
				StatmSequence subsequence = (StatmSequence)canonical;
				for (Statement subitem: subsequence.statements) {
					assert !(subitem instanceof StatmSequence);
					child.add(subitem);
				}
			}
		}
		child = seq.statements.listIterator();
		while (child.hasNext()) {
			if (child.next() instanceof StatmSequence)
				child.remove();
		}
	}

	private class StatementBlock {
		LinkedList<Statement> statements = new LinkedList<>();
		Label start_label;
		boolean used_in_trace;
	};
	
	private class BlockSequence {
		LinkedList<StatementBlock> blocks = new LinkedList<>();
		Label finish_label;
	};
	
	private BlockSequence splitToBlocks(StatmSequence sequence) {
		BlockSequence blocks = new BlockSequence();
		
		ListIterator<Statement> p_statm = sequence.statements.listIterator();
		blocks.finish_label = null;
		while (p_statm.hasNext()) {
			Statement statm = p_statm.next();
			blocks.blocks.add(new StatementBlock());
			StatementBlock new_block = blocks.blocks.getLast();
			new_block.used_in_trace = false;
			if (statm instanceof LabelPlacementStatm) {
				new_block.start_label = ((LabelPlacementStatm)statm).label;
				new_block.statements.add(statm);
				debug("Block starting with label " + new_block.start_label.getName());
			} else {
				p_statm.previous();
				new_block.start_label = IRenv.addLabel();
				LabelPlacementStatm label_statm =
					new LabelPlacementStatm(new_block.start_label);
				p_statm.add(label_statm);
				new_block.statements.add(label_statm);
				debug("Block starting with non-label, adding label " + new_block.start_label.getName());
			}
			while (p_statm.hasNext()) {
				statm = p_statm.next();
				if ((statm instanceof LabelPlacementStatm) ||
					(statm instanceof JumpStatm) ||
				    (statm instanceof CondJumpStatm)) {
					debug("Block ending with label or jump");
					p_statm.previous();
					break;
				} else {
					new_block.statements.add(statm);
				}
			}
			
			if (! p_statm.hasNext() || (statm instanceof LabelPlacementStatm)) {
				Label next_label;
				if (! p_statm.hasNext()) {
					blocks.finish_label = IRenv.addLabel();
					next_label = blocks.finish_label;
					debug("Block ends at the end of sequence, adding finish label " +
						next_label.getName() + " and adding jump to it");
				} else {
					next_label = ((LabelPlacementStatm)statm).label;
					debug("Block ends with label " + next_label.getName() +
						" but no jump, adding the jump");
				}
				JumpStatm jump_to_next = new JumpStatm(next_label);
				p_statm.add(jump_to_next);
				new_block.statements.add(jump_to_next);
			} else {
				debug("Block ends with a jump");
				new_block.statements.add(statm);
				p_statm.next();
			}
		}
		return blocks;
	}
	
	// To hell with Java lists
	private class BlockList {
		StatementBlock block;
		BlockList prev, next;
	}
	
	private class BlockInfo {
		StatementBlock block;
		BlockList position_in_remaining_list;
		
		BlockInfo(StatementBlock _block, BlockList _position) {
			block = _block;
			position_in_remaining_list = _position;
		}
	};
	
	private LinkedList<StatementBlock> arrangeBlocksForPrettyJumps(BlockSequence blocks) {
		LinkedList<StatementBlock> new_order = new LinkedList<>();
		
		BlockList first_remaining = null, last_remaining = null;
		TreeMap<Integer, BlockInfo> blocks_info_by_labelid = new TreeMap<>();
		
		for (StatementBlock block: blocks.blocks) {
			BlockList new_remaining = new BlockList();
			new_remaining.block = block;
			new_remaining.next = null;
			new_remaining.prev = last_remaining;
			if (last_remaining == null)
				first_remaining = new_remaining;
			else
				last_remaining.next = new_remaining;
			last_remaining = new_remaining;
				
			
			//auto position_in_remaining_list =
			//	remaining_blocks.insert(remaining_blocks.end(), &block);
			blocks_info_by_labelid.put(
				block.start_label.getIndex(),
				new BlockInfo(block, new_remaining)
			);
		}
		
		if (blocks.finish_label != null) {
			debug("Adding fake block for the sequence finish label " +
				blocks.finish_label.getName());
			blocks_info_by_labelid.put(
				blocks.finish_label.getIndex(),
				new BlockInfo(null, null)
			);
		}
		
		String s = "Remaining blocks:";
		for (BlockList b = first_remaining; b != null; b = b.next)
			s += " " + b.block.start_label.getName();
		debug(s);
		
		while (first_remaining != null) {
			StatementBlock current_block = first_remaining.block;
			debug("Picking block starting with " + current_block.start_label.getName());
			assert(! current_block.used_in_trace);
			current_block.used_in_trace = true;
			new_order.add(current_block);
			if (first_remaining.next != null)
				first_remaining.next.prev = null;
			first_remaining = first_remaining.next;

			s = "Remaining blocks:";
			for (BlockList b = first_remaining; b != null; b = b.next)
				s += " " + b.block.start_label.getName();
			debug(s);
			
			while (true) {
				Statement jump_statement = current_block.statements.getLast();
				assert ((jump_statement instanceof JumpStatm) || (jump_statement instanceof CondJumpStatm));
				Label jump_label;
				if (jump_statement instanceof JumpStatm) {
					assert ! ((JumpStatm)jump_statement).possible_results.isEmpty();
					jump_label = ((JumpStatm)jump_statement).possible_results.getFirst()[0];
					debug("Unconditionally jumps to " + jump_label.getName());
				} else {
					jump_label = ((CondJumpStatm)jump_statement).falseDest();
					debug(String.format("Conditionally jumps, true destination %s, false destination %s",
						((CondJumpStatm)jump_statement).trueDest().getName(),
						jump_label.getName()));
				}
				assert blocks_info_by_labelid.containsKey(jump_label.getIndex());
				
				BlockInfo blockinfo = blocks_info_by_labelid.get(jump_label.getIndex());
				if ((blockinfo.block == null) // block jumps to the end of the sequence
											  // rather than another block
						|| blockinfo.block.used_in_trace) {
					if (blockinfo.block == null)
						debug("Reached the end of sequence");
					else
						debug("Reached already used block " + blockinfo.block.start_label.getName());
					break;
				}
				debug("Following to block " + jump_label.getName());
				current_block = blockinfo.block;
				current_block.used_in_trace = true;
				new_order.add(current_block);
				//remaining_blocks.erase(blockinfo.position_in_remaining_list);
				BlockList deleting = blockinfo.position_in_remaining_list;
				if (deleting.prev == null)
					first_remaining = deleting.next;
				else
					deleting.prev.next = deleting.next;
				if (deleting.next == null)
					last_remaining = deleting.prev;
				else
					deleting.next.prev = deleting.prev;
				s = "Remaining blocks:";
				for (BlockList b = first_remaining; b != null; b = b.next)
					s += " " + b.block.start_label.getName();
				debug(s);
			}
		}
		return new_order;
	}

	public Canonicalizer(IREnvironment _ir_env) {
		super("canonicalize.log");
		IRenv = _ir_env;
	}
	
	public Expression canonicalizeExpression(Expression exp,
			Expression parentExpression, Statement parentStatement) {
		if (exp instanceof BinaryOpExp) {
			BinaryOpExp binop = (BinaryOpExp)exp;
			binop.left = canonicalizeExpression(binop.left, exp, null);
			binop.right = canonicalizeExpression(binop.right, exp, null);
			return canonicalizeBinaryOpExp(binop);
		} else if (exp instanceof MemoryExp) {
			MemoryExp mem = (MemoryExp)exp;
			mem.address = canonicalizeExpression(mem.address, exp, null);
			return canonicalizeMemoryExp(mem);
		} else if (exp instanceof CallExpression) {
			CallExpression call = (CallExpression)exp;
			call.function = canonicalizeExpression(call.function, exp, null);
			for (ListIterator<Expression> p = call.arguments.listIterator();
					p.hasNext(); ) {
				Expression arg = p.next();
				p.set(canonicalizeExpression(arg, exp, null));
			}
			return canonicalizeCallExp(call, parentExpression, parentStatement);
		} else if (exp instanceof StatExpSequence) {
			StatExpSequence seq = (StatExpSequence)exp;
			seq.exp = canonicalizeExpression(seq.exp, exp, null);
			seq.stat = canonicalizeStatement(seq.stat);
			combineStatExpSequences(seq);
			return seq;
		} else
			return exp;
	}
	
	public Statement canonicalizeStatement(Statement statm) {
		if (statm instanceof MoveStatm) {
			MoveStatm move = (MoveStatm)statm;
			move.to = canonicalizeExpression(move.to, null, statm);
			move.from = canonicalizeExpression(move.from, null, statm);
			return canonicalizeMoveStatement(move);
		} else if (statm instanceof ExpressionStatm) {
			ExpressionStatm exp = (ExpressionStatm)statm;
			exp.exp = canonicalizeExpression(exp.exp, null, statm);
			return canonicalizeExpressionStatement(exp);
		} else if (statm instanceof JumpStatm) {
			JumpStatm jump = (JumpStatm)statm;
			jump.dest = canonicalizeExpression(jump.dest, null, statm);
			return canonicalizeJumpStatement(jump); 
		} else if (statm instanceof CondJumpStatm) {
			CondJumpStatm cjump = (CondJumpStatm)statm;
			cjump.left = canonicalizeExpression(cjump.left, null, statm);
			cjump.right = canonicalizeExpression(cjump.right, null, statm);
			return canonicalizeCondJumpStatement(cjump);
		} else if (statm instanceof StatmSequence) {
			canonicalizeStatmSequence((StatmSequence)statm);
			return statm;
		} else
			return statm;
	}
	
	public void arrangeJumps(StatmSequence sequence) {
		BlockSequence blocks = splitToBlocks(sequence);
		LinkedList<StatementBlock> order = arrangeBlocksForPrettyJumps(blocks);
		sequence.statements.clear();

		for (StatementBlock block: order) {
			assert ! (block.statements.isEmpty());
		
			if ((! sequence.statements.isEmpty()) &&
					(sequence.statements.getLast() instanceof CondJumpStatm)) {
				CondJumpStatm last_cond_jump =
					(CondJumpStatm)sequence.statements.getLast();
				if (block.start_label.getIndex() ==
						last_cond_jump.falseDest().getIndex())
					// already good, do nothing
					;
				else if (block.start_label.getIndex() ==
						last_cond_jump.trueDest().getIndex()) {
					last_cond_jump.flip();
				} else {
					Label immediate_false = IRenv.addLabel();
					sequence.addStatement(new LabelPlacementStatm(immediate_false));
					sequence.addStatement(new JumpStatm(last_cond_jump.falseDest()));
					last_cond_jump.falseDestRef()[0] = immediate_false;
				}
			} else if ((! sequence.statements.isEmpty()) &&
					(sequence.statements.getLast() instanceof JumpStatm)) {
				Expression prev_jump_dest =
					((JumpStatm)sequence.statements.getLast()).dest;
				if ((prev_jump_dest instanceof LabelAddressExp) &&
					(block.start_label.getIndex() ==
						((LabelAddressExp)prev_jump_dest).label().getIndex())
				) {
					sequence.statements.removeLast();
				}
			}
			for (Statement block_statm: block.statements)
				sequence.addStatement(block_statm);
		}
		if (blocks.finish_label != null) {
			if ((! sequence.statements.isEmpty()) &&
					(sequence.statements.getLast() instanceof JumpStatm)) {
				Expression prev_jump_dest =
					((JumpStatm)sequence.statements.getLast()).dest;
				if ((prev_jump_dest instanceof LabelAddressExp) &&
					(blocks.finish_label.getIndex() ==
						((LabelAddressExp)prev_jump_dest).label().getIndex())
				) {
					sequence.statements.removeLast();
				}
			}
			sequence.addStatement(new LabelPlacementStatm(blocks.finish_label));
		}
	}
	
	public void arrangeJumpsInExpression(Expression expression) {
		if ((expression instanceof StatExpSequence) &&
				(((StatExpSequence)expression).stat instanceof StatmSequence))
			arrangeJumps((StatmSequence) ((StatExpSequence)expression).stat);
	}
}
