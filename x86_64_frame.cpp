#include "x86_64_frame.h"
#include "errormsg.h"

#include <list>

namespace IR {

Code* X86_64VarLocation::createCode(AbstractFrame *calling_frame)
{
	std::list<X86_64Frame*> frame_stack;
	X86_64Frame *frame = (X86_64Frame *)calling_frame;
	while (frame != NULL) {
		frame_stack.push_front(frame);
		if (frame == owner_frame)
			break;
		frame = (X86_64Frame *)frame->parent;
	}
	if (frame != owner_frame)
		Error::fatalError("Variable has been used outside its function without syntax error??");
		
	return new IR::ExpressionCode(new IR::MemoryExpression(
		new IR::BinaryOpExpression(IR::OP_PLUS,
			new IR::RegisterExpression(((X86_64Frame *)calling_frame)->framepointer),
			new IR::IntegerExpression(offset)
		)
	));
}

}
