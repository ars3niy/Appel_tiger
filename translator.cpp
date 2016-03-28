#include "translator.h"
#include "errormsg.h"
#include "intermediate.h"
#include "translate_utils.h"
#include <list>
#include <set>
#include <string.h>

namespace Semantic {

class TypesEnvironment {
private:
	std::list<Type*> types;
	LayeredMap typenames;
	std::list<ForwardReferenceType *> unknown_types;
	Type *int_type, *looped_int, *string_type, *error_type, *void_type, *nil_type;
	IdTracker id_provider;
	IR::AbstractFrameManager *framemanager;
	
	Type *createArrayType(Syntax::ArrayTypeDefinition *definition, bool allow_forward_references);
	Type *createRecordType(Syntax::RecordTypeDefinition *definition, bool allow_forward_references);
public:
	TypesEnvironment(IR::AbstractFrameManager *_framemanager);
	~TypesEnvironment();
	
	Type *getIntType() {return int_type;}
	Type *getLoopIntType() {return looped_int;}
	Type *getStringType() {return string_type;}
	Type *getErrorType() {return error_type;}
	Type *getVoidType() {return void_type;}
	Type *getNilType() {return nil_type;}
	Type *getPointerType() {return string_type;}
	Type *getType(Syntax::Tree definition, bool allow_forward_references);
	
	void newLayer() {typenames.newLayer();}
	void removeLastLayer() {typenames.removeLastLayer();}
	void processTypeDeclarationBatch(std::list<Syntax::Tree>::iterator begin,
		std::list<Syntax::Tree>::iterator end);
};

class TranslatorPrivate {
private:
	LayeredMap func_and_var_names;
	std::list<Variable> variables;
	std::list<Function> functions;
	TypesEnvironment *type_environment;
	Variable *undefined_variable;
	IR::IREnvironment IRenvironment;
	
	Function *getmem_func, *getmem_fill_func;
	
	void newLayer();
	void removeLastLayer();
	void processDeclarations(Syntax::ExpressionList *declarations,
		IR::AbstractFrame *currentFrame, std::list<Variable *> &new_vars);
	Declaration *findVariableOrFunction(Syntax::Identifier *id);

	void processVariableDeclaration(Syntax::VariableDeclaration *declaration,
		IR::AbstractFrame *currentFrame, std::list<Variable *> &new_vars);
	void processFunctionDeclarationBatch(std::list<Syntax::Tree>::iterator begin,
		std::list<Syntax::Tree>::iterator end, IR::AbstractFrame *currentFrame);
	
	void translateIntValue(Syntax::IntValue *expression, IR::Code *&translated);
	void translateStringValue(Syntax::StringValue *expression, IR::Code *&translated);
	void translateIdentifier(Syntax::Identifier *expression, IR::Code *&translated,
		Type *&type, IR::AbstractFrame *currentFrame);
	Type *getOpResultType(Type *leftType, Type *rightType,
		Syntax::BinaryOp *expression);
	void translateBinaryOperation(Syntax::BinaryOp *expression,
		IR::Code *&translated, Type *&type, IR::Label *last_loop_exit,
		IR::AbstractFrame *currentFrame);
	void translateSequence(const std::list<Syntax::Tree> &expressions,
		IR::Code *&translated, Type *&type, IR::Label *last_loop_exit,
		IR::AbstractFrame *currentFrame, IR::StatementSequence *existing_sequence = NULL);
	void translateArrayIndexing(Syntax::ArrayIndexing *expression,
		IR::Code *&translated, Type *&type, IR::Label *last_loop_exit,
		IR::AbstractFrame *currentFrame);
	void translateArrayInstantiation(Syntax::ArrayInstantiation *expression,
		IR::Code *&translated, Type *&type, IR::Label *last_loop_exit,
		IR::AbstractFrame *currentFrame);
	void translateIf(Syntax::If *expression, IR::Code *&translated,
		Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame);
	void translateIfElse(Syntax::IfElse *expression, IR::Code *&translated,
		Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame);
	void translateWhile(Syntax::While *expression, IR::Code *&translated,
		Type *&type, IR::AbstractFrame *currentFrame);
	void translateFor(Syntax::For *expression, IR::Code *&translated,
		Type *&type, IR::AbstractFrame *currentFrame);
	void translateScope(Syntax::Scope *expression, IR::Code *&translated,
		Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame);
	void translateRecordField(Syntax::RecordField *expression, IR::Code *&translated,
		Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame);
	void makeCallCode(Function *function, std::list<IR::Code *>arguments,
		IR::Code *&result);
	void translateFunctionCall(Syntax::FunctionCall *expression, IR::Code *&translated,
		Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame);
	void translateRecordInstantiation(Syntax::RecordInstantiation *expression, IR::Code *&translated,
		Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame);
public:
	IR::AbstractFrameManager *framemanager;
	VariablesAccessInfo variables_extra_info;
	
	TranslatorPrivate(IR::AbstractFrameManager *_framemanager);
	~TranslatorPrivate();
	
