#include "translator.h"
#include "errormsg.h"
#include <list>
#include <set>

namespace Semantic {

class TypesEnvironment {
private:
	std::list<Type*> types;
	LayeredMap typenames;
	std::list<ForwardReferenceType *> unknown_types;
	Type *int_type, *looped_int, *string_type, *error_type, *void_type, *nil_type;
	IdTracker id_provider;
	
	Type *createArrayType(Syntax::ArrayTypeDefinition *definition, bool allow_forward_references);
	Type *createRecordType(Syntax::RecordTypeDefinition *definition, bool allow_forward_references);
public:
	TypesEnvironment();
	~TypesEnvironment();
	
	Type *getIntType() {return int_type;}
	Type *getLoopIntType() {return looped_int;}
	Type *getStringType() {return string_type;}
	Type *getErrorType() {return error_type;}
	Type *getVoidType() {return void_type;}
	Type *getNilType() {return nil_type;}
	Type *getType(Syntax::Tree definition, bool allow_forward_references);
	
	void newLayer() {typenames.newLayer();}
	void removeLastLayer() {typenames.removeLastLayer();}
	void processTypeDeclarationBatch(std::list<Syntax::Tree>::iterator begin,
		std::list<Syntax::Tree>::iterator end);
};

class DeclarationsEnvironmentPrivate {
private:
	LayeredMap func_and_var_names;
	std::list<Variable> variables;
	std::list<Function> functions;
	TypesEnvironment *type_environment;
	Variable *undefined_variable;
	
	void newLayer();
	void removeLastLayer();
	void processDeclarations(Syntax::ExpressionList *declarations);
	Declaration *findVariableOrFunction(Syntax::Identifier *id);

	void processVariableDeclaration(Syntax::VariableDeclaration *declaration);
	void processFunctionDeclarationBatch(std::list<Syntax::Tree>::iterator begin,
		std::list<Syntax::Tree>::iterator end);
	
	void translateIntValue(Syntax::IntValue *expression, TranslatedExpression *&translated);
	void translateStringValue(Syntax::StringValue *expression, TranslatedExpression *&translated);
	void translateIdentifier(Syntax::Identifier *expression, TranslatedExpression *&translated,
		Type *&type);
	void translateBinaryOperation(Syntax::BinaryOp *expression,
		TranslatedExpression *&translated, Type *&type, Syntax::Tree lastloop);
	void translateArrayIndexing(Syntax::ArrayIndexing *expression,
		TranslatedExpression *&translated, Type *&type, Syntax::Tree lastloop);
	void translateArrayInstantiation(Syntax::ArrayInstantiation *expression,
		TranslatedExpression *&translated, Type *&type, Syntax::Tree lastloop);
	void translateIf(Syntax::If *expression, TranslatedExpression *&translated,
		Type *&type, Syntax::Tree lastloop);
	void translateIfElse(Syntax::IfElse *expression, TranslatedExpression *&translated,
		Type *&type, Syntax::Tree lastloop);
	void translateWhile(Syntax::While *expression, TranslatedExpression *&translated,
		Type *&type);
	void translateFor(Syntax::For *expression, TranslatedExpression *&translated,
		Type *&type);
	void translateScope(Syntax::Scope *expression, TranslatedExpression *&translated,
		Type *&type, Syntax::Tree lastloop);
	void translateRecordField(Syntax::RecordField *expression, TranslatedExpression *&translated,
		Type *&type, Syntax::Tree lastloop);
	void translateFunctionCall(Syntax::FunctionCall *expression, TranslatedExpression *&translated,
		Type *&type, Syntax::Tree lastloop);
	void translateRecordInstantiation(Syntax::RecordInstantiation *expression, TranslatedExpression *&translated,
		Type *&type, Syntax::Tree lastloop);
public:
	DeclarationsEnvironmentPrivate();
	~DeclarationsEnvironmentPrivate();
	
