#ifndef _FRAME_H
#define _FRAME_H

#include "intermediate.h"
#include "types.h"

namespace IR {

class AbstractFrame;
class AbstractFrameManager;

class VarLocation {
private:
	AbstractFrame *owner_frame;
	/**
	 * null if this variable is in the memory from the beginning
	 * 
	 * If the register is prespilled, the variable will be inconsistently used
	 * sometimes as a register, sometimes a memory location. The resolution is:
	 * if the register is read only, keep the duality and just copy from the
	 * register to memory in the beginning of the function. It can still get
	 * spilled later by the register allocator, in which case that assignment to
	 * memory becomes assignment of a memory location to itself. The redundant
	 * assignment can be removed by a speciall read-only prespilled-spilling
	 * logic of the assembler.
	 * If the register is not read-only, it will be actually spilled for sure
	 * and only ever used as a memory location. 
	 */
	VirtualRegister *reg;
	int offset;
	int size;
	std::string name;
	/**
	 * True for function parameters including the parent frame pointer parameter,
	 * meaning that the variable holds a meaningful value before our code does
	 * anything
	 */
	bool predefined;
	/**
	 * If true, it means that the variable is assigned only once before
	 * it is ever used. This happens to the register where the parent frame
	 * pointer parameter is moved to from its original register, which exists
	 * is the parameter was passed in a register.
	 */
	bool read_only;
public:
	VarLocation(AbstractFrame *_frame, int _size, int _offset,
			const std::string &_name, bool _predefined) :
		owner_frame(_frame),
		reg(NULL),
		offset(_offset),
		size(_size),
		name(_name),
		predefined(_predefined),
		read_only(false) {}
		
	VarLocation(AbstractFrame *_frame, int _size, IR::VirtualRegister *_reg,
			bool _predefined) :
		owner_frame(_frame),
		reg(_reg),
		offset(-1),
		size(_size),
		name(_reg->getName()),
		predefined(_predefined),
		read_only(false) {}

	bool isPredefined() {return predefined;}
	bool isRegister() {return reg != NULL;}
	IR::Expression createCode(AbstractFrame *currentFrame);
	void prespillRegister(VarLocation *location);
	AbstractFrame *getOwnerFrame() {return owner_frame;}
};

class AbstractFrame: public DebugPrinter {
protected:
	virtual VarLocation *createParameter(const std::string &name,
		int size) = 0;
	virtual VarLocation *createParentFpParameter(const std::string &name) = 0;
public:
	virtual VarLocation *createMemoryVariable(const std::string &name,
		int size) = 0;
protected:
	AbstractFrameManager *framemanager;
	std::string name;
	int id;
	AbstractFrame *parent;
	bool calls_others;
private:
	std::list<VarLocation *>variables;
	std::list<VarLocation *>parameters;
	VarLocation *parent_fp_parameter;
	VarLocation *framepointer;
public:
	AbstractFrame(AbstractFrameManager *_framemanager,
		const std::string &_name, int _id, AbstractFrame *_parent) :
		DebugPrinter("translator.log"),
		framemanager(_framemanager), name(_name), id(_id), parent(_parent),
		parent_fp_parameter(NULL), parent_fp_memory_storage(NULL),
		calls_others(false)
	{}
	
	VarLocation *addVariable(const std::string &name,
		int size, bool cant_be_register);
	VarLocation *addParameter(const std::string &name,
		int size, bool cant_be_register);
	const std::list<VarLocation *> &getParameters()
		{return parameters;}
	const std::list<ParameterMovement> &getMovementPrologue()
		{return parameter_store_prologue;}
	void addParentFpParamVariable(bool cant_be_register);
	virtual IR::VirtualRegister *getFramePointer() = 0;
	const std::string &getName() {return name;}
	int getId() {return id;}
	AbstractFrame *getParent() {return parent;}
	
	VarLocation *getParentFpForUs()
	{
		return parent_fp_parameter;
	}
	
	VarLocation *getParentFpForChildren()
	{
		return parent_fp_memory_storage;
	}
	
	void addFunctionCall() {calls_others = true;}
	
	~AbstractFrame();
};

class DummyFrame: public AbstractFrame {
private:
	virtual VarLocation *createVariable(const std::string &name,
		int size, bool cant_be_register)
	{
		return new VarLocation(this, size, 0, name, false);
	}
	virtual VarLocation *createParameter(const std::string &name,
		int size)
	{
		return new DummyVarLocation(this);
	}
public:
	DummyFrame(AbstractFrameManager *_framemanager, const std::string &name,
		int id, DummyFrame *parent) :
		AbstractFrame(_framemanager, name, id, parent) {}
	virtual IR::VirtualRegister *getFramePointer() {return NULL;}
};

class AbstractFrameManager {
protected:
	IREnvironment *IR_env;
public:
	AbstractFrameManager(IREnvironment *env) : IR_env(env) {}
	virtual AbstractFrame *rootFrame() = 0;
	virtual AbstractFrame *newFrame(AbstractFrame *parent, const std::string &name) = 0;
	virtual int getVarSize(Semantic::Type *type) = 0;
	virtual int getPointerSize() = 0;
	virtual void updateRecordSize(int &size, Semantic::Type *newFieldType) = 0;
};

class DummyFrameManager: public AbstractFrameManager {
public:
	DummyFrameManager(IREnvironment *env) : AbstractFrameManager(env) {}
	
	virtual DummyFrame *rootFrame()
	{
		return new DummyFrame(this, ".root", 0, NULL);
	}
	
	virtual DummyFrame *newFrame(AbstractFrame *parent, const std::string &name)
	{
		return new DummyFrame(this, name, 0, (DummyFrame *)parent);
	}
	
	virtual int getVarSize(Semantic::Type *type)
	{
		return 0;
	}
	
	virtual int getPointerSize(Semantic::Type *type)
	{
		return 0;
	}
	
	virtual void updateRecordSize(int &size, Semantic::Type *newFieldType)
	{
	}
};

}

#endif // FRAME_H
