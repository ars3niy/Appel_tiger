#include "frame.h"

namespace IR {

AbstractVarLocation* AbstractFrame::addVariable(const std::string &name,
	int size, bool cant_be_register)
{
	AbstractVarLocation *impl = createVariable(name, size, cant_be_register);
	variables.push_back(impl);
	return impl;
}

AbstractVarLocation* AbstractFrame::addParameter(const std::string &name,
	int size, bool cant_be_register)
{
	AbstractVarLocation *impl = createParameter(name, size);
	parameters.push_back(impl);
	
	if (impl->isRegister() && cant_be_register) {
		AbstractVarLocation *storage = addVariable(name + "::.store." + name,
			size, true);
		parameter_store_prologue.push_back(ParameterMovement());
		parameter_store_prologue.back().parameter = impl;
		parameter_store_prologue.back().where_store = storage;
		return storage;
	}
	return impl;
}

void AbstractFrame::addParentFpParamVariable(bool cant_be_register)
{
 	parent_fp_parameter = createVariable(".parent_fp",
		framemanager->getPointerSize(), false);
	if (parent_fp_parameter->isRegister() && cant_be_register) {
		parent_fp_memory_storage = addVariable(name + "::.store." + name,
			framemanager->getPointerSize(), true);
		parameter_store_prologue.push_back(ParameterMovement());
		parameter_store_prologue.back().parameter = parent_fp_parameter;
		parameter_store_prologue.back().where_store = parent_fp_memory_storage;
		parent_fp_parameter->prespillRegister(parent_fp_memory_storage);
	}
}

AbstractFrame::~AbstractFrame()
{
	for (AbstractVarLocation *var: variables)
		delete var;
}

}
