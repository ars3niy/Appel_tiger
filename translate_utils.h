#ifndef _TRANSLATE_UTILS_H
#define _TRANSLATE_UTILS_H

#include "idmap.h"
#include "syntaxtree.h"
#include "intermediate.h"
#include "types.h"
#include <vector>

namespace IR {

class AbstractFrame;

class AbstractVarLocation {
public:
	AbstractFrame *frame;
	
	AbstractVarLocation(AbstractFrame *_frame) : frame(_frame) {}
	virtual IR::Code *createCode(AbstractFrame *currentFrame) = 0;
};

class DummyVarLocation: public AbstractVarLocation {
public:
	DummyVarLocation(AbstractFrame *frame) : AbstractVarLocation(frame) {}
	virtual IR::Code *createCode(AbstractFrame *currentFrame) {return NULL;}
};

class AbstractFrame {
private:
	virtual AbstractVarLocation *createVariable(int size, bool cant_be_register) = 0;
public:
	std::list<AbstractVarLocation *>variables;
	AbstractVarLocation *addVariable(int size, bool cant_be_register);
	~AbstractFrame();
};

class DummyFrame: public AbstractFrame {
private:
	virtual AbstractVarLocation *createVariable(int size, bool cant_be_register)
	{
		return new DummyVarLocation(this);
	}
};

class AbstractFrameManager {
public:
	virtual AbstractFrame *rootFrame() = 0;
	virtual AbstractFrame *newFrame(AbstractFrame *parent) = 0;
	virtual int getVarSize(Semantic::Type *type) = 0;
	virtual void updateRecordSize(int &size, Semantic::Type *newFieldType) = 0;
};

class DummyFrameManager: public AbstractFrameManager {
public:
	virtual DummyFrame *rootFrame()
	{
		return new DummyFrame;
	}
	
	virtual DummyFrame *newFrame(AbstractFrame *parent)
	{
		return new DummyFrame;
	}
	
	virtual int getVarSize(Semantic::Type *type)
	{
		return 0;
	}
	
	virtual void updateRecordSize(int &size, Semantic::Type *newFieldType)
	{
	}
};

}

namespace Semantic {

class VariablesAccessInfo {
private:
	IdMap info;
public:
	void processExpression(Syntax::Tree expression);
	bool isAccessedByAddress(Syntax::Tree definition);
};

}

#endif