	void translateExpression(Syntax::Tree expression,
		TranslatedExpression *&translated, Type *&type,
		Syntax::Tree lastloop = NULL);
};

TypesEnvironment::TypesEnvironment()
{
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
	for (std::list<Syntax::Tree>::iterator i = begin; i != end; i++) {
		assert((*i)->type == Syntax::TYPEDECLARATION);
		Syntax::TypeDeclaration *declaration = (Syntax::TypeDeclaration *)*i;
		Type *type = getType(declaration->definition, true);
		assert(type != NULL);
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
	
	unknown_types.clear();
}

DeclarationsEnvironmentPrivate::DeclarationsEnvironmentPrivate()
{
	type_environment = new TypesEnvironment;
	
	undefined_variable = new Variable("undefined", type_environment->getErrorType(),
		new TranslatedExpression);
	functions.push_back(Function("print", type_environment->getVoidType(),
		NULL, new TranslatedExpression));
	functions.back().addArgument("s", type_environment->getStringType());
	func_and_var_names.add("print", &(functions.back()));

	functions.push_back(Function("flush", type_environment->getVoidType(),
		NULL, new TranslatedExpression));
	func_and_var_names.add("flush", &(functions.back()));

	functions.push_back(Function("getchar", type_environment->getStringType(),
		NULL, new TranslatedExpression));
	func_and_var_names.add("getchar", &(functions.back()));

	functions.push_back(Function("ord", type_environment->getIntType(),
		NULL, new TranslatedExpression));
	functions.back().addArgument("s", type_environment->getStringType());
	func_and_var_names.add("ord", &(functions.back()));

	functions.push_back(Function("chr", type_environment->getStringType(),
		NULL, new TranslatedExpression));
	functions.back().addArgument("i", type_environment->getIntType());
	func_and_var_names.add("chr", &(functions.back()));

	functions.push_back(Function("size", type_environment->getIntType(),
		NULL, new TranslatedExpression));
	functions.back().addArgument("s", type_environment->getStringType());
	func_and_var_names.add("size", &(functions.back()));

	functions.push_back(Function("substring", type_environment->getStringType(),
		NULL, new TranslatedExpression));
	functions.back().addArgument("s", type_environment->getStringType());
	functions.back().addArgument("first", type_environment->getIntType());
	functions.back().addArgument("n", type_environment->getIntType());
	func_and_var_names.add("substring", &(functions.back()));

	functions.push_back(Function("concat", type_environment->getStringType(),
		NULL, new TranslatedExpression));
	functions.back().addArgument("s1", type_environment->getStringType());
	functions.back().addArgument("s2", type_environment->getStringType());
	func_and_var_names.add("concat", &(functions.back()));

	functions.push_back(Function("not", type_environment->getIntType(),
		NULL, new TranslatedExpression));
	functions.back().addArgument("i", type_environment->getIntType());
	func_and_var_names.add("not", &(functions.back()));

	functions.push_back(Function("exit", type_environment->getVoidType(),
		NULL, new TranslatedExpression));
	functions.back().addArgument("i", type_environment->getIntType());
	func_and_var_names.add("exit", &(functions.back()));
}

DeclarationsEnvironmentPrivate::~DeclarationsEnvironmentPrivate()
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

void DeclarationsEnvironmentPrivate::newLayer()
{
	type_environment->newLayer();
	func_and_var_names.newLayer();
}

void DeclarationsEnvironmentPrivate::removeLastLayer()
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

void DeclarationsEnvironmentPrivate::processVariableDeclaration(
	Syntax::VariableDeclaration *declaration)
{
	Type *vartype;
	if (declaration->type != NULL)
		vartype = type_environment->getType(declaration->type, false)->resolve();
	else
		vartype = NULL;
	Type *exprtype;
	TranslatedExpression *translatedValue;
	translateExpression(declaration->value, translatedValue, exprtype, NULL);
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
	variables.push_back(Variable(declaration->name->name, vartype, translatedValue));
	func_and_var_names.add(declaration->name->name, &(variables.back()));
}

void DeclarationsEnvironmentPrivate::processFunctionDeclarationBatch(
	std::list<Syntax::Tree>::iterator begin,
	std::list<Syntax::Tree>::iterator end)
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
			declaration->body, NULL));
		Function *function = &(functions.back());
		func_and_var_names.add(declaration->name->name, function);
		recent_functions.push_back(function);
		
		for (std::list<Syntax::Tree>::iterator param =
				declaration->parameters->expressions.begin();
				param != declaration->parameters->expressions.end(); param++) {
			assert((*param)->type == Syntax::PARAMETERDECLARATION);
			Syntax::ParameterDeclaration *param_decl = 
				(Syntax::ParameterDeclaration *) *param;
			Type *param_type = type_environment->getType(param_decl->type, false)->resolve();
			function->addArgument(param_decl->name->name, param_type);
		}
	}
	for (std::list<Function *>::iterator fcn = recent_functions.begin();
		 fcn != recent_functions.end(); fcn++) {
		Type *actual_return_type;
		func_and_var_names.newLayer();
		for (std::list<FunctionArgument>::iterator param = (*fcn)->arguments.begin();
				param != (*fcn)->arguments.end(); param++)
			func_and_var_names.add((*param).name, &(*param));
		translateExpression((*fcn)->raw_body, ((*fcn)->body), actual_return_type, NULL);
		func_and_var_names.removeLastLayer();
		if (((*fcn)->return_type->basetype != TYPE_VOID) &&
			! CheckAssignmentTypes((*fcn)->return_type, actual_return_type))
			Error::error("Type of function body doesn't match specified return type",
				(*fcn)->raw_body->linenumber);
	}
}

