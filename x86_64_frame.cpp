#include "x86_64_frame.h"
#include "errormsg.h"

#include <list>

namespace IR {

VarLocation* X86_64Frame::createMemoryVariable(const std::string& name, int size)
{
	assert(size <= 8);
	frame_size += size + (8 - size % 8) % 8;
	return new VarLocation(this, size, -frame_size, name, false);
}

VarLocation* X86_64Frame::createParameter(const std::string& name, int size)
{
	VarLocation *result;
	if (param_count >= 6) {
		result = new VarLocation(this, size, 8+param_stack_size, // 8 for return address on stack
			name, true);
		assert(size <= 8);
		param_stack_size += size + (8 - size % 8) % 8;
	} else
		result = new VarLocation(this, framemanager->getPointerSize(),
			framemanager->getIREnvironment()->addRegister(this->name + "::" + name),
			true);
	param_count++;
	return result;
}

VarLocation *X86_64Frame::createParentFpParameter(const std::string &name) {
	return new VarLocation(this, framemanager->getPointerSize(),
		framemanager->getIREnvironment()->addRegister(this->name + "::" + name),
		true);
}


int X86_64Frame::getFrameSize()
{
	if (calls_others)
		return frame_size + (24 - frame_size % 16) % 16;
	else
		return frame_size;
}

}
