#include "translate_utils.h"
#include "layeredmap.h"

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

namespace Semantic {

struct VarAccessDefInfo {
	ObjectId object_id;
	ObjectId owner_func_id;
	bool is_function;
	
	/**
	 * If it actually describes a function not a variable
	 */
	bool func_needs_parent_fp;
	bool func_exports_parent_fp_to_children;
	
	VarAccessDefInfo(ObjectId _id, ObjectId _owner_func_id, bool _is_func) :
		object_id(_id), owner_func_id(_owner_func_id), is_function(_is_func),
		func_needs_parent_fp(false), func_exports_parent_fp_to_children(false) {}
};

class VariablesAccessInfoPrivate {
public:
	std::list<VarAccessDefInfo> variables, functions;
	LayeredMap variable_names;
	IdMap var_info;
	
	VariablesAccessInfoPrivate() {}
};

VariablesAccessInfo::VariablesAccessInfo()
{
	impl = new VariablesAccessInfoPrivate;
	func_stack.push_back(INVALID_OBJECT_ID); // program body "function"
}

VariablesAccessInfo::~VariablesAccessInfo()
{
	delete impl;
}

void VariablesAccessInfo::handleCall(Syntax::Tree function_exp,
	ObjectId current_function_id)
{
	return;
	if (function_exp->type != Syntax::IDENTIFIER)
		return;
	VarAccessDefInfo *callee_info = (VarAccessDefInfo *)impl->variable_names.
		lookup(((Syntax::Identifier *)function_exp)->name);
		
	if (callee_info == NULL)
		return;
	
	int callee_parent = callee_info->owner_func_id;
	int id = current_function_id;
	while ((id >= 0) && (id != callee_parent)) {
		VarAccessDefInfo *func_info =
			(VarAccessDefInfo *)impl->var_info.lookup(current_function_id);
		assert(func_info != NULL);
		func_info->func_needs_parent_fp = true;
	}
}


void VariablesAccessInfo::processDeclaration(Syntax::Tree declaration,
	ObjectId current_function_id)
{
	switch (declaration->type) {
		case Syntax::TYPEDECLARATION:
			break;
		case Syntax::VARDECLARATION:
			impl->variables.push_back(VarAccessDefInfo(
				((Syntax::VariableDeclaration *)declaration)->id,
				current_function_id, false));
			impl->variable_names.add(((Syntax::VariableDeclaration *)declaration)->name->name,
				&(impl->variables.back()));
			break;
		case Syntax::FUNCTION: {
			Syntax::Function *func_declaration = (Syntax::Function *)declaration;
			impl->functions.push_back(VarAccessDefInfo(
				func_declaration->id,
				current_function_id, true));
			impl->functions.back().func_needs_parent_fp = true;
			if (current_function_id >= 0) {
				VarAccessDefInfo *our_info = 
					(VarAccessDefInfo *)impl->var_info.lookup(current_function_id);
				assert(our_info != NULL);
				
				our_info->func_exports_parent_fp_to_children = true;
			}
			
			impl->variable_names.add(func_declaration->name->name,
				&(impl->functions.back()));
			impl->var_info.add(func_declaration->id, &(impl->functions.back()));
			
			if (func_declaration->body != NULL) {
				impl->variable_names.newLayer();
				for (Syntax::Tree param: func_declaration->parameters->expressions) {
					assert(param->type == Syntax::PARAMETERDECLARATION);
					impl->variables.push_back(VarAccessDefInfo(
						((Syntax::ParameterDeclaration *) param)->id,
						func_declaration->id, false));
					impl->variable_names.add(((Syntax::VariableDeclaration *) param)->name->name,
						&(impl->variables.back()));
				}
				func_stack.push_back(func_declaration->id);
				processExpression(func_declaration->body, func_declaration->id);
				func_stack.pop_back();
				impl->variable_names.removeLastLayer();
			}
			break;
		}
		default:
			Error::fatalError("Not a declaration inside scope declarations?", declaration->linenumber);
	}
}

void VariablesAccessInfo::processExpression(Syntax::Tree expression,
	ObjectId current_function_id)
{
	switch (expression->type) {
		case Syntax::IDENTIFIER: {
			VarAccessDefInfo *variable = (VarAccessDefInfo *)impl->variable_names.
				lookup(((Syntax::Identifier *)expression)->name);
			if ((variable != NULL) && (! variable->is_function) &&
					(variable->owner_func_id != current_function_id)) {
				impl->var_info.add(variable->object_id, variable);
				std::list<int>::reverse_iterator func_in_stack =
					func_stack.rbegin();
				while ((func_in_stack != func_stack.rend()) &&
						(*func_in_stack != variable->owner_func_id)) {
					VarAccessDefInfo *func_info =
						(VarAccessDefInfo *)impl->var_info.lookup(*func_in_stack);
					assert(func_info != NULL);
					func_info->func_needs_parent_fp = true;
					if (*func_in_stack != current_function_id)
						func_info->func_exports_parent_fp_to_children = true;
					func_in_stack++;
				}
			}
			break;
		}
		case Syntax::INTVALUE:
		case Syntax::STRINGVALUE:
		case Syntax::NIL:
		case Syntax::BREAK:
			break;
		case Syntax::BINARYOP:
			processExpression(((Syntax::BinaryOp *)expression)->left, current_function_id);
			processExpression(((Syntax::BinaryOp *)expression)->right, current_function_id);
			break;
		case Syntax::SEQUENCE: {
			for (Syntax::Tree child: ((Syntax::Sequence *)expression)->content->expressions)
				processExpression(child, current_function_id);
			break;
		}
		case Syntax::ARRAYINDEXING:
			processExpression(((Syntax::ArrayIndexing *)expression)->array, current_function_id);
			processExpression(((Syntax::ArrayIndexing *)expression)->index, current_function_id);
			break;
		case Syntax::ARRAYINSTANTIATION:
			processExpression(((Syntax::ArrayInstantiation *)expression)->arraydef->index, current_function_id);
			processExpression(((Syntax::ArrayInstantiation *)expression)->value, current_function_id);
			break;
		case Syntax::IF:
			processExpression(((Syntax::If *)expression)->condition, current_function_id);
			processExpression(((Syntax::If *)expression)->action, current_function_id);
			break;
		case Syntax::IFELSE:
			processExpression(((Syntax::IfElse *)expression)->condition, current_function_id);
			processExpression(((Syntax::IfElse *)expression)->action, current_function_id);
			processExpression(((Syntax::IfElse *)expression)->elseaction, current_function_id);
			break;
		case Syntax::WHILE:
			processExpression(((Syntax::While *)expression)->condition, current_function_id);
			processExpression(((Syntax::While *)expression)->action, current_function_id);
			break;
		case Syntax::FOR:
			impl->variables.push_back(VarAccessDefInfo(
				((Syntax::For *)expression)->variable_id, current_function_id, false));
			impl->variable_names.add(((Syntax::For *)expression)->variable->name,
				&(impl->variables.back()));
			processExpression(((Syntax::For *)expression)->start, current_function_id);
			processExpression(((Syntax::For *)expression)->stop, current_function_id);
			processExpression(((Syntax::For *)expression)->action, current_function_id);
			break;
		case Syntax::SCOPE: {
			impl->variable_names.newLayer();
			for (Syntax::Tree declaration: ((Syntax::Scope *)expression)->declarations->expressions)
				processDeclaration(declaration, current_function_id);
			for (Syntax::Tree body_expression: ((Syntax::Scope *)expression)->action->expressions)
				processExpression(body_expression, current_function_id);
			impl->variable_names.removeLastLayer();
			break;
		}
		case Syntax::RECORDFIELD:
			processExpression(((Syntax::RecordField *)expression)->record, current_function_id);
			break;
		case Syntax::FUNCTIONCALL:
			processExpression(((Syntax::FunctionCall *)expression)->function, current_function_id);
			handleCall(((Syntax::FunctionCall *)expression)->function, current_function_id); 
			for (Syntax::Tree child: ((Syntax::FunctionCall *)expression)->arguments->expressions)
				processExpression(child, current_function_id);
			break;
		case Syntax::RECORDINSTANTIATION: {
			for (Syntax::Tree field_def: ((Syntax::RecordInstantiation *)expression)->fieldvalues->expressions) {
				assert(field_def->type == Syntax::BINARYOP);
				Syntax::BinaryOp *fieldvalue = (Syntax::BinaryOp *) field_def;
				assert(fieldvalue->operation == SYM_ASSIGN);
				processExpression(fieldvalue->right, current_function_id);
			}
			break;
		}
		default:
			Error::fatalError("Unexpected syntax element :-/", expression->linenumber);
	}
}

bool VariablesAccessInfo::isAccessedByAddress(Syntax::Tree definition)
{
	int id;
	switch (definition->type) {
		case Syntax::VARDECLARATION:
			id = ((Syntax::VariableDeclaration *)definition)->id;
			break;
		case Syntax::PARAMETERDECLARATION:
			id = ((Syntax::ParameterDeclaration *)definition)->id;
			break;
		case Syntax::FOR:
			id = ((Syntax::For *)definition)->variable_id;
			break;
		default:
			Error::fatalError("Variable access checked for non-variable declaration",
				definition->linenumber);
	}
	return impl->var_info.lookup(id) != NULL;
}

bool VariablesAccessInfo::functionNeedsParentFp(Syntax::Function* definition)
{
	VarAccessDefInfo *info = (VarAccessDefInfo *)impl->var_info.lookup(definition->id);
	if (info == NULL)
		return false;
	else
		return info->func_needs_parent_fp;
}

bool VariablesAccessInfo::isFunctionParentFpAccessedByChildren(Syntax::Function* definition)
{
	VarAccessDefInfo *info = (VarAccessDefInfo *)impl->var_info.lookup(definition->id);
	if (info == NULL)
		return false;
	else
		return info->func_exports_parent_fp_to_children;
}

}
