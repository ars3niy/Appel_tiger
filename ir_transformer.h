#ifndef _IR_TRANSFORMER_H
#define _IR_TRANSFORMER_H

#include "intermediate.h"
#include "debugprint.h"
#include "frame.h"

namespace IR {

bool canSwapExps(Expression exp1, Expression exp2);
void EvaluateExpression(Expression &exp);

class IRTransformer: public DebugPrinter {
private:
	IREnvironment *ir_env;

	void pullStatementsOutOfTwoOperands(Expression &left,
		Expression &right, std::shared_ptr<StatementSequence> &collected_statements);
	void canonicalizeMemoryExp(Expression &exp);
	void canonicalizeBinaryOpExp(Expression &exp);
	void canonicalizeCallExp(Expression &exp,
		Expression parentExpression, Statement parentStatement);
	void combineStatExpSequences(std::shared_ptr<StatExpSequence> exp);
	
	void canonicalizeMoveStatement(Statement &statm);
	void canonicalizeExpressionStatement(Statement &statm);
	void canonicalizeJumpStatement(Statement &statm);
	void canonicalizeCondJumpStatement(Statement &statm);
	void doChildrenAndMergeChildStatSequences(std::shared_ptr<StatementSequence> statm);

	struct StatementBlock {
		std::list<Statement> statements;
		Label *start_label;
		bool used_in_trace;
		bool arrivable;
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
	
	void splitToBlocks(std::shared_ptr<StatementSequence> sequence, BlockSequence &blocks);
	void arrangeBlocksForPrettyJumps(BlockSequence &blocks,
		BlockOrdering &new_order);
public:
	IRTransformer(IREnvironment *_ir_env) :
		DebugPrinter("canonicalize.log"),
		ir_env(_ir_env){}

	/**
	 * Either parentExpression or parentStatement (or both) must be null
	 */
	void canonicalizeExpression(Expression &exp,
		Expression parentExpression, Statement parentStatement);
	void canonicalizeStatement(Statement &statm);
	void arrangeJumps(std::shared_ptr<StatementSequence> sequence);
	void arrangeJumpsInExpression(Expression expression);
};

}

#endif
