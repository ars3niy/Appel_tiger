#ifndef _IR_TRANSFORMER_H
#define _IR_TRANSFORMER_H

#include "intermediate.h"

namespace IR {

class IRTransformer {
private:
	IREnvironment *ir_env;

	void pullStatementsOutOfTwoOperands(Expression *&left,
		Expression *&right, StatementSequence *&collected_statements);
	void canonicalizeMemoryExp(Expression *&exp);
	void canonicalizeBinaryOpExp(Expression *&exp);
	void canonicalizeCallExp(Expression *&exp,
		Expression *parentExpression, Statement *parentStatement);
	void combineStatExpSequences(StatExpSequence *exp);
	
	void canonicalizeMoveStatement(Statement *&statm);
	void canonicalizeExpressionStatement(Statement *&statm);
	void canonicalizeJumpStatement(Statement *&statm);
	void canonicalizeCondJumpStatement(Statement *&statm);
	void doChildrenAndMergeChildStatSequences(StatementSequence *statm);

	struct StatementBlock {
		std::list<Statement *> statements;
		Label *start_label;
		bool used_in_trace;
	};
	
	struct BlockSequence {
		std::list<StatementBlock> blocks;
		Label *finish_label;
	};
	
	typedef std::list<StatementBlock*> BlockOrdering;
	struct BlockInfo {
		StatementBlock *block;
		std::list<StatementBlock *>::iterator position_in_remaining_list;
		
		BlockInfo(StatementBlock *_block,
			std::list<StatementBlock *>::iterator _position) :
				block(_block), position_in_remaining_list(_position) {}
	};
	
	void splitToBlocks(StatementSequence *sequence, BlockSequence &blocks);
	void arrangeBlocksForPrettyJumps(BlockSequence &blocks,
		BlockOrdering &new_order);
public:
	IRTransformer(IREnvironment *_ir_env) : ir_env(_ir_env) {}

	/**
	 * Either parentExpression or parentStatement (or both) must be NULL
	 */
	void canonicalizeExpression(Expression *&exp,
		Expression *parentExpression, Statement *parentStatement);
	void canonicalizeStatement(Statement *&statm);
	void arrangeJumps(StatementSequence *sequence);
	void arrangeJumpsInExpression(Expression *expression);
};

}

#endif
