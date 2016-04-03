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
	for (std::list<AbstractVarLocation *>::iterator var = variables.begin();
			var != variables.end(); var++)
		delete *var;
}

}

namespace Semantic {

struct VarAccessDefInfo {
	ObjectId object_id;
	ObjectId owner_func_id;
	
	/**
	 * If it actually describes a function not a variable
	 */
	bool func_needs_parent_fp;
	bool func_exports_parent_fp_to_children;
	
	VarAccessDefInfo(ObjectId _id, ObjectId _owner_func_id) :
		object_id(_id), owner_func_id(_owner_func_id),
		func_needs_parent_fp(false), func_exports_parent_fp_to_children(false) {}
};

class VariablesAccessInfoPrivate {
public:
	std::list<VarAccessDefInfo> variables, functions;
	LayeredMap variable_names;
	VarAccessDefInfo function_info;
	IdMap var_info;
	
	VariablesAccessInfoPrivate() : function_info(INVALID_OBJECT_ID, INVALID_OBJECT_ID) {}
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

void VariablesAccessInfo::processDeclaration(Syntax::Tree declaration,
	ObjectId current_function_id)
{
	switch (declaration->type) {
		case Syntax::TYPEDECLARATION:
			break;
		case Syntax::VARDECLARATION:
			impl->variables.push_back(VarAccessDefInfo(
				((Syntax::VariableDeclaration *)declaration)->id,
				current_function_id));
			impl->variable_names.add(((Syntax::VariableDeclaration *)declaration)->name->name,
				&(impl->variables.back()));
			break;
		case Syntax::FUNCTION: {
			Syntax::Function *func_declaration = (Syntax::Function *)declaration;
			impl->variable_names.add(func_declaration->name->name,
				&(impl->function_info));
			impl->variable_names.newLayer();
			for (std::list<Syntax::Tree>::iterator param = func_declaration->parameters->expressions.begin();
					param != func_declaration->parameters->expressions.end();
					param++) {
				assert((*param)->type == Syntax::PARAMETERDECLARATION);
				impl->variables.push_back(VarAccessDefInfo(
					((Syntax::ParameterDeclaration *) *param)->id,
					func_declaration->id));
				impl->variable_names.add(((Syntax::VariableDeclaration *) *param)->name->name,
					&(impl->variables.back()));
			}
			func_stack.push_back(func_declaration->id);
			processExpression(func_declaration->body, func_declaration->id);
			func_stack.pop_back();
			impl->variable_names.removeLastLayer();
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
			if ((variable != NULL) && (variable->object_id != INVALID_OBJECT_ID) &&
					(variable->owner_func_id != current_function_id)) {
				impl->var_info.add(variable->object_id, expression);
				std::list<int>::reverse_iterator func_in_stack =
					func_stack.rbegin();
				while ((func_in_stack != func_stack.rend()) &&
						(*func_in_stack != variable->owner_func_id)) {
					VarAccessDefInfo *func_info =
						(VarAccessDefInfo *)impl->var_info.lookup(*func_in_stack);
					if (func_info == NULL) {
						impl->functions.push_back(VarAccessDefInfo(
							INVALID_OBJECT_ID, INVALID_OBJECT_ID));
						func_info = & impl->functions.back();
						impl->var_info.add(*func_in_stack, func_info);
					}
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
			for (std::list<Syntax::Tree>::iterator child =
					((Syntax::Sequence *)expression)->content->expressions.begin();
					child != ((Syntax::Sequence *)expression)->content->expressions.end();
					child++)
				processExpression(*child, current_function_id);
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
				((Syntax::For *)expression)->variable_id, current_function_id));
			impl->variable_names.add(((Syntax::For *)expression)->variable->name,
				&(impl->variables.back()));
			processExpression(((Syntax::For *)expression)->start, current_function_id);
			processExpression(((Syntax::For *)expression)->stop, current_function_id);
			processExpression(((Syntax::For *)expression)->action, current_function_id);
			break;
		case Syntax::SCOPE: {
			impl->variable_names.newLayer();
			for (std::list<Syntax::Tree>::iterator declaration =
					((Syntax::Scope *)expression)->declarations->expressions.begin();
					declaration != ((Syntax::Scope *)expression)->declarations->expressions.end();
					declaration++)
				processDeclaration(*declaration, current_function_id);
			for (std::list<Syntax::Tree>::iterator body_expression =
					((Syntax::Scope *)expression)->action->expressions.begin();
					body_expression != ((Syntax::Scope *)expression)->action->expressions.end();
					body_expression++)
				processExpression(*body_expression, current_function_id);
			impl->variable_names.removeLastLayer();
			break;
		}
		case Syntax::RECORDFIELD:
			processExpression(((Syntax::RecordField *)expression)->record, current_function_id);
			break;
		case Syntax::FUNCTIONCALL:
			processExpression(((Syntax::FunctionCall *)expression)->function, current_function_id);
			for (std::list<Syntax::Tree>::iterator child =
					((Syntax::FunctionCall *)expression)->arguments->expressions.begin();
					child != ((Syntax::FunctionCall *)expression)->arguments->expressions.end();
					child++)
				processExpression(*child, current_function_id);
			break;
		case Syntax::RECORDINSTANTIATION: {
			for (std::list<Syntax::Tree>::iterator field_def =
					((Syntax::RecordInstantiation *)expression)->fieldvalues->expressions.begin();
					field_def != ((Syntax::RecordInstantiation *)expression)->fieldvalues->expressions.end();
					field_def++) {
				assert((*field_def)->type == Syntax::BINARYOP);
				Syntax::BinaryOp *fieldvalue = (Syntax::BinaryOp *) *field_def;
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
