#ifndef _FRAME_H
#define _FRAME_H

#include "intermediate.h"
#include <map>

namespace IR {

class AbstractFrame;
class AbstractFrameManager;

class VarLocation: public DebugPrinter {
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
public:
	VarLocation(AbstractFrame *_frame, int _size, int _offset,
			const std::string &_name, bool _predefined) :
		DebugPrinter("translator.log"),
		owner_frame(_frame),
		reg(NULL),
		offset(_offset),
		size(_size),
		name(_name),
		predefined(_predefined),
		read_only(false) {}
		
	VarLocation(AbstractFrame *_frame, int _size, IR::VirtualRegister *_reg,
			bool _predefined) :
		DebugPrinter("translator.log"),
		owner_frame(_frame),
		reg(_reg),
		offset(-1),
		size(_size),
		name(_reg->getName()),
		predefined(_predefined),
		read_only(false) {}
	
	/**
	 * If true, it means that the variable is assigned only once before
	 * it is ever used. This happens to the register where the parent frame
	 * pointer parameter is moved to from its original register, which exists
	 * is the parameter was passed in a register.
	 */
	bool read_only;
	bool isPredefined() {return predefined;}
	bool isRegister() {return reg != NULL;}
	VirtualRegister *getRegister() {return reg;}
	IR::Expression createCode(AbstractFrame *currentFrame);
	void prespillRegister();
	AbstractFrame *getOwnerFrame() {return owner_frame;}
};

class AbstractFrame: public DebugPrinter {
protected:
	virtual VarLocation *createParameter(const std::string &name,
		int size) = 0;
	virtual VarLocation *createParentFpParameter(const std::string &name) = 0;
	virtual VarLocation *createMemoryVariable(const std::string &name,
		int size) = 0;
		
	AbstractFrameManager *framemanager;
	std::string name;
	int id;
	AbstractFrame *parent;
	bool calls_others;
private:
	std::list<VarLocation *>variables;
	std::list<VarLocation *>parameters;
	VarLocation *parent_fp_parameter;
	VirtualRegister *framepointer;
public:
	AbstractFrame(AbstractFrameManager *_framemanager,
		const std::string &_name, int _id, AbstractFrame *_parent);
	virtual ~AbstractFrame();
	
	VarLocation *addMemoryVariable(const std::string &name, int size);
	VarLocation *addVariable(const std::string &name, int size);
	VarLocation *addParameter(const std::string &name, int size);
	const std::list<VarLocation *> &getParameters() {return parameters;}
	const std::list<VarLocation *> &getVariables() {return variables;}
	
	void addParentFpParameter();
	VirtualRegister *getFramePointer() {return framepointer;}
	const std::string &getName() {return name;}
	int getId() {return id;}
	AbstractFrame *getParent() {return parent;}
	
	VarLocation *getParentFpForUs() {return parent_fp_parameter;}
	VarLocation *getParentFpForChildren();
	
	void prespillRegisters(Expression &exp, const std::map<int, VarLocation*> &spills);
	void prespillRegisters(Statement statm, const std::map<int, VarLocation*> &spills);
	
	void addFunctionCall() {calls_others = true;}
};

class DummyFrame: public AbstractFrame {
private:
	virtual VarLocation *createMemoryVariable(const std::string &name,
		int size)
	{
		return new VarLocation(this, size, 0, name, false);
	}
	virtual VarLocation *createParameter(const std::string &name,
		int size)
	{
		return new VarLocation(this, size, 0, name, false);
	}
	virtual VarLocation *createParentFpParameter(const std::string &name)
	{
		return createParameter(name, 4);
	}
public:
	DummyFrame(AbstractFrameManager *_framemanager, const std::string &name,
		int id, DummyFrame *parent) :
		AbstractFrame(_framemanager, name, id, parent) {}
	virtual IR::VirtualRegister *getFramePointer() {return NULL;}
};

class ParentFpHandler {
public:
	virtual void notifyFrameWithParentFp(AbstractFrame *frame) = 0;
};

class AbstractFrameManager {
private:
	IREnvironment *IR_env;
	ParentFpHandler *parent_fp_handler;
	std::list<AbstractFrame *>frames;
protected:
	virtual AbstractFrame *createFrame(AbstractFrame *parent, const std::string &name) = 0;
public:
	virtual AbstractFrame *rootFrame() = 0;
	virtual int getIntSize() = 0;
	virtual int getPointerSize() = 0;
	virtual void updateRecordSize(int &size, int field_size) = 0;
	/**
	 * Abstract because of possible endianness variability
	 */
	virtual void placeInt(int value, uint8_t *dest) = 0;
	void placeIntSameArch(int value, uint8_t *dest) {*((int *)dest) = value;}
	
	AbstractFrameManager(IREnvironment *env) :
		IR_env(env), parent_fp_handler(NULL) {}
	~AbstractFrameManager();
	IREnvironment *getIREnvironment() {return IR_env;}
	
	AbstractFrame *newFrame(AbstractFrame *parent, const std::string &name);
	
	void setFrameParentFpNotificationHandler(ParentFpHandler *handler)
	{
		parent_fp_handler = handler;
	}
	
	void notifyFrameWithParentFp(AbstractFrame *frame)
	{
		if (parent_fp_handler != NULL)
			parent_fp_handler->notifyFrameWithParentFp(frame);
	}
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
	
	virtual int getIntSize()
	{
		return 0;
	}
	
	virtual int getPointerSize()
	{
		return 0;
	}
	
	virtual void updateRecordSize(int &size, int field_size)
	{
	}
};

}

#endif // FRAME_H
