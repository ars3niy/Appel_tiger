#include "translate_utils.h"

namespace IR {
	
AbstractVarLocation* AbstractFrame::addVariable(int size, bool cant_be_register)
{
	AbstractVarLocation *impl = createVariable(size, cant_be_register);
	variables.push_back(impl);
	return impl;
}

AbstractFrame::~AbstractFrame()
{
	for (std::list<AbstractVarLocation *>::iterator var = variables.begin();
			var != variables.end(); var++)
		delete *var;
}

}

namespace Semantic {

void VariablesAccessInfo::processExpression(Syntax::Tree expression)
{

}

bool VariablesAccessInfo::isAccessedByAddress(Syntax::Tree definition)
{
	return true;
}

}
