#include "x86_64_frame.h"
#include "errormsg.h"

#include <list>

namespace IR {

Expression* X86_64VarLocation::createCode(AbstractFrame *calling_frame)
{
	if (is_register) {
		assert(calling_frame == owner_frame);
		return new RegisterExpression(this->reg);
	}
	
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
	
	Expression *result = NULL;
	X86_64Frame *var_owner = (X86_64Frame *)owner_frame;
	Expression **put_access_to_owner_frame = &result;
	
	for (std::list<X86_64Frame *>::iterator frame = frame_stack.begin();
			frame != frame_stack.end(); frame++) {
		X86_64VarLocation *var_location =
			((*frame) == owner_frame) ?
				this :
				(X86_64VarLocation *) (*frame)->getParentFpParamVariable();
				
		assert(var_location != NULL);
		assert(put_access_to_owner_frame != NULL);
	
		Expression *access_to_owner_frame;
		Expression **put_access_to_next_owner_frame;
		if (var_location->is_register) {
			assert(*frame == calling_frame);
			access_to_owner_frame = var_location->createCode(*frame);
			put_access_to_next_owner_frame = NULL;
		} else {
			BinaryOpExpression *address_expr = new BinaryOpExpression(
				IR::OP_MINUS, NULL, new IntegerExpression(var_location->offset));
			access_to_owner_frame = new MemoryExpression(address_expr);
			put_access_to_next_owner_frame = &address_expr->left;
		}
		
		*put_access_to_owner_frame = access_to_owner_frame;
		put_access_to_owner_frame = put_access_to_next_owner_frame;
	}
	if (put_access_to_owner_frame != NULL) {
		*put_access_to_owner_frame = new RegisterExpression(
			((X86_64Frame *)calling_frame)->framepointer);
	}
	assert(result != NULL);
	return result;
}

}
