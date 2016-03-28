#ifndef _X86_64_FRAME_H
#define _X86_64_FRAME_H

#include "declarations.h"
#include "intermediate.h"

namespace IR {

class X86_64VarLocation: public AbstractVarLocation {
private:
	int offset;
public:
	X86_64VarLocation(AbstractFrame *frame, int _offset) :
		AbstractVarLocation(frame), offset(_offset) {}
	virtual IR::Code *createCode(AbstractFrame *frame);
};

class X86_64Frame: public AbstractFrame {
public:
	Register *framepointer;
	X86_64Frame *parent;
	int frame_size;

	X86_64Frame(X86_64Frame *_parent, Register *_framepointer) :
		parent(_parent), framepointer(_framepointer) {}
	
	virtual AbstractVarLocation *createVariable(int size, bool cant_be_register)
	{
		X86_64VarLocation *result = new X86_64VarLocation(this, frame_size);
		frame_size += size + (8 - size % 8) % 8;
		return result;
	}
	
};

class X86_64FrameManager: public AbstractFrameManager {
private:
	X86_64Frame *root_frame;
public:
	X86_64FrameManager(IREnvironment *env) : AbstractFrameManager(env) {}
	
	virtual AbstractFrame *rootFrame()
	{
		return root_frame;
	}
	
	virtual AbstractFrame *newFrame(AbstractFrame *parent)
	{
		return new X86_64Frame((X86_64Frame *)parent, IR_env->addRegister());
	}
	
	virtual int getVarSize(Semantic::Type *type)
	{
		return 8;
	}
	
	virtual void updateRecordSize(int &size, Semantic::Type *newFieldType)
	{
		size += getVarSize(newFieldType);
	}
};

}


#endif
