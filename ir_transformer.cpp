#include "intermediate.h"
#include "errormsg.h"

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

void IREnvironment::canonicalizeMemoryExp(Expression *&exp)
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
void IREnvironment::pullStatementsOutOfTwoOperands(Expression *&left,
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
			Register *save_left = addRegister();
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

void IREnvironment::canonicalizeBinaryOpExp(Expression *&exp)
{
	BinaryOpExpression *op_exp = ToBinaryOpExpression(exp);
	StatementSequence *pre_statements;
	
	pullStatementsOutOfTwoOperands(op_exp->left, op_exp->right, pre_statements);
	
	if (! pre_statements->statements.empty())
		exp = new StatExpSequence(pre_statements, op_exp);
	else
		delete pre_statements;
}

void IREnvironment::canonicalizeCallExp(Expression *&exp,
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
							Register *save_arg = addRegister();
							pre_statements->addStatement(new MoveStatement(
								new RegisterExpression(save_arg), *pre_prev_arg));
							*pre_prev_arg = new RegisterExpression(save_arg);
						}
						pre_prev_index++;
					}
					
					// Now finally save the argument
					arg_saved[prev_index] = true;
					Register *save_arg = addRegister();
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
	// "Move function result to a constant destination", replace call with
	// moving return value to a register plus presenting the register
	if (
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
		Register *save_result = addRegister();
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

void IREnvironment::combineStatExpSequences(StatExpSequence *exp)
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

void IREnvironment::canonicalizeExpression(Expression *&exp,
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

void IREnvironment::canonicalizeMoveStatement(Statement *&statm)
{
	MoveStatement *move_statm = ToMoveStatement(statm);
	StatementSequence *pre_statements;
	
	pullStatementsOutOfTwoOperands(move_statm->to, move_statm->from, pre_statements);
	
	if (! pre_statements->statements.empty()) {
		pre_statements->addStatement(move_statm);
		statm = pre_statements;
	} else
		delete pre_statements;
}

void IREnvironment::canonicalizeExpressionStatement(Statement *&statm)
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

void IREnvironment::canonicalizeJumpStatement(Statement *&statm)
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

void IREnvironment::canonicalizeCondJumpStatement(Statement *&statm)
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

void IREnvironment::mergeChildStatSequences(StatementSequence *statm)
{
	std::list<Statement *>::iterator child = statm->statements.begin();
	while (child != statm->statements.end()) {
		std::list<Statement *>::iterator next = child;
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

void IREnvironment::canonicalizeStatement(Statement *&statm)
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
			mergeChildStatSequences(ToStatementSequence(statm));
			break;
		case IR_LABEL:
			break;
		default:
			Error::fatalError("Unhandled IR::Statement kind");
	}
}

}