	void translateExpression(Syntax::Tree expression,
		IR::Code *&translated, Type *&type,
		IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame);
};

TypesEnvironment::TypesEnvironment(IR::AbstractFrameManager *_framemanager)
{
	framemanager = _framemanager;
	int_type = new Type(TYPE_INT);
	looped_int = new Type(TYPE_INT);
	looped_int->is_loop_variable = true;
	string_type = new Type(TYPE_STRING);
	nil_type = new Type(TYPE_NIL);
	void_type = new Type(TYPE_VOID);
	error_type = new Type(TYPE_ERROR);
	
	types.push_back(int_type);
	types.push_back(string_type);
	types.push_back(nil_type);
	types.push_back(void_type);
	types.push_back(error_type);
	
	typenames.add("int", int_type);
	typenames.add("string", string_type);
}

TypesEnvironment::~TypesEnvironment()
{
	delete int_type;
	delete looped_int;
	delete string_type;
	delete nil_type;
	delete void_type;
	delete error_type;
	
	for (std::list<Type*>::iterator i = types.begin(); i != types.end(); i++)
		delete *i;
}

Type *TypesEnvironment::createArrayType(Syntax::ArrayTypeDefinition *definition,
	bool allow_forward_references)
{
	ArrayType *type = new ArrayType(id_provider.getId(), getType(definition->elementtype,
		allow_forward_references
	));
	types.push_back(type);
	return type;
}

Type *TypesEnvironment::createRecordType(Syntax::RecordTypeDefinition *definition,
	bool allow_forward_references)
{
	RecordType *type = new RecordType(id_provider.getId());
	types.push_back(type);
	
	for (std::list<Syntax::Tree>::iterator i = definition->fields->expressions.begin();
	     i != definition->fields->expressions.end(); i++) {
		assert((*i)->type == Syntax::PARAMETERDECLARATION);
		Syntax::ParameterDeclaration *param = (Syntax::ParameterDeclaration *)*i;
		type->addField(param->name->name, getType(param->type,
			allow_forward_references));
	}
	
	return type;
}

Type *TypesEnvironment::getType(Syntax::Tree definition,
	bool allow_forward_references)
{
	switch (definition->type) {
		case Syntax::ARRAYTYPEDEFINITION:
			return createArrayType((Syntax::ArrayTypeDefinition *)definition,
				allow_forward_references);
		case Syntax::RECORDTYPEDEFINITION:
			return createRecordType((Syntax::RecordTypeDefinition *)definition,
				allow_forward_references);
		case Syntax::IDENTIFIER: {
			std::string &identifier = ((Syntax::Identifier *)definition)->name;
			Semantic::Type *type = (Semantic::Type *)typenames.lookup(identifier);
			if (type == NULL) {
				if (! allow_forward_references) {
					Error::error(std::string("Undefined type ") + ((Syntax::Identifier *)definition)->name);
					return error_type;
				}
				type = new ForwardReferenceType(identifier, definition);
				unknown_types.push_back((ForwardReferenceType *)type);
				typenames.add(identifier, type);
			}
			return type;
		}
		default:
			Error::fatalError("TypesEnvironment::createComplexType got not a type definition",
				definition->linenumber);
	}
}

void TypesEnvironment::processTypeDeclarationBatch(std::list<Syntax::Tree>::iterator begin,
		std::list<Syntax::Tree>::iterator end)
{
	unknown_types.clear();
	std::list<Type *> new_records;
	for (std::list<Syntax::Tree>::iterator i = begin; i != end; i++) {
		assert((*i)->type == Syntax::TYPEDECLARATION);
		Syntax::TypeDeclaration *declaration = (Syntax::TypeDeclaration *)*i;
		Type *type = getType(declaration->definition, true);
		assert(type != NULL);
		if (type->basetype == TYPE_RECORD)
			new_records.push_back(type);
		Type *existing = (Type *)typenames.lookup_last_layer(declaration->name->name);
		if (existing != NULL) {
			if ((existing->basetype == TYPE_NAMEREFERENCE) &&
				(((ForwardReferenceType *)existing)->meaning == NULL)) {
				((ForwardReferenceType *)existing)->meaning = type;
			} else {
				Error::error(std::string("Type redefined in the same scope: ") +
					declaration->name->name, declaration->name->linenumber);
			}
		} else
			typenames.add(declaration->name->name, type);
	}
	
	for (std::list<ForwardReferenceType *>::iterator t = unknown_types.begin();
		 t != unknown_types.end(); t++) {
		if ((*t)->meaning == NULL) {
			Error::error(std::string("Type not defined in this scope: ") + (*t)->name,
						 (*t)->reference_position->linenumber);
			(*t)->meaning = error_type;
		}
	}
		
	for (std::list<ForwardReferenceType *>::iterator t = unknown_types.begin();
		 t != unknown_types.end(); t++) {
		std::set<ForwardReferenceType *> visited_types;
		ForwardReferenceType *current = *t;
		while (current->meaning->basetype == TYPE_NAMEREFERENCE) {
			visited_types.insert(current);
			ForwardReferenceType *next = (ForwardReferenceType *)current->meaning;
			if (visited_types.find(next) != visited_types.end()) {
				Error::error(std::string("Type ") + current->name +
					std::string(" being defined as ") + 
					next->name + " creates a circular reference",
					current->reference_position->linenumber);
				current->meaning = error_type;
				break;
			}
			current = next;
		}
	}
	
	for (std::list<Type *>::iterator rec = new_records.begin();
			rec != new_records.end(); rec++) {
		assert((*rec)->basetype == TYPE_RECORD);
		RecordType *record = (RecordType *) *rec;
		record->data_size = 0;
		for (RecordType::FieldsList::iterator field = record->field_list.begin();
				field != record->field_list.end(); field++) {
			(*field).offset = record->data_size;
		framemanager->updateRecordSize(record->data_size, (*field).type->resolve());
		}
	}
	
	unknown_types.clear();
}

TranslatorPrivate::TranslatorPrivate(IR::AbstractFrameManager * _framemanager)
{
	framemanager = _framemanager;
	type_environment = new TypesEnvironment(_framemanager);
	
	undefined_variable = new Variable("undefined", type_environment->getErrorType(),
		NULL, NULL);
	functions.push_back(Function("print", type_environment->getVoidType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment.addLabel("print")));
	functions.back().addArgument("s", type_environment->getStringType(), NULL);
	func_and_var_names.add("print", &(functions.back()));

	functions.push_back(Function("flush", type_environment->getVoidType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment.addLabel("flush")));
	func_and_var_names.add("flush", &(functions.back()));

	functions.push_back(Function("getchar", type_environment->getStringType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment.addLabel("getchar")));
	func_and_var_names.add("getchar", &(functions.back()));

	functions.push_back(Function("ord", type_environment->getIntType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment.addLabel("ord")));
	functions.back().addArgument("s", type_environment->getStringType(), NULL);
	func_and_var_names.add("ord", &(functions.back()));

	functions.push_back(Function("chr", type_environment->getStringType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment.addLabel("chr")));
	functions.back().addArgument("i", type_environment->getIntType(), NULL);
	func_and_var_names.add("chr", &(functions.back()));

	functions.push_back(Function("size", type_environment->getIntType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment.addLabel("size")));
	functions.back().addArgument("s", type_environment->getStringType(), NULL);
	func_and_var_names.add("size", &(functions.back()));

	functions.push_back(Function("substring", type_environment->getStringType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment.addLabel("substring")));
	functions.back().addArgument("s", type_environment->getStringType(), NULL);
	functions.back().addArgument("first", type_environment->getIntType(), NULL);
	functions.back().addArgument("n", type_environment->getIntType(), NULL);
	func_and_var_names.add("substring", &(functions.back()));

	functions.push_back(Function("concat", type_environment->getStringType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment.addLabel("concat")));
	functions.back().addArgument("s1", type_environment->getStringType(), NULL);
	functions.back().addArgument("s2", type_environment->getStringType(), NULL);
	func_and_var_names.add("concat", &(functions.back()));

	functions.push_back(Function("not", type_environment->getIntType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment.addLabel("not")));
	functions.back().addArgument("i", type_environment->getIntType(), NULL);
	func_and_var_names.add("not", &(functions.back()));

	functions.push_back(Function("exit", type_environment->getVoidType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment.addLabel("exit")));
	functions.back().addArgument("i", type_environment->getIntType(), NULL);
	func_and_var_names.add("exit", &(functions.back()));

	functions.push_back(Function("getmem", type_environment->getIntType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment.addLabel("getmem")));
	functions.back().addArgument("size", type_environment->getIntType(), NULL);
	func_and_var_names.add("getmem", &(functions.back()));
	getmem_func = &(functions.back());

	functions.push_back(Function("getmem_fill", type_environment->getIntType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment.addLabel("getmem_fill")));
	functions.back().addArgument("elemcount", type_environment->getIntType(), NULL);
	functions.back().addArgument("value", type_environment->getIntType(), NULL);
	func_and_var_names.add("getmem", &(functions.back()));
	getmem_fill_func = &(functions.back());
}

TranslatorPrivate::~TranslatorPrivate()
{
	for (std::list<Variable>::iterator var = variables.begin();
		 var != variables.end(); var++)
		 delete (*var).value;
	for (std::list<Function>::iterator func = functions.begin();
		 func != functions.end(); func++)
		 delete (*func).body;
	delete undefined_variable->value;
	delete undefined_variable;
	delete type_environment;
}

void TranslatorPrivate::newLayer()
{
	type_environment->newLayer();
	func_and_var_names.newLayer();
}

void TranslatorPrivate::removeLastLayer()
{
	type_environment->removeLastLayer();
	func_and_var_names.removeLastLayer();
}

std::string TypeToStr(Type *type)
{
	char res[32];
	sprintf(res, "%d", (int)type->basetype);
	return std::string(res);
}

bool IsInt(Type *type)
{
	return (type->basetype == TYPE_INT) || (type->basetype == TYPE_ERROR);
}

bool IsArray(Type *type)
{
	return (type->basetype == TYPE_ARRAY) || (type->basetype == TYPE_ERROR);
}

bool IsRecord(Type *type)
{
	return (type->basetype == TYPE_RECORD) || (type->basetype == TYPE_ERROR);
}

bool CheckAssignmentTypes(Type *left, Type *right)
{
	if (right->basetype == TYPE_ERROR)
		return true;
	switch (left->basetype) {
		case TYPE_INT:
		case TYPE_STRING:
			return right->basetype == left->basetype;
		case TYPE_ARRAY:
			return (right->basetype == left->basetype) &&
				((ArrayType *)left)->id ==
				((ArrayType *)right)->id;
		case TYPE_RECORD:
			return 
				(right->basetype == TYPE_NIL) ||
				(right->basetype == left->basetype) &&
				((RecordType *)left)->id ==
				((RecordType *)right)->id;
		case TYPE_ERROR:
			return true;
		default:
			Error::fatalError(std::string("Not supposed to assign to type ") + TypeToStr(left));
	}
}

void TranslatorPrivate::processVariableDeclaration(
	Syntax::VariableDeclaration *declaration, IR::AbstractFrame *currentFrame,
	std::list<Variable *> &new_vars)
{
	Type *vartype;
	if (declaration->type != NULL)
		vartype = type_environment->getType(declaration->type, false)->resolve();
	else
		vartype = NULL;
	Type *exprtype;
	IR::Code *translatedValue;
	translateExpression(declaration->value, translatedValue, exprtype, NULL,
		currentFrame);
	if (exprtype->basetype == TYPE_VOID) {
		Error::error("Statement without value assigned to a variable",
					 declaration->linenumber);
		exprtype = type_environment->getErrorType();
	}
	if (vartype == NULL) {
		if (exprtype->basetype == TYPE_NIL) {
			Error::error("Variable without explicit type cannot be initializod to nil",
						declaration->linenumber);
			exprtype = type_environment->getErrorType();
		}
		vartype = exprtype;
	} else
		if (! CheckAssignmentTypes(vartype, exprtype))
			Error::error("Incompatible type for variable initialization",
				declaration->linenumber);
	variables.push_back(Variable(declaration->name->name, vartype,
		translatedValue,
		currentFrame->addVariable(framemanager->getVarSize(vartype),
			variables_extra_info.isAccessedByAddress(declaration))));
	func_and_var_names.add(declaration->name->name, &(variables.back()));
	new_vars.push_back(&(variables.back()));
}

void TranslatorPrivate::processFunctionDeclarationBatch(
	std::list<Syntax::Tree>::iterator begin,
	std::list<Syntax::Tree>::iterator end, IR::AbstractFrame *currentFrame)
{
	std::list<Function *> recent_functions;
	for (std::list<Syntax::Tree>::iterator f = begin; f != end; f++) {
		assert((*f)->type == Syntax::FUNCTION);
		Syntax::Function *declaration = (Syntax::Function *) *f;
		Type *return_type;
		if (declaration->type == NULL)
			return_type = type_environment->getVoidType();
		else
			return_type = type_environment->getType(declaration->type, false)->resolve();
		functions.push_back(Function(declaration->name->name, return_type,
			declaration->body, NULL,
			framemanager->newFrame(currentFrame), IRenvironment.addLabel()));
		Function *function = &(functions.back());
		function->frame->addVariable(framemanager->getVarSize(
			type_environment->getPointerType()), true);
		func_and_var_names.add(declaration->name->name, function);
		recent_functions.push_back(function);
		
		for (std::list<Syntax::Tree>::iterator param =
				declaration->parameters->expressions.begin();
				param != declaration->parameters->expressions.end(); param++) {
			assert((*param)->type == Syntax::PARAMETERDECLARATION);
			Syntax::ParameterDeclaration *param_decl = 
				(Syntax::ParameterDeclaration *) *param;
			Type *param_type = type_environment->getType(param_decl->type, false)->resolve();
			function->addArgument(param_decl->name->name, param_type,
				function->frame->addVariable(
					framemanager->getVarSize(param_type),
					variables_extra_info.isAccessedByAddress(param_decl)
				)
			);
		}
	}
	for (std::list<Function *>::iterator fcn = recent_functions.begin();
		 fcn != recent_functions.end(); fcn++) {
		Type *actual_return_type;
		func_and_var_names.newLayer();
		for (std::list<FunctionArgument>::iterator param = (*fcn)->arguments.begin();
				param != (*fcn)->arguments.end(); param++)
			func_and_var_names.add((*param).name, &(*param));
		translateExpression((*fcn)->raw_body, ((*fcn)->body), actual_return_type,
			NULL, (*fcn)->frame);
		func_and_var_names.removeLastLayer();
		if (((*fcn)->return_type->basetype != TYPE_VOID) &&
			! CheckAssignmentTypes((*fcn)->return_type, actual_return_type))
			Error::error("Type of function body doesn't match specified return type",
				(*fcn)->raw_body->linenumber);
	}
}

void TranslatorPrivate::processDeclarations(
	Syntax::ExpressionList *declarations, IR::AbstractFrame *currentFrame,
	std::list<Variable *> &new_vars)
{
	std::list<Syntax::Tree>::iterator d = declarations->expressions.begin();
	std::list<Syntax::Tree>::iterator end = declarations->expressions.end();
	while (d != end) {
		Syntax::Tree declaration = *d;
		Syntax::NodeType dectype = declaration->type;
		if (dectype == Syntax::VARDECLARATION) {
			processVariableDeclaration((Syntax::VariableDeclaration *)declaration,
				currentFrame, new_vars);
			d++;
		} else if ((dectype == Syntax::FUNCTION) || 
			       (dectype == Syntax::TYPEDECLARATION)) {
			std::list<Syntax::Tree>::iterator batch_begin = d;
			while ((d != end) && ((*d)->type == dectype))
				d++;
			if (dectype == Syntax::FUNCTION) {
				processFunctionDeclarationBatch(batch_begin, d, currentFrame);
			} else {
				type_environment->processTypeDeclarationBatch(batch_begin, d);
			}
		} else
			Error::fatalError("Not a declaration inside declaration block", declaration->linenumber);
	}
}

Declaration *TranslatorPrivate::findVariableOrFunction(Syntax::Identifier *id)
{
	Declaration *result = (Declaration *)func_and_var_names.lookup(id->name);
	if (result == NULL) {
		Error::error("Undefined variable or function " + id->name, id->linenumber);
		result = undefined_variable;
	}
	return result;
}

void TranslatorPrivate::translateIntValue(Syntax::IntValue *expression,
	IR::Code *&translated)
{
	translated = new IR::ExpressionCode(new IR::IntegerExpression(expression->value));
}

void TranslatorPrivate::translateStringValue(Syntax::StringValue *expression,
	IR::Code *&translated)
{
	IR::Blob *stringdata = IRenvironment.addBlob();
	int intsize = framemanager->getVarSize(type_environment->getIntType());
	stringdata->data.resize(expression->value.size() + intsize);
	*((int *)stringdata->data.data()) = expression->value.size();
	memmove(stringdata->data.data() + intsize, expression->value.c_str(), 
		expression->value.size());
	translated = new IR::ExpressionCode(new IR::LabelAddressExpression(
		stringdata->label));
}

IR::Code *ErrorPlaceholderCode()
{
	return new IR::ExpressionCode(NULL);
}

void TranslatorPrivate::translateIdentifier(Syntax::Identifier *expression,
	IR::Code *&translated, Type *&type, IR::AbstractFrame *currentFrame)
{
	Declaration *var_or_function = (Declaration *)func_and_var_names.lookup(
		expression->name);
	if (var_or_function == NULL) {
		Error::error("Undeclared variable/function identifier", expression->linenumber);
		type = type_environment->getErrorType();
		translated = ErrorPlaceholderCode();
		return;
	}
	if (var_or_function->kind == DECL_FUNCTION) {
		Error::error("Function used as a variable", expression->linenumber);
		type = type_environment->getErrorType();
		translated = ErrorPlaceholderCode();
	} else {
		type = ((Variable *)var_or_function)->type->resolve();
		translated = ((Variable *)var_or_function)->implementation->createCode(currentFrame);
	}
}

bool CheckComparisonTypes(Type *left, Type *right)
{
	if ((left->basetype == TYPE_ERROR) || (right->basetype == TYPE_ERROR))
		return true;
	switch (left->basetype) {
		case TYPE_INT:
		case TYPE_STRING:
			return right->basetype == left->basetype;
		case TYPE_ARRAY:
			return (right->basetype == left->basetype) &&
				(((ArrayType *)left)->id == ((ArrayType *)right)->id);
		case TYPE_RECORD:
			return (right->basetype == TYPE_NIL) ||
				(right->basetype == left->basetype) &&
				(((RecordType *)left)->id == ((RecordType *)right)->id);
		case TYPE_NIL:
			return right->basetype == TYPE_RECORD;
	}
}

Type *TranslatorPrivate::getOpResultType(Type *leftType, Type *rightType,
	Syntax::BinaryOp *expression)
{
	switch (expression->operation) {
		case SYM_PLUS:
		case SYM_MINUS:
		case SYM_ASTERISK:
		case SYM_SLASH:
		case SYM_AND:
		case SYM_OR:
			if (! IsInt(leftType))
				Error::error("Operand for arithmetic is not integer",
							 expression->left->linenumber);
			if (! IsInt(rightType))
				Error::error("Operand for arithmetic is not integer",
							 expression->right->linenumber);
			return type_environment->getIntType();
			break;
		case SYM_EQUAL:
		case SYM_NONEQUAL:
			if (! CheckComparisonTypes(leftType, rightType))
				Error::error("Incompatible types for equality comparison",
							 expression->linenumber);
			return type_environment->getIntType();
		case SYM_LESS:
		case SYM_LESSEQUAL:
		case SYM_GREATER:
		case SYM_GREATEQUAL:
			if ((leftType->basetype != TYPE_INT) && (leftType->basetype != TYPE_STRING))
				Error::error("Operand for arithmetic is not integer",
							 expression->left->linenumber);
			if ((rightType->basetype != TYPE_INT) && (rightType->basetype != TYPE_STRING))
				Error::error("Operand for arithmetic is not integer",
							 expression->right->linenumber);
			if (leftType->basetype != rightType->basetype)
				Error::error("Incompatible types for comparison",
							 expression->right->linenumber);
			return type_environment->getIntType();
		case SYM_ASSIGN:
			if (! CheckAssignmentTypes(leftType, rightType))
				Error::error("Incompatible types for assignment",
							 expression->linenumber);
			if (leftType->is_loop_variable)
				Error::error("Assignment to for loop variable",
							 expression->left->linenumber);
			return type_environment->getVoidType();
		default:
			Error::fatalError("I forgot to handle this operation",
				expression->left->linenumber);
	}
}

IR::BinaryOp getIRBinaryOp(yytokentype op_token)
{
	switch (op_token) {
		case SYM_PLUS:
			return IR::OP_PLUS;
		case SYM_MINUS:
			return IR::OP_MINUS;
		case SYM_ASTERISK:
			return IR::OP_MUL;
		case SYM_SLASH:
			return IR::OP_DIV;
	}
}

IR::ComparisonOp getIRComparisonOp(yytokentype op_token)
{
	switch (op_token) {
		case SYM_EQUAL:
			return IR::OP_EQUAL;
		case SYM_NONEQUAL:
			return IR::OP_NONEQUAL;
		case SYM_LESS:
			return IR::OP_LESS;
		case SYM_LESSEQUAL:
			return IR::OP_LESSEQUAL;
		case SYM_GREATER:
			return IR::OP_GREATER;
		case SYM_GREATEQUAL:
			return IR::OP_GREATEQUAL;
	}
}

void TranslatorPrivate::translateBinaryOperation(Syntax::BinaryOp *expression,
	IR::Code *&translated, Type *&type, IR::Label *last_loop_exit,
	IR::AbstractFrame *currentFrame)
{
	IR::Code *left, *right;
	Type *leftType, *rightType;
	translateExpression(expression->left, left, leftType, last_loop_exit, currentFrame);
	translateExpression(expression->right, right, rightType, last_loop_exit, currentFrame);
	bool bullshit = false;
	if (leftType->basetype == TYPE_VOID) {
		Error::error("Statement without value used in expression",
					 expression->left->linenumber);
		bullshit = true;
	}
	if (rightType->basetype == TYPE_VOID) {
		Error::error("Statement without value used in expression",
					 expression->right->linenumber);
		bullshit = true;
	}
	if (bullshit || (leftType->basetype == TYPE_ERROR) || (rightType->basetype == TYPE_ERROR)) {
		type = type_environment->getErrorType();
		translated = ErrorPlaceholderCode();
		return;
	}
	type = getOpResultType(leftType, rightType, expression);
	switch (expression->operation) {
		case SYM_PLUS:
		case SYM_MINUS:
		case SYM_ASTERISK:
		case SYM_SLASH: {
			IR::Expression *left_expr = IRenvironment.killCodeToExpression(left);
			IR::Expression *right_expr = IRenvironment.killCodeToExpression(right);
			translated = new IR::ExpressionCode(new IR::BinaryOpExpression(
				getIRBinaryOp(expression->operation), left_expr, right_expr));
			break;
		}
		case SYM_EQUAL:
		case SYM_NONEQUAL:
		case SYM_LESS:
		case SYM_LESSEQUAL:
		case SYM_GREATER:
		case SYM_GREATEQUAL: {
			IR::Expression *left_expr = IRenvironment.killCodeToExpression(left);
			IR::Expression *right_expr = IRenvironment.killCodeToExpression(right);
			IR::CondJumpStatement *compare_statm = new IR::CondJumpStatement(
				getIRComparisonOp(expression->operation), left_expr, right_expr,
				NULL, NULL);
			IR::CondJumpPatchesCode *result = new IR::CondJumpPatchesCode(compare_statm);
			result->replace_true.push_back(&compare_statm->true_dest);
			result->replace_false.push_back(&compare_statm->false_dest);
			translated = result;
			break;
		}
		case SYM_ASSIGN: {
			IR::Expression *left_expr = IRenvironment.killCodeToExpression(left);
			IR::Expression *right_expr = IRenvironment.killCodeToExpression(right);
			translated = new IR::StatementCode(new IR::MoveStatement(
				left_expr, right_expr));
			break;
		}
		default:
			Error::fatalError("I forgot to handle this operation",
				expression->left->linenumber);
	}
}

void TranslatorPrivate::translateSequence(const std::list<Syntax::Tree> &expressions,
	IR::Code *&translated, Type *&type, IR::Label *last_loop_exit,
	IR::AbstractFrame *currentFrame, IR::StatementSequence *existing_sequence)
{
	type = type_environment->getVoidType();
	IR::StatementSequence *sequence = existing_sequence;
	if (sequence == NULL)
		sequence = new IR::StatementSequence;
	IR::Expression *last_expression = NULL;
	for (std::list<Syntax::Tree>::const_iterator e = expressions.begin();
			e != expressions.end();
			e++) {
		std::list<Syntax::Tree>::const_iterator next = e;
		next++;
		IR::Code *item_code;
		translateExpression(*e, item_code, type, last_loop_exit, currentFrame);
		if ((next != expressions.end()) || type->basetype == TYPE_VOID)
			sequence->addStatement(IRenvironment.killCodeToStatement(item_code));
		else
			last_expression = IRenvironment.killCodeToExpression(item_code);
	}
	if (last_expression == NULL)
		translated = new IR::StatementCode(sequence);
	else
		if (sequence->statements.size() > 0)
			translated = new IR::ExpressionCode(new IR::StatExpSequence(
				sequence, last_expression));
		else
			translated = new IR::ExpressionCode(last_expression);
}

void TranslatorPrivate::translateArrayIndexing(
	Syntax::ArrayIndexing *expression,
	IR::Code *&translated, Type *&type, IR::Label *last_loop_exit,
	IR::AbstractFrame *currentFrame)
{
	IR::Code *array, *index;
	Type *arrayType, *indexType;
	translateExpression(expression->array, array, arrayType, last_loop_exit, currentFrame);
	if (! IsArray(arrayType))
		Error::error("Not an array got indexed", expression->array->linenumber);
	translateExpression(expression->index, index, indexType, last_loop_exit, currentFrame);
	if (! IsInt(indexType))
		Error::error("Index is not an integer", expression->index->linenumber);
	if (arrayType->basetype == TYPE_ARRAY) {
		type = ((ArrayType *)arrayType)->elemtype->resolve();
		IR::BinaryOpExpression *offset = new IR::BinaryOpExpression(
			IR::OP_MUL, IRenvironment.killCodeToExpression(index),
			new IR::IntegerExpression(framemanager->getVarSize(type)));
		IR::BinaryOpExpression *target_address = new IR::BinaryOpExpression(
			IR::OP_PLUS, IRenvironment.killCodeToExpression(array), offset);
		translated = new IR::ExpressionCode(new IR::MemoryExpression(target_address));
	} else {
		type = type_environment->getErrorType();
		translated = ErrorPlaceholderCode();
	}
}

void TranslatorPrivate::translateArrayInstantiation(
	Syntax::ArrayInstantiation *expression, IR::Code *&translated,
	Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame)
{
	assert(expression->arrayIsSingleId());
	type = type_environment->getType(expression->arraydef->array, false)->resolve();
	Type *elem_type = NULL;
	if (type == NULL) {
		Error::error("Undefined type identifier in array instantiation",
			expression->arraydef->array->linenumber);
		type = type_environment->getErrorType();
	} else {
		if (type->basetype != TYPE_ARRAY) {
			if (type->basetype != TYPE_ERROR)
				Error::error("Not an array type in array instantiation",
					expression->arraydef->array->linenumber);
			type = type_environment->getErrorType();
		} else {
			elem_type = ((ArrayType *)type)->elemtype->resolve();
		}
	}
	IR::Code *length;
	Type *lengthType;
	translateExpression(expression->arraydef->index, length, lengthType, last_loop_exit,
		currentFrame);
	if (! IsInt(lengthType))
		Error::error("Not an integer as array length", expression->arraydef->index->linenumber);

	IR::Code *value;
	Type *valueType;
	translateExpression(expression->value, value, valueType, last_loop_exit, currentFrame);
	if ((elem_type != NULL) && ! CheckAssignmentTypes(elem_type, valueType))
		Error::error("Value type doesn't match array element type",
			expression->value->linenumber);
	
	if (elem_type != NULL) {
		std::list<IR::Code *> args;
		args.push_back(length);
		args.push_back(value);
		makeCallCode(getmem_fill_func, args, translated);
	} else
		translated = ErrorPlaceholderCode();
}

void TranslatorPrivate::translateIf(
	Syntax::If *expression, IR::Code *&translated,
	Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame)
{
	type = type_environment->getVoidType();
	Type *conditionType, *actionType;
	IR::Code *condition, *action;
	translateExpression(expression->condition, condition, conditionType, last_loop_exit,
		currentFrame);
	if (! IsInt(conditionType))
		Error::error("If condition not integer", expression->condition->linenumber);
	translateExpression(expression->action, action, actionType, last_loop_exit,
		currentFrame);
	
	if (conditionType->basetype == TYPE_INT) {
		std::list<IR::Label**> replace_true, replace_false;
		IR::Statement *jump_by_condition = IRenvironment.killCodeToCondJump(condition,
			replace_true, replace_false);
		IR::Label *true_label = IRenvironment.addLabel();
		IR::Label *false_label = IRenvironment.addLabel();
		IR::putLabels(replace_true, replace_false, true_label, false_label);
		IR::StatementSequence *statement = new IR::StatementSequence;
		statement->addStatement(jump_by_condition);
		statement->addStatement(new IR::LabelPlacementStatement(true_label));
		statement->addStatement(IRenvironment.killCodeToStatement(action));
		statement->addStatement(new IR::LabelPlacementStatement(false_label));
		translated = new IR::StatementCode(statement);
	} else
		translated = ErrorPlaceholderCode();
}

void TranslatorPrivate::translateIfElse(
	Syntax::IfElse *expression, IR::Code *&translated,
	Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame)
{
	Type *conditionType, *actionType, *elseType;
	IR::Code *condition_code, *action_code, *elseaction_code;
	translateExpression(expression->condition, condition_code, conditionType, last_loop_exit,
		currentFrame);
	if (! IsInt(conditionType))
		Error::error("If condition not integer", expression->condition->linenumber);
	translateExpression(expression->action, action_code, actionType, last_loop_exit,
		currentFrame);
	translateExpression(expression->elseaction, elseaction_code, elseType, last_loop_exit,
		currentFrame);
	if ((actionType->basetype == TYPE_VOID) && (elseType->basetype == TYPE_VOID) ||
			CheckComparisonTypes(actionType, elseType)) {
		if (actionType->basetype == TYPE_NIL)
			type = elseType;
		else
			type = actionType;
		if (conditionType->basetype == TYPE_INT) {
			IR::Label *true_label = IRenvironment.addLabel();
			IR::Label *false_label = IRenvironment.addLabel();
			IR::Label *finish_label = IRenvironment.addLabel();
			IR::Register *value_storage = NULL;
			if (type->basetype != TYPE_VOID)
				value_storage = IRenvironment.addRegister();
			std::list<IR::Label**> replace_true, replace_false;
			IR::Statement *condition_jump = IRenvironment.killCodeToCondJump(
				condition_code, replace_true, replace_false);
			IR::putLabels(replace_true, replace_false, true_label, false_label);
			IR::StatementSequence *calculation = new IR::StatementSequence;
			calculation->addStatement(condition_jump);
			calculation->addStatement(new IR::LabelPlacementStatement(true_label));
			if (type->basetype != TYPE_VOID) {
				calculation->addStatement(new IR::MoveStatement(
					new IR::RegisterExpression(value_storage),
					IRenvironment.killCodeToExpression(action_code)));
			} else {
				calculation->addStatement(IRenvironment.killCodeToStatement(action_code));
			}
			calculation->addStatement(new IR::JumpStatement(
				new IR::LabelAddressExpression(finish_label), finish_label));
			calculation->addStatement(new IR::LabelPlacementStatement(false_label));
			if (type->basetype != TYPE_VOID) {
				calculation->addStatement(new IR::MoveStatement(
					new IR::RegisterExpression(value_storage),
					IRenvironment.killCodeToExpression(elseaction_code)));
			} else {
				calculation->addStatement(IRenvironment.killCodeToStatement(elseaction_code));
			}
			calculation->addStatement(new IR::LabelPlacementStatement(finish_label));
			if (type->basetype == TYPE_VOID) {
				translated = new IR::StatementCode(calculation);
			} else {
				translated = new IR::ExpressionCode(new IR::StatExpSequence(
					calculation, new IR::RegisterExpression(value_storage)));
			}
		} else
			translated = ErrorPlaceholderCode();
	} else {
		Error::error("Incompatible or improper types for then and else", expression->linenumber);
		type = type_environment->getErrorType();
		translated = ErrorPlaceholderCode();
	}
}

void TranslatorPrivate::translateWhile(
	Syntax::While *expression, IR::Code *&translated,
	Type *&type, IR::AbstractFrame *currentFrame)
{
	type = type_environment->getVoidType();
	Type *conditionType, *actionType;
	IR::Code *condition, *action;
	IR::Label *exit_label = IRenvironment.addLabel();
	translateExpression(expression->condition, condition, conditionType, exit_label,
		currentFrame);
	if (! IsInt(conditionType))
		Error::error("While condition not integer", expression->condition->linenumber);
	translateExpression(expression->action, action, actionType, exit_label,
		currentFrame);
	if (conditionType->basetype == TYPE_INT) {
		IR::Label *loop_label = IRenvironment.addLabel();
		IR::Label *proceed_label = IRenvironment.addLabel();
		std::list<IR::Label **> replace_true, replace_false;
		IR::Statement *cond_jump = IRenvironment.killCodeToCondJump(condition,
			replace_true, replace_false);
		IR::putLabels(replace_true, replace_false, proceed_label, exit_label);
		IR::StatementSequence *sequence = new IR::StatementSequence;
		sequence->addStatement(new IR::LabelPlacementStatement(loop_label));
		sequence->addStatement(cond_jump);
		sequence->addStatement(new IR::LabelPlacementStatement(proceed_label));
		sequence->addStatement(IRenvironment.killCodeToStatement(action));
		sequence->addStatement(new IR::JumpStatement(
			new IR::LabelAddressExpression(loop_label), loop_label));
		sequence->addStatement(new IR::LabelPlacementStatement(exit_label));
	} else
		translated = ErrorPlaceholderCode();
}

void TranslatorPrivate::translateFor(
	Syntax::For *expression, IR::Code *&translated,
	Type *&type, IR::AbstractFrame *currentFrame)
{
	type = type_environment->getVoidType();
	Type *from_type, *to_type, *actionType;
	IR::Code *from_code, *to_code, *action_code;
	IR::Label *exit_label = IRenvironment.addLabel();
	translateExpression(expression->start, from_code, from_type, exit_label,
		currentFrame);
	if (! IsInt(from_type))
		Error::error("For loop start not integer", expression->start->linenumber);
	translateExpression(expression->stop, to_code, to_type, exit_label,
		currentFrame);
	if (! IsInt(to_type))
		Error::error("For loop 'to' not integer", expression->stop->linenumber);
	func_and_var_names.newLayer();
	variables.push_back(Variable(
	//LoopVariable *loopvar = new LoopVariable(
		expression->variable->name,
		type_environment->getLoopIntType(), NULL, currentFrame->addVariable(
			framemanager->getVarSize(from_type),
			variables_extra_info.isAccessedByAddress(expression)
		)));
	Variable *loopvar = &(variables.back());
	func_and_var_names.add(expression->variable->name, loopvar);
	translateExpression(expression->action, action_code, actionType, exit_label,
		currentFrame);
	if ((from_type->basetype == TYPE_INT) && (to_type->basetype == TYPE_INT)) {
		IR::StatementSequence *sequence = new IR::StatementSequence;
		IR::Register *upper_bound = IRenvironment.addRegister();
		sequence->addStatement(new IR::MoveStatement(
			new IR::RegisterExpression(upper_bound),
			IRenvironment.killCodeToExpression(to_code)));
		IR::Code *loopvar_code = loopvar->implementation->createCode(currentFrame);
		sequence->addStatement(new IR::MoveStatement(
			IRenvironment.killCodeToExpression(loopvar_code),
			IRenvironment.killCodeToExpression(from_code)));
		IR::Label *loop_label = IRenvironment.addLabel();
		loopvar_code = loopvar->implementation->createCode(currentFrame);
		sequence->addStatement(new IR::CondJumpStatement(IR::OP_LESSEQUAL,
			IRenvironment.killCodeToExpression(loopvar_code),
			new IR::RegisterExpression(upper_bound),
			loop_label, exit_label));
		sequence->addStatement(new IR::LabelPlacementStatement(loop_label));
		sequence->addStatement(IRenvironment.killCodeToStatement(action_code));
		IR::Label *proceed_label = IRenvironment.addLabel();
		loopvar_code = loopvar->implementation->createCode(currentFrame);
		sequence->addStatement(new IR::CondJumpStatement(IR::OP_LESS,
			IRenvironment.killCodeToExpression(loopvar_code),
			new IR::RegisterExpression(upper_bound),
			proceed_label, exit_label));
		sequence->addStatement(new IR::LabelPlacementStatement(proceed_label));
		loopvar_code = loopvar->implementation->createCode(currentFrame);
		IR::Code *loopvar_code2 = loopvar->implementation->createCode(currentFrame);
		sequence->addStatement(new IR::MoveStatement(
			IRenvironment.killCodeToExpression(loopvar_code),
			new IR::BinaryOpExpression(IR::OP_PLUS,
				IRenvironment.killCodeToExpression(loopvar_code2),
				new IR::IntegerExpression(1)
			)
		));
		sequence->addStatement(new IR::JumpStatement(
			new IR::LabelAddressExpression(loop_label), loop_label));
		sequence->addStatement(new IR::LabelPlacementStatement(exit_label));
		translated = new IR::StatementCode(sequence);
	} else
		translated = ErrorPlaceholderCode();
	func_and_var_names.removeLastLayer();
}

void TranslatorPrivate::translateScope(
	Syntax::Scope *expression, IR::Code *&translated,
	Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame)
{
	newLayer();
	std::list<Variable *> new_vars;
	processDeclarations(expression->declarations, currentFrame, new_vars);
	IR::StatementSequence *sequence = new IR::StatementSequence;
	for (std::list<Variable *>::iterator newvar = new_vars.begin();
			newvar != new_vars.end(); newvar++) {
		IR::Code *var_code = (*newvar)->implementation->createCode(currentFrame);
		sequence->addStatement(new IR::MoveStatement(
			IRenvironment.killCodeToExpression(var_code),
			IRenvironment.killCodeToExpression((*newvar)->value)
		));
	}
	translateSequence(expression->action->expressions, translated, type,
		NULL, currentFrame, sequence);
	removeLastLayer();
}

void TranslatorPrivate::translateRecordField(
	Syntax::RecordField *expression, IR::Code *&translated,
	Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame)
{
	Type *recType;
	IR::Code *record_code;
	translateExpression(expression->record, record_code, recType, last_loop_exit,
		currentFrame);
	if (recType->basetype != TYPE_RECORD) {
		if (recType->basetype != TYPE_ERROR)
			Error::error("What is being got field of is not a record",
				expression->record->linenumber);
		type = type_environment->getErrorType();
		translated = ErrorPlaceholderCode();
	} else {
		RecordType *record = (RecordType *)recType;
		RecordType::FieldsMap::iterator field = record->fields.find(expression->field->name);
		if (field == record->fields.end()) {
			Error::error("This field is not in that record",
				expression->field->linenumber);
			type = type_environment->getErrorType();
			translated = ErrorPlaceholderCode();
		} else {
			type = (*field).second->type->resolve();
			IR::BinaryOpExpression *target_address = new IR::BinaryOpExpression(
				IR::OP_PLUS, IRenvironment.killCodeToExpression(record_code),
				new IR::IntegerExpression((*field).second->offset));
			translated = new IR::ExpressionCode(new IR::MemoryExpression(target_address));
		}
	}
}

void TranslatorPrivate::makeCallCode(Function *function, std::list<IR::Code *>arguments,
		IR::Code *&result)
{
	IR::CallExpression *call = new IR::CallExpression(
		new IR::LabelAddressExpression(function->label));
	for (std::list<IR::Code *>::iterator arg = arguments.begin();
			arg != arguments.end(); arg++)
		 call->addArgument(IRenvironment.killCodeToExpression(*arg));
	result = new IR::ExpressionCode(call);
}

void TranslatorPrivate::translateFunctionCall(
	Syntax::FunctionCall *expression, IR::Code *&translated,
	Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame)
{
	Declaration *var_or_function = (Declaration *)
		func_and_var_names.lookup(expression->function->name);
	if (var_or_function == NULL) {
		Error::error(std::string("Undefined variable or function identifier ") +
			expression->function->name,
			expression->function->linenumber);
		type = type_environment->getErrorType();
		translated = ErrorPlaceholderCode();
		return;
	}
	if (var_or_function->kind != DECL_FUNCTION) {
		Error::error("Variable used as a function",
			expression->function->linenumber);
		type = type_environment->getErrorType();
		translated = ErrorPlaceholderCode();
		return;
	}
	Function *function = (Function *)var_or_function;
	type = function->return_type->resolve();
	std::list<FunctionArgument>::iterator function_arg = function->arguments.begin();
	bool already_too_many;
	std::list<IR::Code *> arguments_code;
	for (std::list<Syntax::Tree>::iterator passed_arg = expression->arguments->expressions.begin();
		 passed_arg != expression->arguments->expressions.end(); passed_arg++) {
		Type *argument_type = type_environment->getErrorType();
		Type *passed_type;
		IR::Code *argument_code;
		translateExpression(*passed_arg, argument_code, passed_type, NULL,
			currentFrame);
		arguments_code.push_back(argument_code);
		if (! already_too_many && (function_arg == function->arguments.end())) {
			already_too_many = true;
			Error::error("Too many arguments to the function",
				(*passed_arg)->linenumber);
		}
		
		if (! already_too_many) {
			argument_type = (*function_arg).type->resolve();
			if (! CheckAssignmentTypes(argument_type, passed_type)) {
				Error::error("Incompatible type passed as an argument",
					(*passed_arg)->linenumber);
			}
			function_arg++;
		}
	}
	if (function_arg != function->arguments.end())
		Error::error("Not enough arguments to the function",
			expression->linenumber);
	makeCallCode(function, arguments_code, translated);
}

void TranslatorPrivate::translateRecordInstantiation(
	Syntax::RecordInstantiation *expression, IR::Code *&translated,
	Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame)
{
	type = type_environment->getType(expression->type, false)->resolve();
	if (type == NULL) {
		Error::error("Unknown type identifier", expression->type->linenumber);
		type = type_environment->getErrorType();
		return;
	}
	if (type->basetype != TYPE_RECORD) {
		if (type->basetype != TYPE_ERROR)
			Error::error("Not a record type being record-instantiated",
				expression->type->linenumber);
		type = type_environment->getErrorType();
		return;
	}
	RecordType *record = (RecordType *)type;
	
	IR::Register *record_address = IRenvironment.addRegister();
	IR::StatementSequence *sequence = new IR::StatementSequence;
	std::list<IR::Code *>alloc_argument;
	alloc_argument.push_back(new IR::ExpressionCode(new IR::IntegerExpression(
		record->data_size)));
	IR::Code *alloc_code;
	makeCallCode(getmem_func, alloc_argument, alloc_code);
	sequence->addStatement(new IR::MoveStatement(
		new IR::RegisterExpression(record_address),
		IRenvironment.killCodeToExpression(alloc_code)));
		
	RecordType::FieldsList::iterator record_field = record->field_list.begin();
	for (std::list<Syntax::Tree>::iterator set_field = expression->fieldvalues->expressions.begin();
			set_field != expression->fieldvalues->expressions.end(); set_field++) {
		if (record_field == record->field_list.end()) {
			Error::error("More field initialized than there are in the record",
				(*set_field)->linenumber);
			break;
		}
		Type *field_type = (*record_field).type->resolve();
		assert((*set_field)->type == Syntax::BINARYOP);
		Syntax::BinaryOp *fieldvalue = (Syntax::BinaryOp *) *set_field;
		assert(fieldvalue->operation == SYM_ASSIGN);
		assert(fieldvalue->left->type == Syntax::IDENTIFIER);
		Syntax::Identifier * field = (Syntax::Identifier *)fieldvalue->left;
		if (field->name != (*record_field).name) {
			Error::error(std::string("Wrong field name ") + field->name +
				" expected " + (*record_field).name,
				fieldvalue->left->linenumber);
		} else {
			Type *valueType;
			IR::Code *value_code;
			translateExpression(fieldvalue->right, value_code, valueType, last_loop_exit,
				currentFrame);
			if (! CheckAssignmentTypes(field_type, valueType)) {
				Error::error("Incompatible type for this field value",
					fieldvalue->right->linenumber);
			} else {
				sequence->addStatement(new IR::MoveStatement(
					new IR::BinaryOpExpression(IR::OP_PLUS,
						new IR::RegisterExpression(record_address),
						new IR::IntegerExpression((*record_field).offset)
					), IRenvironment.killCodeToExpression(value_code)
				));
			}
		}
		record_field++;
	}
	translated = new IR::StatementCode(sequence);
}

const char *NODETYPENAMES[] = {
	"INTVALUE",
	"STRINGVALUE",
	"IDENTIFIER",
	"NIL",
	"BINARYOP",
	"EXPRESSIONLIST",
	"SEQUENCE",
	"ARRAYINDEXING",
	"ARRAYINSTANTIATION",
	"IF",
	"IFELSE",
	"WHILE",
	"FOR",
	"BREAK",
	"SCOPE",
	"RECORDFIELD",
	"FUNCTIONCALL",
	"RECORDINSTANTIATION",
	"TYPEDECLARATION",
	"ARRAYTYPEDEFINITION",
	"RECORDTYPEDEFINITION",
	"PARAMETERDECLARATION",
	"VARDECLARATION",
	"FUNCTION",
};

void TranslatorPrivate::translateExpression(Syntax::Tree expression,
	IR::Code *&translated, Type*& type,
	IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame)
{
	translated = NULL;
	switch (expression->type) {
		case Syntax::INTVALUE:
			translateIntValue((Syntax::IntValue *)expression, translated);
			type = type_environment->getIntType();
			break;
		case Syntax::STRINGVALUE:
			translateStringValue((Syntax::StringValue *)expression, translated);
			type = type_environment->getStringType();
			break;
		case Syntax::IDENTIFIER:
			translateIdentifier((Syntax::Identifier *)expression, translated, type,
				currentFrame);
			break;
		case Syntax::NIL:
			type = type_environment->getNilType();
			break;
		case Syntax::BINARYOP:
			translateBinaryOperation((Syntax::BinaryOp *)expression, translated,
				type, last_loop_exit, currentFrame);
			break;
		case Syntax::SEQUENCE:
			translateSequence(((Syntax::Sequence *)expression)->content->expressions, translated, type,
				last_loop_exit, currentFrame);
			break;
		case Syntax::ARRAYINDEXING:
			translateArrayIndexing((Syntax::ArrayIndexing *)expression, translated,
				type, last_loop_exit, currentFrame);
			break;
		case Syntax::ARRAYINSTANTIATION:
			translateArrayInstantiation((Syntax::ArrayInstantiation *)expression,
				translated, type, last_loop_exit, currentFrame);
			break;
		case Syntax::IF:
			translateIf((Syntax::If *)expression, translated, type, last_loop_exit,
				currentFrame);
			break;
		case Syntax::IFELSE:
			translateIfElse((Syntax::IfElse *)expression, translated, type, last_loop_exit,
				currentFrame);
			break;
		case Syntax::WHILE:
			translateWhile((Syntax::While *)expression, translated, type, currentFrame);
			break;
		case Syntax::FOR:
			translateFor((Syntax::For *)expression, translated, type, currentFrame);
			break;
		case Syntax::BREAK:
			if (last_loop_exit == NULL)
				Error::error("Break outside of a loop", expression->linenumber);
			type = type_environment->getVoidType();
			break;
		case Syntax::SCOPE:
			translateScope((Syntax::Scope *)expression, translated, type, last_loop_exit,
				currentFrame);
			break;
		case Syntax::RECORDFIELD:
			translateRecordField((Syntax::RecordField *)expression, translated, type,
				last_loop_exit, currentFrame);
			break;
		case Syntax::FUNCTIONCALL:
			translateFunctionCall((Syntax::FunctionCall *)expression, translated, type,
				last_loop_exit, currentFrame);
			break;
		case Syntax::RECORDINSTANTIATION:
			translateRecordInstantiation((Syntax::RecordInstantiation *)expression,
				translated, type, last_loop_exit, currentFrame);
			break;
		default:
			Error::fatalError(std::string("Unexpected ") + NODETYPENAMES[expression->type]);
	}
}

Translator::Translator(IR::AbstractFrameManager * _framemanager)
{
	impl = new TranslatorPrivate(_framemanager);
}

Translator::~Translator()
{
	delete impl;
}

void Translator::translateProgram(Syntax::Tree expression,
	IR::Code*& translated, Type*& type)
{
	impl->variables_extra_info.processExpression(expression);
	impl->translateExpression(expression, translated, type, NULL,
		impl->framemanager->newFrame(impl->framemanager->rootFrame()));
}


}
