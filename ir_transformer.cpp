#include "ir_transformer.h"
#include "errormsg.h"
#include <map>

namespace IR {

bool isNop(Statement *statm)
{
	return (statm->kind == IR_EXP_IGNORE_RESULT) &&
		ToExpressionStatement(statm)->exp->kind == IR_INTEGER;
}

bool canSwapExpAndStatement(Expression *exp, Statement *statm)
{
	return (exp->kind == IR_INTEGER) || (exp->kind == IR_LABELADDR) || isNop(statm);
}

bool canSwapExps(Expression *exp1, Expression *exp2)
{
	return (exp1->kind == IR_INTEGER) || (exp2->kind == IR_LABELADDR) ||
		(exp2->kind == IR_INTEGER) || (exp2->kind == IR_LABELADDR);
}

void IRTransformer::canonicalizeMemoryExp(Expression *&exp)
{
	MemoryExpression *mem_exp = ToMemoryExpression(exp);
	if (mem_exp->address->kind == IR_STAT_EXP_SEQ) {
		StatExpSequence *stat_exp = ToStatExpSequence(mem_exp->address);
		mem_exp->address = stat_exp->exp;
		stat_exp->exp = mem_exp;
		exp = stat_exp;
	}
}

void growStatementSequence(StatementSequence *sequence, Statement *statement)
{
	if (statement->kind == IR_STAT_SEQ) {
		for (std::list<Statement *>::iterator newstatm =
				ToStatementSequence(statement)->statements.begin();
				newstatm != ToStatementSequence(statement)->statements.end();
				newstatm++)
			sequence->addStatement(*newstatm);
	} else
		sequence->addStatement(statement);
}

/**
 * left and right can be overwritten
 * collected_statements is like a return value
 */
void IRTransformer::pullStatementsOutOfTwoOperands(Expression *&left,
	Expression *&right, StatementSequence *&collected_statements)
{
	StatementSequence *pre_statements = new StatementSequence;
	
	if (left->kind == IR_STAT_EXP_SEQ) {
		StatExpSequence *stat_exp = ToStatExpSequence(left);
		
		// stat_exp->exp cannot be IR_STAT_EXP_SEQ because
		// canonicalize that has been called on left earlier
		// gets rid of nested IR_STAT_EXP_SEQ
		assert(stat_exp->exp->kind != IR_STAT_EXP_SEQ);
		left = stat_exp->exp;
		growStatementSequence(pre_statements, stat_exp->stat);
		delete stat_exp;
	}

	if (right->kind == IR_STAT_EXP_SEQ) {
		StatExpSequence *stat_exp = ToStatExpSequence(right);
		if (canSwapExpAndStatement(left, stat_exp->stat)) {
			right = stat_exp->exp;
			growStatementSequence(pre_statements, stat_exp->stat);
		} else {
			VirtualRegister *save_left = ir_env->addRegister();
			pre_statements->addStatement(new MoveStatement(
				new RegisterExpression(save_left), left));
			growStatementSequence(pre_statements, stat_exp->stat);
			left = new RegisterExpression(save_left);
			// left is free of IR_STAT_EXP_SEQ (see above)
			// and so is stat_exp->exp for the same reason
			assert(stat_exp->exp->kind != IR_STAT_EXP_SEQ);
			right = stat_exp->exp;
		}
		delete stat_exp;
	}

	collected_statements = pre_statements;
}

void IRTransformer::canonicalizeBinaryOpExp(Expression *&exp)
{
	BinaryOpExpression *op_exp = ToBinaryOpExpression(exp);
	StatementSequence *pre_statements;
	
	pullStatementsOutOfTwoOperands(op_exp->left, op_exp->right, pre_statements);
	
	if (! pre_statements->statements.empty())
		exp = new StatExpSequence(pre_statements, op_exp);
	else
		delete pre_statements;
}

void IRTransformer::canonicalizeCallExp(Expression *&exp,
	Expression *parentExpression, Statement *parentStatement)
{
	CallExpression *call_exp = ToCallExpression(exp);
	StatementSequence *pre_statements = new StatementSequence;
	Expression **op_expression_at = &exp;
	std::vector<bool> arg_saved;
	arg_saved.resize(call_exp->arguments.size(), false);
	
	int arg_index = 0;
	for (std::list<Expression *>::iterator arg = call_exp->arguments.begin();
			arg != call_exp->arguments.end(); arg++) {
		if ((*arg)->kind == IR_STAT_EXP_SEQ) {
			StatExpSequence *stat_exp = ToStatExpSequence(*arg);
			int prev_index = 0;
			for (std::list<Expression *>::iterator prev_arg =
					call_exp->arguments.begin();
					prev_arg != arg;  prev_arg++) {
				if (! arg_saved[prev_index] &&
						! canSwapExpAndStatement(*prev_arg, stat_exp->stat)) {
					// Cannot move the statement pust this expression, need to
					// save the expression with implies pulling the expression
					// past all previously unsaved expressions
					int pre_prev_index = 0;
					for (std::list<Expression *>::iterator pre_prev_arg =
							call_exp->arguments.begin();
							pre_prev_arg != prev_arg;  pre_prev_arg++) {
						if (! arg_saved[pre_prev_index] &&
								! canSwapExps(*pre_prev_arg, *prev_arg)) {
							// Cannot move pre_arg expression past this expression
							// Need to save this expression
							arg_saved[pre_prev_index] = true;
							VirtualRegister *save_arg = ir_env->addRegister();
							pre_statements->addStatement(new MoveStatement(
								new RegisterExpression(save_arg), *pre_prev_arg));
							*pre_prev_arg = new RegisterExpression(save_arg);
						}
						pre_prev_index++;
					}
					
					// Now finally save the argument
					arg_saved[prev_index] = true;
					VirtualRegister *save_arg = ir_env->addRegister();
					pre_statements->addStatement(new MoveStatement(
						new RegisterExpression(save_arg), *prev_arg));
					*prev_arg = new RegisterExpression(save_arg);
				}
				prev_index++;
			}
			growStatementSequence(pre_statements, stat_exp->stat);
			// stat_exp->exp and thus the new argument expression is free from
			// IR_STAT_EXP_SEQ because recursize call to canonicalizeExpression
			// has got rid of nested IR_STAT_EXP_SEQ before calling us
			*arg = stat_exp->exp;
			delete stat_exp;
		}
		arg_index++;
	}
	
	Expression *transformed_call_exp;
	// Unless we are a child of statement "ignore expression result" or
	// "Move function result to a constant destination" or child of nothing,
	// replace call with moving return value to a register plus presenting
	// the register
	if (
		(parentStatement == NULL) && (parentExpression == NULL) ||
		(parentStatement != NULL) && (
			(parentStatement->kind == IR_EXP_IGNORE_RESULT) ||
			(parentStatement->kind == IR_MOVE) && (
				(ToMoveStatement(parentStatement)->to->kind == IR_REGISTER) ||
				(ToMoveStatement(parentStatement)->to->kind == IR_MEMORY) &&
				(( ToMemoryExpression(ToMoveStatement(parentStatement)->to) )->address->kind == IR_LABELADDR)
			)
		)
	) {
		// Already a child of acceptable parent
		transformed_call_exp = call_exp;
	} else {
		VirtualRegister *save_result = ir_env->addRegister();
		pre_statements->addStatement(new MoveStatement(
			new RegisterExpression(save_result), call_exp
		));
		transformed_call_exp = new RegisterExpression(save_result);
	}
		
	if (! pre_statements->statements.empty())
		exp = new StatExpSequence(pre_statements, transformed_call_exp);
	else {
		exp = transformed_call_exp;
		delete pre_statements;
	}
}

void IRTransformer::combineStatExpSequences(StatExpSequence *exp)
{
	if (exp->exp->kind == IR_STAT_EXP_SEQ) {
		StatExpSequence *child_stat_exp = ToStatExpSequence(exp->exp);
		assert(child_stat_exp->exp->kind != IR_STAT_EXP_SEQ);
		if (exp->stat->kind != IR_STAT_SEQ) {
			StatementSequence *stat_sequence = new StatementSequence;
			stat_sequence->addStatement(exp->stat);
			exp->stat = stat_sequence;
		}
		
		assert(exp->stat->kind == IR_STAT_SEQ);
		growStatementSequence(ToStatementSequence(exp->stat),
			child_stat_exp->stat);
		if (child_stat_exp->stat->kind == IR_STAT_SEQ)
			// After merging second sequence to the first, the second sequence
			// object (not its items) can leave
			delete ToStatementSequence(child_stat_exp->stat);
		
		exp->exp = child_stat_exp->exp;
		delete child_stat_exp;
	}
}

void IRTransformer::canonicalizeExpression(Expression *&exp,
	Expression *parentExpression, Statement *parentStatement)
{
	std::list<Expression **> subexpressions;
	
	switch (exp->kind) {
		case IR_INTEGER:
		case IR_LABELADDR:
		case IR_REGISTER:
			break;
		case IR_BINARYOP:
			subexpressions.push_back(& ToBinaryOpExpression(exp)->left);
			subexpressions.push_back(& ToBinaryOpExpression(exp)->left);
			break;
		case IR_MEMORY:
			subexpressions.push_back(& ToMemoryExpression(exp)->address);
			break;
		case IR_FUN_CALL: {
			subexpressions.push_back(& ToCallExpression(exp)->function);
			for (std::list<Expression *>::iterator arg =
					ToCallExpression(exp)->arguments.begin();
					arg != ToCallExpression(exp)->arguments.end(); arg++)
				subexpressions.push_back(&(*arg));
			break;
		}
		case IR_STAT_EXP_SEQ:
			subexpressions.push_back(& ToStatExpSequence(exp)->exp);
			canonicalizeStatement(ToStatExpSequence(exp)->stat);
			break;
		default:
			Error::fatalError("Unhandled IR::Expression kind");
	}
	
	for (std::list<Expression **>::iterator subexp = subexpressions.begin();
			 subexp != subexpressions.end(); subexp++)
		canonicalizeExpression(*(*subexp), exp, NULL);
	
	switch (exp->kind) {
		case IR_INTEGER:
		case IR_LABELADDR:
		case IR_REGISTER:
			break;
		
		case IR_MEMORY:
			canonicalizeMemoryExp(exp);
			break;
		
		case IR_BINARYOP:
			canonicalizeBinaryOpExp(exp);
			break;
		
		case IR_FUN_CALL:
			canonicalizeCallExp(exp, parentExpression, parentStatement);
			break;
		
		// If exp has been converted to IR_STAT_EXP_SEQ by the above cases,
		// it doesn't need to be processed by the below case because its
		// subexpression tree already doesn't contain any IR_STAT_EXP_SEQ
		case IR_STAT_EXP_SEQ:
			combineStatExpSequences(ToStatExpSequence(exp));
			break;
			
		default:
			Error::fatalError("Unhandled IR::Expression kind");
	}
}

void IRTransformer::canonicalizeMoveStatement(Statement *&statm)
{
	MoveStatement *move_statm = ToMoveStatement(statm);
	
	if (move_statm->to->kind == IR_MEMORY) {
		StatementSequence *pre_statements;
		pullStatementsOutOfTwoOperands(ToMemoryExpression(move_statm->to)->address,
			move_statm->from, pre_statements);
		if (! pre_statements->statements.empty()) {
			pre_statements->addStatement(move_statm);
			statm = pre_statements;
		} else
			delete pre_statements;
	} else if (move_statm->to->kind == IR_REGISTER) {
		if (move_statm->from->kind == IR_STAT_EXP_SEQ) {
			StatementSequence *sequence = new StatementSequence;
			StatExpSequence *stat_exp_seq = ToStatExpSequence(move_statm->from);
			growStatementSequence(sequence, stat_exp_seq->stat);
			move_statm->from = stat_exp_seq->exp;
			delete stat_exp_seq;
			growStatementSequence(sequence, move_statm);
			statm = sequence;
		}
	} else
		Error::fatalError("canonicalizeMoveStatement: strange destination");
}

void IRTransformer::canonicalizeExpressionStatement(Statement *&statm)
{
	ExpressionStatement *exp_statement = ToExpressionStatement(statm);
	if (exp_statement->exp->kind == IR_STAT_EXP_SEQ) {
		StatementSequence *sequence = new StatementSequence;
		StatExpSequence *stat_exp_seq = ToStatExpSequence(exp_statement->exp);
		growStatementSequence(sequence, stat_exp_seq->stat);
		exp_statement->exp = stat_exp_seq->exp;
		delete stat_exp_seq;
		growStatementSequence(sequence, exp_statement);
		statm = sequence;
	}
}

void IRTransformer::canonicalizeJumpStatement(Statement *&statm)
{
	JumpStatement *jump_statement = ToJumpStatement(statm);
	if (jump_statement->dest->kind == IR_STAT_EXP_SEQ) {
		StatementSequence *sequence = new StatementSequence;
		StatExpSequence *stat_exp_seq = ToStatExpSequence(jump_statement->dest);
		growStatementSequence(sequence, stat_exp_seq->stat);
		jump_statement->dest = stat_exp_seq->exp;
		delete stat_exp_seq;
		growStatementSequence(sequence, jump_statement);
		statm = sequence;
	}
}

void IRTransformer::canonicalizeCondJumpStatement(Statement *&statm)
{
	CondJumpStatement *cjump_statm = ToCondJumpStatement(statm);
	StatementSequence *pre_statements;
	
	pullStatementsOutOfTwoOperands(cjump_statm->left, cjump_statm->right, pre_statements);
	
	if (! pre_statements->statements.empty()) {
		pre_statements->addStatement(cjump_statm);
		statm = pre_statements;
	} else
		delete pre_statements;
}

void IRTransformer::doChildrenAndMergeChildStatSequences(StatementSequence *statm)
{
	std::list<Statement *>::iterator child = statm->statements.begin();
	while (child != statm->statements.end()) {
		std::list<Statement *>::iterator next = child;
		canonicalizeStatement(*child);
		next++;
		if ((*child)->kind == IR_STAT_SEQ) {
			StatementSequence *subsequence = ToStatementSequence(*child);
			for (std::list<Statement *>::iterator subitem =
					subsequence->statements.begin();
					subitem != subsequence->statements.end(); subitem++) {
				assert((*subitem)->kind != IR_STAT_SEQ);
				statm->statements.insert(child, *subitem);
			}
			delete subsequence;
			statm->statements.erase(child);
		}
		child = next;
	}
}

void IRTransformer::canonicalizeStatement(Statement *&statm)
{
	switch (statm->kind) {
		case IR_MOVE:
			canonicalizeExpression(ToMoveStatement(statm)->to, NULL, statm);
			canonicalizeExpression(ToMoveStatement(statm)->from, NULL, statm);
			canonicalizeMoveStatement(statm);
			break;
		case IR_EXP_IGNORE_RESULT: {
			canonicalizeExpression(ToExpressionStatement(statm)->exp, NULL, statm);
			canonicalizeExpressionStatement(statm);
			break;
		}
		case IR_JUMP:
			canonicalizeExpression(ToJumpStatement(statm)->dest, NULL, statm);
			canonicalizeJumpStatement(statm); // destroy child StatExpSequence, make statement sequence plus jump to clean expression
			break;
		case IR_COND_JUMP:
			canonicalizeExpression(ToCondJumpStatement(statm)->left, NULL, statm);
			canonicalizeExpression(ToCondJumpStatement(statm)->right, NULL, statm);
			canonicalizeCondJumpStatement(statm); // pull statements out of both operands, similar to processing binary expression
			break;
		case IR_STAT_SEQ:
			doChildrenAndMergeChildStatSequences(ToStatementSequence(statm));
			break;
		case IR_LABEL:
			break;
		default:
			Error::fatalError("Unhandled IR::Statement kind");
	}
}

void IRTransformer::splitToBlocks(StatementSequence* sequence,
	BlockSequence& blocks)
{
	std::list<Statement *>::iterator statm = sequence->statements.begin();
	blocks.finish_label = NULL;
	while (statm != sequence->statements.end()) {
		blocks.blocks.push_back(StatementBlock());
		StatementBlock &new_block = blocks.blocks.back();
		new_block.used_in_trace = false;
		if ((*statm)->kind == IR_LABEL) {
			new_block.start_label = ToLabelPlacementStatement(*statm)->label;
			new_block.statements.push_back(*statm);
			statm++;
		} else {
			new_block.start_label = ir_env->addLabel();
			LabelPlacementStatement *label_statm =
				new LabelPlacementStatement(new_block.start_label);
			sequence->statements.insert(statm, label_statm);
			new_block.statements.push_back(label_statm);
		}
		while ((statm != sequence->statements.end()) &&
				((*statm)->kind != IR_LABEL) && ((*statm)->kind != IR_JUMP) &&
				((*statm)->kind != IR_COND_JUMP)) {
			new_block.statements.push_back(*statm);
			statm++;
		}
		if ((statm == sequence->statements.end()) || ((*statm)->kind == IR_LABEL)) {
			Label *next_label;
			if (statm == sequence->statements.end()) {
				blocks.finish_label = ir_env->addLabel();
				next_label = blocks.finish_label;
			} else {
				next_label = ToLabelPlacementStatement(*statm)->label;
			}
			JumpStatement *jump_to_next = new JumpStatement(
				new LabelAddressExpression(next_label), next_label);
			sequence->statements.insert(statm, jump_to_next);
			new_block.statements.push_back(jump_to_next);
		} else {
			new_block.statements.push_back(*statm);
			statm++;
		}
	}
}

void IRTransformer::arrangeBlocksForPrettyJumps(BlockSequence &blocks,
		BlockOrdering &new_order)
{
	BlockOrdering remaining_blocks;
	std::map<int, BlockInfo> blocks_info_by_labelid;
	new_order.clear();
	
	for (std::list<StatementBlock>::iterator block = blocks.blocks.begin();
			block != blocks.blocks.end(); block++) {
		std::list<StatementBlock *>::iterator position_in_remaining_list =
			remaining_blocks.insert(remaining_blocks.end(), &(*block));
		blocks_info_by_labelid.insert(std::make_pair(
			(*block).start_label->getIndex(),
			BlockInfo(&(*block), position_in_remaining_list)
		));
	}
	if (blocks.finish_label != NULL)
		blocks_info_by_labelid.insert(std::make_pair(
			blocks.finish_label->getIndex(),
			BlockInfo(NULL, remaining_blocks.end())
		));
	
	while (! remaining_blocks.empty()) {
		StatementBlock *current_block = remaining_blocks.front();
		debug("Picking block starting with %s", current_block->start_label->getName().c_str());
		assert(! current_block->used_in_trace);
		current_block->used_in_trace = true;
		new_order.push_back(current_block);
		remaining_blocks.pop_front();
		
		while (true) {
			Statement *jump_statement = current_block->statements.back();
			assert((jump_statement->kind == IR_JUMP) || (jump_statement->kind == IR_COND_JUMP));
			Label *jump_label;
			if (jump_statement->kind == IR_JUMP) {
				assert(! ToJumpStatement(jump_statement)->possible_results.empty());
				jump_label = ToJumpStatement(jump_statement)->possible_results.front();
				debug("Unconditionally jumps to %s", jump_label->getName().c_str());
			} else {
				jump_label = ToCondJumpStatement(jump_statement)->false_dest;
				debug("Conditionally jumps, true destination %s, false destination %s",
					ToCondJumpStatement(jump_statement)->true_dest->getName().c_str(),
					jump_label->getName().c_str());
			}
			assert(blocks_info_by_labelid.find(jump_label->getIndex()) != 
				blocks_info_by_labelid.end());
			
			BlockInfo &blockinfo = blocks_info_by_labelid.at(jump_label->getIndex());
			if ((blockinfo.block == NULL) // block jumps to the end of the sequence
										  // rather than another block
					|| blockinfo.block->used_in_trace) {
				if (blockinfo.block == NULL)
					debug("Reached the end of sequence");
				else
					debug("Reached already used block %s", blockinfo.block->start_label->getName().c_str());
				break;
			}
			debug("Following to block %s", jump_label->getName().c_str());
			current_block = blockinfo.block;
			current_block->used_in_trace = true;
			new_order.push_back(current_block);
			remaining_blocks.erase(blockinfo.position_in_remaining_list);
		}
	}
}

void IRTransformer::arrangeJumps(StatementSequence* sequence)
{
	BlockSequence blocks;
	BlockOrdering order;
	splitToBlocks(sequence, blocks);
	arrangeBlocksForPrettyJumps(blocks, order);
	sequence->statements.clear();
	Label *finish_label = blocks.finish_label;
	
	for (BlockOrdering::iterator block = order.begin(); block != order.end();
			block++) {
		assert(! (*block)->statements.empty());
	
		if ((! sequence->statements.empty()) &&
				(sequence->statements.back()->kind == IR_COND_JUMP)) {
			CondJumpStatement *last_cond_jump =
				ToCondJumpStatement(sequence->statements.back());
			if ((*block)->start_label->getIndex() ==
					last_cond_jump->false_dest->getIndex())
				// already good, do nothing
				;
			else if ((*block)->start_label->getIndex() ==
					last_cond_jump->true_dest->getIndex()) {
				FlipComparison(last_cond_jump);
			} else {
				Label *immediate_false = ir_env->addLabel();
				sequence->addStatement(new LabelPlacementStatement(immediate_false));
				sequence->addStatement(new JumpStatement(
					new LabelAddressExpression(last_cond_jump->false_dest),
					last_cond_jump->false_dest));
				last_cond_jump->false_dest = immediate_false;
			}
		} else if ((! sequence->statements.empty()) &&
				(sequence->statements.back()->kind == IR_JUMP)) {
			Expression *prev_jump_dest =
				ToJumpStatement(sequence->statements.back())->dest;
			if ((prev_jump_dest->kind == IR_LABELADDR) &&
				((*block)->start_label->getIndex() ==
					ToLabelAddressExpression(prev_jump_dest)->label->getIndex())
			) {
				delete ToJumpStatement(sequence->statements.back());
				sequence->statements.pop_back();
			}
		}
		for (std::list<Statement *>::iterator block_statm =
				(*block)->statements.begin();
				block_statm != (*block)->statements.end(); block_statm++)
			sequence->addStatement(*block_statm);
	}
	if (blocks.finish_label != NULL) {
		if ((! sequence->statements.empty()) &&
				(sequence->statements.back()->kind == IR_JUMP)) {
			Expression *prev_jump_dest =
				ToJumpStatement(sequence->statements.back())->dest;
			if ((prev_jump_dest->kind == IR_LABELADDR) &&
				(blocks.finish_label->getIndex() ==
					ToLabelAddressExpression(prev_jump_dest)->label->getIndex())
			) {
				delete ToJumpStatement(sequence->statements.back());
				sequence->statements.pop_back();
			}
		}
		sequence->addStatement(new LabelPlacementStatement(blocks.finish_label));
	}
}

void IRTransformer::arrangeJumpsInExpression(Expression* expression)
{
	if ((expression->kind == IR_STAT_EXP_SEQ) &&
			(ToStatExpSequence(expression)->stat->kind == IR_STAT_SEQ))
		arrangeJumps(ToStatementSequence(ToStatExpSequence(expression)->stat));
}


}