void DeclarationsEnvironmentPrivate::processDeclarations(Syntax::ExpressionList *declarations)
{
	std::list<Syntax::Tree>::iterator d = declarations->expressions.begin();
	std::list<Syntax::Tree>::iterator end = declarations->expressions.end();
	while (d != end) {
		Syntax::Tree declaration = *d;
		Syntax::NodeType dectype = declaration->type;
		if (dectype == Syntax::VARDECLARATION) {
			processVariableDeclaration((Syntax::VariableDeclaration *)declaration);
			d++;
		} else if ((dectype == Syntax::FUNCTION) || 
			       (dectype == Syntax::TYPEDECLARATION)) {
			std::list<Syntax::Tree>::iterator batch_begin = d;
			while ((d != end) && ((*d)->type == dectype))
				d++;
			if (dectype == Syntax::FUNCTION) {
				processFunctionDeclarationBatch(batch_begin, d);
			} else {
				type_environment->processTypeDeclarationBatch(batch_begin, d);
			}
		} else
			Error::fatalError("Not a declaration inside declaration block", declaration->linenumber);
	}
}

Declaration *DeclarationsEnvironmentPrivate::findVariableOrFunction(Syntax::Identifier *id)
{
	Declaration *result = (Declaration *)func_and_var_names.lookup(id->name);
	if (result == NULL) {
		Error::error("Undefined variable or function " + id->name, id->linenumber);
		result = undefined_variable;
	}
	return result;
}

void DeclarationsEnvironmentPrivate::translateIntValue(Syntax::IntValue *expression,
	TranslatedExpression *&translated)
{
}

void DeclarationsEnvironmentPrivate::translateStringValue(Syntax::StringValue *expression,
	TranslatedExpression *&translated)
{
}

