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
	virtual IR::Expression *createCode(AbstractFrame *frame);
};

class X86_64Frame: public AbstractFrame {
public:
	VirtualRegister *framepointer;
	X86_64Frame *parent;
	int frame_size;
	IREnvironment *ir_env;

	X86_64Frame(AbstractFrameManager *_framemanager, const std::string &name,
		X86_64Frame *_parent, IREnvironment *_ir_env,
		VirtualRegister *_framepointer) :
		AbstractFrame(_framemanager, name),
		parent(_parent),
		ir_env(_ir_env),
		framepointer(_framepointer),
		frame_size(0)
	{}
	
	virtual AbstractVarLocation *createVariable(const std::string &name,
		int size, bool cant_be_register)
	{
		X86_64VarLocation *result;
		if (cant_be_register) {
			result = new X86_64VarLocation(this, frame_size);
			frame_size += size + (8 - size % 8) % 8;
		} else {
			result = new X86_64VarLocation(this, ir_env->addRegister(this->name + "::" + name));
		}
		return result;
	}
	
};

class X86_64FrameManager: public AbstractFrameManager {
private:
	X86_64Frame *root_frame;
public:
	X86_64FrameManager(IREnvironment *env) : AbstractFrameManager(env)
	{
		root_frame = new X86_64Frame(this, ".root", NULL, IR_env,
			IR_env->addRegister("fp"));
	}
	
	~X86_64FrameManager()
	{
		delete root_frame;
	}
	
	virtual AbstractFrame *rootFrame()
	{
		return root_frame;
	}
	
	virtual AbstractFrame *newFrame(AbstractFrame *parent, const std::string &name)
	{
		return new X86_64Frame(this, name, (X86_64Frame *)parent, IR_env,
			IR_env->addRegister("fp"));
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
