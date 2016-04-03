#ifndef _TRANSLATE_UTILS_H
#define _TRANSLATE_UTILS_H

#include "idmap.h"
#include "syntaxtree.h"
#include "intermediate.h"
#include "types.h"
#include <vector>
#include <list>

namespace IR {

class AbstractFrame;
class AbstractFrameManager;

class AbstractVarLocation {
public:
	AbstractFrame *owner_frame;
	
	AbstractVarLocation(AbstractFrame *_frame) : owner_frame(_frame) {}
	virtual IR::Expression *createCode(AbstractFrame *currentFrame) = 0;
	virtual bool isRegister() = 0;
	virtual void prespillRegister(AbstractVarLocation *location) = 0;
};

class DummyVarLocation: public AbstractVarLocation {
public:
	DummyVarLocation(AbstractFrame *frame) : AbstractVarLocation(frame) {}
	virtual IR::Expression *createCode(AbstractFrame *currentFrame) {return NULL;}
	virtual bool isRegister() {return true;}
	virtual void prespillRegister(AbstractVarLocation *) {}
};

class AbstractFrame {
private:
	virtual AbstractVarLocation *createVariable(const std::string &name,
		int size, bool cant_be_register) = 0;
	virtual AbstractVarLocation *createParameter(const std::string &name,
		int size) = 0;
public:
	struct ParameterMovement {
		AbstractVarLocation *parameter;
		AbstractVarLocation *where_store;
	};
protected:
	std::string name;
	AbstractVarLocation *parent_fp_parameter;
	AbstractVarLocation *parent_fp_memory_storage;
	AbstractFrameManager *framemanager;
	std::list<AbstractVarLocation *>variables;
	std::list<AbstractVarLocation *>parameters;
	
	std::list<ParameterMovement> parameter_store_prologue;
public:
	AbstractFrame(AbstractFrameManager *_framemanager,
		const std::string &_name) : framemanager(_framemanager), name(_name),
		parent_fp_parameter(NULL), parent_fp_memory_storage(NULL)
	{}
	
	AbstractVarLocation *addVariable(const std::string &name,
		int size, bool cant_be_register);
	AbstractVarLocation *addParameter(const std::string &name,
		int size, bool cant_be_register);
	const std::list<AbstractVarLocation *> &getParameters()
		{return parameters;}
	const std::list<ParameterMovement> &getMovementPrologue()
		{return parameter_store_prologue;}
	void addParentFpParamVariable(bool cant_be_register);
	virtual IR::VirtualRegister *getFramePointer() = 0;
	
	AbstractVarLocation *getParentFpForUs()
	{
		return parent_fp_parameter;
	}
	
	AbstractVarLocation *getParentFpForChildren()
	{
		return parent_fp_memory_storage;
	}
	
	~AbstractFrame();
};

class DummyFrame: public AbstractFrame {
private:
	virtual AbstractVarLocation *createVariable(const std::string &name,
		int size, bool cant_be_register)
	{
		return new DummyVarLocation(this);
	}
	virtual AbstractVarLocation *createParameter(const std::string &name,
		int size)
	{
		return new DummyVarLocation(this);
	}
public:
	DummyFrame(AbstractFrameManager *_framemanager, const std::string &name) :
		AbstractFrame(_framemanager, name) {}
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
		return new DummyFrame(this, ".root");
	}
	
	virtual DummyFrame *newFrame(AbstractFrame *parent, const std::string &name)
	{
		return new DummyFrame(this, name);
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

namespace Semantic {

class VariablesAccessInfoPrivate;

class VariablesAccessInfo {
private:
	VariablesAccessInfoPrivate *impl;
	std::list<int> func_stack;
	
public:
	VariablesAccessInfo();
	~VariablesAccessInfo();
	
	void processExpression(Syntax::Tree expression, ObjectId current_function_id);
	void processDeclaration(Syntax::Tree declaration, ObjectId current_function_id);
	bool isAccessedByAddress(Syntax::Tree definition);
	bool functionNeedsParentFp(Syntax::Function *definition);
	bool isFunctionParentFpAccessedByChildren(Syntax::Function *definition);
};

}

#endif
