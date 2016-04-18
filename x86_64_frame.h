#ifndef _X86_64_FRAME_H
#define _X86_64_FRAME_H

#include "declarations.h"
#include "intermediate.h"

namespace IR {

class X86_64Frame: public AbstractFrame {
private:
	int frame_size;
	int param_count;
	int param_stack_size;
public:
 	X86_64Frame(AbstractFrameManager *_framemanager,
		const std::string &_name, int _id, AbstractFrame *_parent) :
		AbstractFrame(_framemanager, _name, _id, _parent),
		frame_size(0), param_count(0), param_stack_size(0) {}
	
	virtual VarLocation *createMemoryVariable(const std::string &name,
		int size);
	
	virtual VarLocation *createParameter(const std::string &name,
		int size);
	
	virtual VarLocation *createParentFpParameter(const std::string &name);
	
	int getFrameSize();
};

class X86_64FrameManager: public AbstractFrameManager {
private:
	X86_64Frame root_frame;
	int framecount;
public:
	X86_64FrameManager(IREnvironment *env) : AbstractFrameManager(env),
		root_frame(this, ".root", 0, NULL)
	{
		framecount = 1;
	}
	
	virtual AbstractFrame *rootFrame()
	{
		return &root_frame;
	}
	
	virtual AbstractFrame *newFrame(AbstractFrame *parent, const std::string &name)
	{
		return new X86_64Frame(this, name, framecount++, (X86_64Frame *)parent);
	}
	
	virtual int getIntSize()
	{
		return 8;
	}
	
	virtual int getPointerSize()
	{
		return 8;
	}
	
	virtual void updateRecordSize(int &size, int field_size)
	{
		size += field_size;
	}
	
	virtual void placeInt(int value, uint8_t *dest)
	{
		placeIntSameArch(value, dest);
	}
	
};

}


#endif
