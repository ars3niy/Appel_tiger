#ifndef _X86_64_FRAME_H
#define _X86_64_FRAME_H

#include "declarations.h"
#include "intermediate.h"

namespace IR {

class X86_64VarLocation: public AbstractVarLocation {
private:
	bool is_register;
	int offset;
	VirtualRegister *reg;
public:
	X86_64VarLocation(AbstractFrame *frame, int _offset) :
		AbstractVarLocation(frame), is_register(false), offset(_offset),
		reg(NULL)
	{}
	X86_64VarLocation(AbstractFrame *frame, VirtualRegister *_register) :
		AbstractVarLocation(frame), is_register(true), offset(0),
		reg(_register)
	{}
	virtual ~X86_64VarLocation() {}
	virtual IR::Expression createCode(AbstractFrame *frame);
	virtual bool isRegister() {return reg != NULL;}
	VirtualRegister *getRegister() {return reg;}
	virtual void prespillRegister(AbstractVarLocation *location)
	{
		if (reg != NULL)
			reg->prespill(location);
	}
};

class X86_64Frame: public AbstractFrame {
private:
	IREnvironment *ir_env;
	VirtualRegister *framepointer;
	int frame_size;
	int param_count;
	int param_stack_size;
public:
	X86_64Frame(AbstractFrameManager *_framemanager, const std::string &name,
		int _id, X86_64Frame *_parent, IREnvironment *_ir_env,
		VirtualRegister *_framepointer) :
		AbstractFrame(_framemanager, name, _id, _parent),
		ir_env(_ir_env),
		framepointer(_framepointer),
		frame_size(0),
		param_count(0),
		param_stack_size(0)
	{}
	
	virtual AbstractVarLocation *createVariable(const std::string &name,
		int size, bool cant_be_register);
	
	virtual AbstractVarLocation *createParameter(const std::string &name,
		int size);
	
	virtual IR::VirtualRegister *getFramePointer() {return framepointer;}
	
	int getFrameSize();
};

class X86_64FrameManager: public AbstractFrameManager {
private:
	X86_64Frame root_frame;
	int framecount;
public:
	X86_64FrameManager(IREnvironment *env) : AbstractFrameManager(env),
		root_frame(this, ".root", 0, NULL, IR_env,
			IR_env->addRegister("fp"))
	{
		framecount = 1;
	}
	
	virtual AbstractFrame *rootFrame()
	{
		return &root_frame;
	}
	
	virtual AbstractFrame *newFrame(AbstractFrame *parent, const std::string &name)
	{
		return new X86_64Frame(this, name, framecount++, (X86_64Frame *)parent,
			IR_env, IR_env->addRegister("fp"));
	}
	
	virtual int getVarSize(Semantic::Type *type)
	{
		return 8;
	}
	
	virtual int getPointerSize()
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
