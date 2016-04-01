#ifndef _TRANSLATE_UTILS_H
#define _TRANSLATE_UTILS_H

#include "idmap.h"
#include "syntaxtree.h"
#include "intermediate.h"
#include "types.h"
#include <vector>

namespace IR {

class AbstractFrame;
class AbstractFrameManager;

class AbstractVarLocation {
public:
	AbstractFrame *owner_frame;
	
	AbstractVarLocation(AbstractFrame *_frame) : owner_frame(_frame) {}
	virtual IR::Expression *createCode(AbstractFrame *currentFrame) = 0;
};

class DummyVarLocation: public AbstractVarLocation {
public:
	DummyVarLocation(AbstractFrame *frame) : AbstractVarLocation(frame) {}
	virtual IR::Expression *createCode(AbstractFrame *currentFrame) {return NULL;}
};

class AbstractFrame {
private:
	virtual AbstractVarLocation *createVariable(const std::string &name,
		int size, bool cant_be_register) = 0;
protected:
	std::string name;
	AbstractVarLocation *parent_fp_parameter;
	AbstractFrameManager *framemanager;
public:
	std::list<AbstractVarLocation *>variables;
	
	AbstractFrame(AbstractFrameManager *_framemanager,
		const std::string &_name) : framemanager(_framemanager), name(_name),
		parent_fp_parameter(NULL)
	{}
	
	AbstractVarLocation *addVariable(const std::string &name,
		int size, bool cant_be_register);
	void addParentFpParamVariable(bool cant_be_register);
	
	AbstractVarLocation *getParentFpParamVariable()
	{
		return parent_fp_parameter;
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
public:
	DummyFrame(AbstractFrameManager *_framemanager, const std::string &name) :
		AbstractFrame(_framemanager, name) {}
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
	IdMap var_info;
	VariablesAccessInfoPrivate *impl;
public:
	VariablesAccessInfo();
	~VariablesAccessInfo();
	
	void processExpression(Syntax::Tree expression, ObjectId current_function_id);
	void processDeclaration(Syntax::Tree declaration, ObjectId current_function_id);
	bool isAccessedByAddress(Syntax::Tree definition);
	bool functionNeedsParentFp(Syntax::Tree definition)
	{return true;}
	bool isFunctionParentFpAccessedByChildren(Syntax::Tree definition)
	{return true;}
};

}

#endif