void DeclarationsEnvironmentPrivate::translateIdentifier(Syntax::Identifier *expression,
	TranslatedExpression *&translated, Type *&type)
{
	Declaration *var_or_function = (Declaration *)func_and_var_names.lookup(
		expression->name);
	if (var_or_function == NULL) {
		Error::error("Undeclared variable/function identifier", expression->linenumber);
		type = type_environment->getErrorType();
		return;
	}
	if (var_or_function->kind == DECL_FUNCTION) {
		Error::error("Function used as a variable", expression->linenumber);
		type = type_environment->getErrorType();
	} else {
		type = ((Variable *)var_or_function)->type->resolve();
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

void DeclarationsEnvironmentPrivate::translateBinaryOperation(Syntax::BinaryOp *expression,
	TranslatedExpression *&translated, Type *&type, Syntax::Tree lastloop)
{
	TranslatedExpression *left, *right;
	Type *leftType, *rightType;
	translateExpression(expression->left, left, leftType, lastloop);
	translateExpression(expression->right, right, rightType, lastloop);
	if (leftType->basetype == TYPE_VOID)
		Error::error("Statement without value used in expression",
					 expression->left->linenumber);
	if (rightType->basetype == TYPE_VOID)
		Error::error("Statement without value used in expression",
					 expression->right->linenumber);
	if ((leftType->basetype == TYPE_ERROR) || (rightType->basetype == TYPE_ERROR)) {
		type = type_environment->getErrorType();
		return;
	}
	switch (expression->operation) {
		case SYM_PLUS:
		case SYM_MINUS:
		case SYM_ASTERISK:
		case SYM_SLASH:
		case SYM_AND:
		case SYM_OR:
			type = type_environment->getIntType();
			if (! IsInt(leftType))
				Error::error("Operand for arithmetic is not integer",
							 expression->left->linenumber);
			if (! IsInt(rightType))
				Error::error("Operand for arithmetic is not integer",
							 expression->right->linenumber);
			break;
		case SYM_EQUAL:
		case SYM_NONEQUAL:
			type = type_environment->getIntType();
			if (! CheckComparisonTypes(leftType, rightType))
				Error::error("Incompatible types for equality comparison",
							 expression->linenumber);
			break;
		case SYM_LESS:
		case SYM_LESSEQUAL:
		case SYM_GREATER:
		case SYM_GREATEQUAL:
			type = type_environment->getIntType();
			if ((leftType->basetype != TYPE_INT) && (leftType->basetype != TYPE_STRING))
				Error::error("Operand for arithmetic is not integer",
							 expression->left->linenumber);
			if ((rightType->basetype != TYPE_INT) && (rightType->basetype != TYPE_STRING))
				Error::error("Operand for arithmetic is not integer",
							 expression->right->linenumber);
			if (leftType->basetype != rightType->basetype)
				Error::error("Incompatible types for comparison",
							 expression->right->linenumber);
			break;
		case SYM_ASSIGN:
			type = type_environment->getVoidType();
			if (! CheckAssignmentTypes(leftType, rightType))
				Error::error("Incompatible types for assignment",
							 expression->linenumber);
			if (leftType->is_loop_variable)
				Error::error("Assignment to for loop variable",
							 expression->left->linenumber);
			break;
	}
}

void DeclarationsEnvironmentPrivate::translateArrayIndexing(
	Syntax::ArrayIndexing *expression,
	TranslatedExpression *&translated, Type *&type, Syntax::Tree lastloop)
{
	TranslatedExpression *array, *index;
	Type *arrayType, *indexType;
	translateExpression(expression->array, array, arrayType, lastloop);
	if (! IsArray(arrayType))
		Error::error("Not an array got indexed", expression->array->linenumber);
	translateExpression(expression->index, index, indexType, lastloop);
	if (! IsInt(indexType))
		Error::error("Index is not an integer", expression->index->linenumber);
	if (arrayType->basetype == TYPE_ARRAY)
		type = ((ArrayType *)arrayType)->elemtype->resolve();
	else
		type = type_environment->getErrorType();
}

void DeclarationsEnvironmentPrivate::translateArrayInstantiation(
	Syntax::ArrayInstantiation *expression, TranslatedExpression *&translated,
	Type *&type, Syntax::Tree lastloop)
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
	TranslatedExpression *length;
	Type *lengthType;
	translateExpression(expression->arraydef->index, length, lengthType, lastloop);
	if (! IsInt(lengthType))
		Error::error("Not an integer as array length", expression->arraydef->index->linenumber);

	TranslatedExpression *value;
	Type *valueType;
	translateExpression(expression->value, value, valueType, lastloop);
	if ((elem_type != NULL) && ! CheckAssignmentTypes(elem_type, valueType))
		Error::error("Value type doesn't match array element type",
			expression->value->linenumber);
}

void DeclarationsEnvironmentPrivate::translateIf(
	Syntax::If *expression, TranslatedExpression *&translated,
	Type *&type, Syntax::Tree lastloop)
{
	type = type_environment->getVoidType();
	Type *conditionType, *actionType;
	translateExpression(expression->condition, translated, conditionType, lastloop);
	if (! IsInt(conditionType))
		Error::error("If condition not integer", expression->condition->linenumber);
	translateExpression(expression->action, translated, actionType, lastloop);
}

void DeclarationsEnvironmentPrivate::translateIfElse(
	Syntax::IfElse *expression, TranslatedExpression *&translated,
	Type *&type, Syntax::Tree lastloop)
{
	Type *conditionType, *actionType, *elseType;
	translateExpression(expression->condition, translated, conditionType, lastloop);
	if (! IsInt(conditionType))
		Error::error("If condition not integer", expression->condition->linenumber);
	translateExpression(expression->action, translated, actionType, lastloop);
	translateExpression(expression->elseaction, translated, elseType, lastloop);
	if ((actionType->basetype == TYPE_VOID) && (elseType->basetype == TYPE_VOID) ||
			CheckComparisonTypes(actionType, elseType)) {
		if (actionType->basetype == TYPE_NIL)
			type = elseType;
		else
			type = actionType;
	} else {
		Error::error("Incompatible or improper types for then and else", expression->linenumber);
		type = type_environment->getErrorType();
	}
}

void DeclarationsEnvironmentPrivate::translateWhile(
	Syntax::While *expression, TranslatedExpression *&translated,
	Type *&type)
{
	type = type_environment->getVoidType();
	Type *conditionType, *actionType;
	translateExpression(expression->condition, translated, conditionType, expression);
	if (! IsInt(conditionType))
		Error::error("While condition not integer", expression->condition->linenumber);
	translateExpression(expression->action, translated, actionType, expression);
}

void DeclarationsEnvironmentPrivate::translateFor(
	Syntax::For *expression, TranslatedExpression *&translated,
	Type *&type)
{
	type = type_environment->getVoidType();
	Type *loopType, *actionType;
	translateExpression(expression->start, translated, loopType, expression);
	if (! IsInt(loopType))
		Error::error("For loop start not integer", expression->start->linenumber);
	translateExpression(expression->stop, translated, loopType, expression);
	if (! IsInt(loopType))
		Error::error("For loop 'to' not integer", expression->stop->linenumber);
	func_and_var_names.newLayer();
	LoopVariable loopvar(expression->variable->name, type_environment->getLoopIntType());
	func_and_var_names.add(expression->variable->name, &loopvar);
	translateExpression(expression->action, translated, actionType, expression);
	func_and_var_names.removeLastLayer();
}

void DeclarationsEnvironmentPrivate::translateScope(
	Syntax::Scope *expression, TranslatedExpression *&translated,
	Type *&type, Syntax::Tree lastloop)
{
	newLayer();
	processDeclarations(expression->declarations);
	for (std::list<Syntax::Tree>::iterator exp = expression->action->expressions.begin();
			exp != expression->action->expressions.end(); exp++)
		translateExpression(*exp, translated, type);
	removeLastLayer();
}

void DeclarationsEnvironmentPrivate::translateRecordField(
	Syntax::RecordField *expression, TranslatedExpression *&translated,
	Type *&type, Syntax::Tree lastloop)
{
	Type *recType;
	translateExpression(expression->record, translated, recType, lastloop);
	if (recType->basetype != TYPE_RECORD) {
		if (recType->basetype != TYPE_ERROR)
			Error::error("What is being got field of is not a record",
				expression->record->linenumber);
		type = type_environment->getErrorType();
	} else {
		RecordType *record = (RecordType *)recType;
		RecordType::FieldsMap::iterator field = record->fields.find(expression->field->name);
		if (field == record->fields.end()) {
			Error::error("This field is not in that record",
				expression->field->linenumber);
			type = type_environment->getErrorType();
		} else
			type = (*field).second->type->resolve();
	}
}

void DeclarationsEnvironmentPrivate::translateFunctionCall(
	Syntax::FunctionCall *expression, TranslatedExpression *&translated,
	Type *&type, Syntax::Tree lastloop)
{
	Declaration *var_or_function = (Declaration *)
		func_and_var_names.lookup(expression->function->name);
	if (var_or_function == NULL) {
		Error::error(std::string("Undefined variable or function identifier ") +
			expression->function->name,
			expression->function->linenumber);
		type = type_environment->getErrorType();
		return;
	}
	if (var_or_function->kind != DECL_FUNCTION) {
		Error::error("Variable used as a function",
			expression->function->linenumber);
		type = type_environment->getErrorType();
		return;
	}
	Function *function = (Function *)var_or_function;
	type = function->return_type->resolve();
	std::list<FunctionArgument>::iterator function_arg = function->arguments.begin();
	bool already_too_many;
	for (std::list<Syntax::Tree>::iterator passed_arg = expression->arguments->expressions.begin();
		 passed_arg != expression->arguments->expressions.end(); passed_arg++) {
		Type *argument_type = type_environment->getErrorType();
		Type *passed_type;
		translateExpression(*passed_arg, translated, passed_type);
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
}

void DeclarationsEnvironmentPrivate::translateRecordInstantiation(
	Syntax::RecordInstantiation *expression, TranslatedExpression *&translated,
	Type *&type, Syntax::Tree lastloop)
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
			translateExpression(fieldvalue->right, translated, valueType, lastloop);
			if (! CheckAssignmentTypes(field_type, valueType))
				Error::error("Incompatible type for this field value",
					fieldvalue->right->linenumber);
		}
		record_field++;
	}
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

