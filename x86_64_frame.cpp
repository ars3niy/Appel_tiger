#include "x86_64_frame.h"
#include "errormsg.h"

#include <list>

namespace IR {

VarLocation* X86_64Frame::createVariable(const std::string& name, int size, bool cant_be_register)
{
	X86_64VarLocation *result;
	if (cant_be_register) {
		assert(size <= 8);
		frame_size += size + (8 - size % 8) % 8;
		result = new X86_64VarLocation(this, -frame_size);
	} else {
		result = new X86_64VarLocation(this, ir_env->addRegister(this->name + "::" + name));
	}
	return result;
}

VarLocation* X86_64Frame::createParameter(const std::string& name, int size)
{
	VarLocation *result;
	if (param_count >= 6) {
		result = new X86_64VarLocation(this, 8+param_stack_size); // 8 for return address on stack
		assert(size <= 8);
		param_stack_size += size + (8 - size % 8) % 8;
	} else
		result = createVariable(name, size, false);
	param_count++;
	return result;
}

int X86_64Frame::getFrameSize()
{
	if (calls_others)
		return frame_size + (24 - frame_size % 16) % 16;
	else
		return frame_size;
}

Expression X86_64VarLocation::createCode(AbstractFrame *calling_frame)
{
	if (is_register) {
		assert(calling_frame == owner_frame);
		return std::make_shared<RegisterExpression>(this->reg);
	}
	
	std::list<X86_64Frame*> frame_stack;
	X86_64Frame *frame = (X86_64Frame *)calling_frame;
	while (frame != NULL) {
		frame_stack.push_front(frame);
		if (frame == owner_frame)
			break;
		frame = (X86_64Frame *)frame->getParent();
	}
	if (frame != owner_frame)
		Error::fatalError("Variable has been used outside its function without syntax error??");
	
	Expression result = nullptr;
	//X86_64Frame *var_owner = (X86_64Frame *)owner_frame;
	Expression *put_access_to_owner_frame = &result;
	
	for (std::list<X86_64Frame *>::iterator frame = frame_stack.begin();
			frame != frame_stack.end(); frame++) {
		X86_64VarLocation *var_location;
		if ((*frame) == owner_frame)
			var_location = this;
		else if (*frame == calling_frame)
			var_location = (X86_64VarLocation *) (*frame)->getParentFpForUs();
		else
			var_location = (X86_64VarLocation *) (*frame)->getParentFpForChildren();
				
		assert(var_location != NULL);
		assert(put_access_to_owner_frame != NULL);
	
		Expression access_to_owner_frame;
		Expression *put_access_to_next_owner_frame;
		if (var_location->is_register) {
			assert(*frame == calling_frame);
			access_to_owner_frame = var_location->createCode(*frame);
			put_access_to_next_owner_frame = NULL;
		} else {
			std::shared_ptr<BinaryOpExpression> address_expr = std::make_shared<BinaryOpExpression>(
				IR::OP_PLUS, nullptr, std::make_shared<IntegerExpression>(var_location->offset));
			access_to_owner_frame = std::make_shared<MemoryExpression>(address_expr);
			put_access_to_next_owner_frame = &address_expr->left;
		}
		
		*put_access_to_owner_frame = access_to_owner_frame;
		put_access_to_owner_frame = put_access_to_next_owner_frame;
	}
	if (put_access_to_owner_frame != NULL) {
		*put_access_to_owner_frame = std::make_shared<RegisterExpression>(
			((X86_64Frame *)calling_frame)->getFramePointer());
	}
	assert(result != NULL);
	return result;
}

}