void DeclarationsEnvironmentPrivate::translateExpression(Syntax::Tree expression,
	TranslatedExpression *&translated, Type*& type,
	Syntax::Tree lastloop)
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
			translateIdentifier((Syntax::Identifier *)expression, translated, type);
			break;
		case Syntax::NIL:
			type = type_environment->getNilType();
			break;
		case Syntax::BINARYOP:
			translateBinaryOperation((Syntax::BinaryOp *)expression, translated,
				type, lastloop);
			break;
		case Syntax::SEQUENCE:
			type = type_environment->getVoidType();
			for (std::list<Syntax::Tree>::iterator e = ((Syntax::Sequence *)expression)->content->expressions.begin();
					e != ((Syntax::Sequence *)expression)->content->expressions.end();
					e++)
				translateExpression(*e, translated, type, lastloop);
			break;
		case Syntax::ARRAYINDEXING:
			translateArrayIndexing((Syntax::ArrayIndexing *)expression, translated,
				type, lastloop);
			break;
		case Syntax::ARRAYINSTANTIATION:
			translateArrayInstantiation((Syntax::ArrayInstantiation *)expression,
				translated, type, lastloop);
			break;
		case Syntax::IF:
			translateIf((Syntax::If *)expression, translated, type, lastloop);
			break;
		case Syntax::IFELSE:
			translateIfElse((Syntax::IfElse *)expression, translated, type, lastloop);
			break;
		case Syntax::WHILE:
			translateWhile((Syntax::While *)expression, translated, type);
			break;
		case Syntax::FOR:
			translateFor((Syntax::For *)expression, translated, type);
			break;
		case Syntax::BREAK:
			if (lastloop == NULL)
				Error::error("Break outside of a loop", expression->linenumber);
			type = type_environment->getVoidType();
			break;
		case Syntax::SCOPE:
			translateScope((Syntax::Scope *)expression, translated, type, lastloop);
			break;
		case Syntax::RECORDFIELD:
			translateRecordField((Syntax::RecordField *)expression, translated, type,
				lastloop);
			break;
		case Syntax::FUNCTIONCALL:
			translateFunctionCall((Syntax::FunctionCall *)expression, translated, type,
				lastloop);
			break;
		case Syntax::RECORDINSTANTIATION:
			translateRecordInstantiation((Syntax::RecordInstantiation *)expression,
				translated, type, lastloop);
			break;
		default:
			Error::fatalError(std::string("Unexpected ") + NODETYPENAMES[expression->type]);
	}
}

DeclarationsEnvironment::DeclarationsEnvironment()
{
	impl = new DeclarationsEnvironmentPrivate;
}

DeclarationsEnvironment::~DeclarationsEnvironment()
{
	delete impl;
}

void DeclarationsEnvironment::translateExpression(Syntax::Tree expression,
	TranslatedExpression*& translated, Type*& type, Syntax::Tree lastloop)
{
	impl->translateExpression(expression, translated, type, lastloop);
}


}
