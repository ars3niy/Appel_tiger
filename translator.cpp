#include "translator.h"
#include "errormsg.h"
#include "intermediate.h"
#include "ir_transformer.h"
#include "debugprint.h"
#include <list>
#include <set>
#include <string.h>
#include <map>

namespace Semantic {

class TypesEnvironment {
private:
	std::list<Type*> types;
	LayeredMap typenames;
	std::list<ForwardReferenceType *> unknown_types;
	Type int_type, looped_int, string_type, error_type, void_type, nil_type;
	IdTracker id_provider;
	IR::AbstractFrameManager *framemanager;
	
	Type *createArrayType(Syntax::ArrayTypeDefinition *definition, bool allow_forward_references);
	Type *createRecordType(Syntax::RecordTypeDefinition *definition, bool allow_forward_references);
public:
	TypesEnvironment(IR::AbstractFrameManager *_framemanager);
	~TypesEnvironment();
	
	Type *getIntType() {return &int_type;}
	Type *getLoopIntType() {return &looped_int;}
	Type *getStringType() {return &string_type;}
	Type *getErrorType() {return &error_type;}
	Type *getVoidType() {return &void_type;}
	Type *getNilType() {return &nil_type;}
	Type *getPointerType() {return &string_type;}
	Type *getType(Syntax::Tree definition, bool allow_forward_references);

	int getTypeSize(Type *type);
	
	void newLayer() {typenames.newLayer();}
	void removeLastLayer() {typenames.removeLastLayer();}
	void processTypeDeclarationBatch(std::list<Syntax::Tree>::iterator begin,
		std::list<Syntax::Tree>::iterator end);
};

class TranslatorPrivate: public DebugPrinter, public IR::ParentFpHandler {
private:
	LayeredMap func_and_var_names;
	TypesEnvironment *type_environment;
	Variable *undefined_variable;
	typedef std::map<std::string, IR::Blob *> BlobsMap;
	BlobsMap blobs_by_string;
	Function *getmem_func, *getmem_fill_func, *strcmp_func;

	void newLayer();
	void removeLastLayer();
	void processDeclarations(Syntax::ExpressionList *declarations,
		IR::AbstractFrame *currentFrame, std::list<Variable *> &new_vars);
	Declaration *findVariableOrFunction(Syntax::Identifier *id);

	void processVariableDeclaration(std::shared_ptr<Syntax::VariableDeclaration> declaration,
		IR::AbstractFrame *currentFrame, std::list<Variable *> &new_vars);
	void processFunctionDeclarationBatch(std::list<Syntax::Tree>::iterator begin,
		std::list<Syntax::Tree>::iterator end, IR::AbstractFrame *currentFrame);
// 	void prependPrologue(IR::Code &translated,
// 		IR::AbstractFrame *func_frame);
	
	void translateIntValue(int value, IR::Code &translated);
	void translateStringValue(Syntax::StringValue *expression, IR::Code &translated);
	void translateIdentifier(Syntax::Identifier *expression, IR::Code &translated,
		Type *&type, IR::AbstractFrame *currentFrame);
	Type *getOpResultType(Type *leftType, Type *rightType,
		Syntax::BinaryOp *expression);
	void translateBinaryOperation(Syntax::BinaryOp *expression,
		IR::Code &translated, Type *&type, IR::Label *last_loop_exit,
		IR::AbstractFrame *currentFrame);
	void translateSequence(const std::list<Syntax::Tree> &expressions,
		IR::Code &translated, Type *&type, IR::Label *last_loop_exit,
		IR::AbstractFrame *currentFrame,
		std::shared_ptr<IR::StatementSequence> existing_sequence = nullptr);
	void translateArrayIndexing(Syntax::ArrayIndexing *expression,
		IR::Code &translated, Type *&type, IR::Label *last_loop_exit,
		IR::AbstractFrame *currentFrame);
	void translateArrayInstantiation(Syntax::ArrayInstantiation *expression,
		IR::Code &translated, Type *&type, IR::Label *last_loop_exit,
		IR::AbstractFrame *currentFrame);
	/**
	 * elseaction can be NULL for if (if-then-else)-then without outer else
	 */
	bool translateIf_IfElse_Then_MaybeElse(Syntax::IfElse *condition,
		Syntax::Tree action, Syntax::Tree elseaction, bool converted_from_logic,
		IR::Code &translated,
		Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame);
	void translateIf(Syntax::If *expression, IR::Code &translated,
		Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame);
	void translateIfElse(Syntax::IfElse *expression, IR::Code &translated,
		Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame);
	void translateWhile(Syntax::While *expression, IR::Code &translated,
		Type *&type, IR::AbstractFrame *currentFrame);
	void translateFor(std::shared_ptr<Syntax::For> expression, IR::Code &translated,
		Type *&type, IR::AbstractFrame *currentFrame);
	void translateBreak(IR::Code &translated, IR::Label *loop_exit);
	void translateScope(Syntax::Scope *expression, IR::Code &translated,
		Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame);
	void translateRecordField(Syntax::RecordField *expression, IR::Code &translated,
		Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame);

	class CallRecord {
	public:
		std::shared_ptr<IR::CallExpression> call;
		IR::AbstractFrame *calling_frame;
		
		CallRecord(std::shared_ptr<IR::CallExpression> _call, IR::AbstractFrame *frame) :
			call(_call), calling_frame(frame) {}
	};
	std::list<IR::AbstractFrame *> frames_with_added_parentfps;
	std::map<int, std::list<CallRecord> > calls_to_frames;
		
	void makeCallCode(Function *function, std::list<IR::Code >arguments,
		IR::Code &result, IR::AbstractFrame *currentFrame);
	void translateFunctionCall(Syntax::FunctionCall *expression, IR::Code &translated,
		Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame);
	void translateRecordInstantiation(Syntax::RecordInstantiation *expression, IR::Code &translated,
		Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame);
public:
	IR::IREnvironment *IRenvironment;
	IR::IRTransformer *IRtransformer;
	std::list<Variable> variables;
	IR::AbstractFrameManager *framemanager;
	std::list<Function> functions;
	
	TranslatorPrivate(IR::IREnvironment *ir_inv,
		IR::AbstractFrameManager *_framemanager);
	virtual ~TranslatorPrivate();
	
	void translateExpression(Syntax::Tree expression,
		IR::Code &translated, Type *&type,
		IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame,
		bool expect_comparison);
	virtual void notifyFrameWithParentFp(IR::AbstractFrame *frame);
	void prespillRegisters(IR::Code code, IR::AbstractFrame *frame);
	void addParentFpToCalls();
};

TypesEnvironment::TypesEnvironment(IR::AbstractFrameManager *_framemanager) :
	int_type(TYPE_INT),
	looped_int(TYPE_INT),
	string_type(TYPE_STRING),
	error_type(TYPE_ERROR),
	void_type(TYPE_VOID),
	nil_type(TYPE_NIL)
{
	framemanager = _framemanager;
	
	looped_int.is_loop_variable = true;
	typenames.add("int", &int_type);
	typenames.add("string", &string_type);
}

TypesEnvironment::~TypesEnvironment()
{
	for (Type *t: types)
		delete t;
}

int TypesEnvironment::getTypeSize(Type *type)
{
	if (type->basetype == TYPE_NAMEREFERENCE)
		return getTypeSize(type->resolve());

	if (type->basetype== TYPE_INT)
		return framemanager->getIntSize();
	else
		return framemanager->getPointerSize();
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
	
	for (Syntax::Tree t: definition->fields->expressions) {
		assert(t->type == Syntax::PARAMETERDECLARATION);
		std::shared_ptr<Syntax::ParameterDeclaration> param =
			std::static_pointer_cast<Syntax::ParameterDeclaration>(t);
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
			return createArrayType(std::static_pointer_cast<Syntax::ArrayTypeDefinition>(definition).get(),
				allow_forward_references);
		case Syntax::RECORDTYPEDEFINITION:
			return createRecordType(std::static_pointer_cast<Syntax::RecordTypeDefinition>(definition).get(),
				allow_forward_references);
		case Syntax::IDENTIFIER: {
			std::string &identifier = std::static_pointer_cast<Syntax::Identifier>(definition)->name;
			Semantic::Type *type = (Semantic::Type *)typenames.lookup(identifier);
			if (type == NULL) {
				if (! allow_forward_references) {
					Error::error(std::string("Undefined type ") +
						std::static_pointer_cast<Syntax::Identifier>(definition)->name);
					return &error_type;
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
	return NULL;
}

void TypesEnvironment::processTypeDeclarationBatch(std::list<Syntax::Tree>::iterator begin,
		std::list<Syntax::Tree>::iterator end)
{
	unknown_types.clear();
	std::list<Type *> new_records;
	std::set<std::string> types_in_the_batch;
	for (std::list<Syntax::Tree>::iterator i = begin; i != end; i++) {
		assert((*i)->type == Syntax::TYPEDECLARATION);
		std::shared_ptr<Syntax::TypeDeclaration> declaration =
			std::static_pointer_cast<Syntax::TypeDeclaration>(*i);
		if (types_in_the_batch.find(declaration->name->name) != types_in_the_batch.end())
			Error::error("Type " + declaration->name->name +
				"redefined in a same batch of consequtive types", declaration->name->linenumber);
		types_in_the_batch.insert(declaration->name->name);
		Type *type = getType(declaration->definition, true);
		assert(type != NULL);
		if (type->basetype == TYPE_RECORD)
			new_records.push_back(type);
		Type *existing = (Type *)typenames.lookup_last_layer(declaration->name->name);
		if (existing != NULL) {
			if ((existing->basetype == TYPE_NAMEREFERENCE) &&
				(((ForwardReferenceType *)existing)->meaning == NULL)) {
				((ForwardReferenceType *)existing)->meaning = type;
			}
		} else
			typenames.add(declaration->name->name, type);
	}
	
	for (ForwardReferenceType *t: unknown_types) {
		if (t->meaning == NULL) {
			Error::error(std::string("Type not defined in this scope: ") + t->name,
						 t->reference_position->linenumber);
			t->meaning = &error_type;
		}
	}
		
	for (ForwardReferenceType *t: unknown_types) {
		std::set<ForwardReferenceType *> visited_types;
		ForwardReferenceType *current = t;
		while (current->meaning->basetype == TYPE_NAMEREFERENCE) {
			visited_types.insert(current);
			ForwardReferenceType *next = (ForwardReferenceType *)current->meaning;
			if (visited_types.find(next) != visited_types.end()) {
				Error::error(std::string("Type ") + current->name +
					std::string(" being defined as ") + 
					next->name + " creates a circular reference",
					current->reference_position->linenumber);
				current->meaning = &error_type;
				break;
			}
			current = next;
		}
	}
	
	for (Type *rec: new_records) {
		assert(rec->basetype == TYPE_RECORD);
		RecordType *record = (RecordType *) rec;
		record->data_size = 0;
		for (RecordField &field: record->field_list) {
			field.offset = record->data_size;
			framemanager->updateRecordSize(record->data_size, 
				getTypeSize(field.type));
		}
	}
	
	unknown_types.clear();
}

TranslatorPrivate::TranslatorPrivate(IR::IREnvironment *ir_inv,
	IR::AbstractFrameManager * _framemanager): DebugPrinter("translator.log")
{
	IRenvironment = ir_inv;
	framemanager = _framemanager;
	type_environment = new TypesEnvironment(_framemanager);
	IRtransformer = new IR::IRTransformer(ir_inv);
	framemanager->setFrameParentFpNotificationHandler(this);
	
	undefined_variable = new Variable("undefined", type_environment->getErrorType(),
		NULL, NULL);
	functions.push_back(Function("print", type_environment->getVoidType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment->addLabel("__print")));
	functions.back().addArgument("s", type_environment->getStringType(), NULL);
	func_and_var_names.add("print", &(functions.back()));

	functions.push_back(Function("flush", type_environment->getVoidType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment->addLabel("__flush")));
	func_and_var_names.add("flush", &(functions.back()));

	functions.push_back(Function("getchar", type_environment->getStringType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment->addLabel("__getchar")));
	func_and_var_names.add("getchar", &(functions.back()));

	functions.push_back(Function("ord", type_environment->getIntType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment->addLabel("__ord")));
	functions.back().addArgument("s", type_environment->getStringType(), NULL);
	func_and_var_names.add("ord", &(functions.back()));

	functions.push_back(Function("chr", type_environment->getStringType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment->addLabel("__chr")));
	functions.back().addArgument("i", type_environment->getIntType(), NULL);
	func_and_var_names.add("chr", &(functions.back()));

	functions.push_back(Function("size", type_environment->getIntType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment->addLabel("__size")));
	functions.back().addArgument("s", type_environment->getStringType(), NULL);
	func_and_var_names.add("size", &(functions.back()));

	functions.push_back(Function("substring", type_environment->getStringType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment->addLabel("__substring")));
	functions.back().addArgument("s", type_environment->getStringType(), NULL);
	functions.back().addArgument("first", type_environment->getIntType(), NULL);
	functions.back().addArgument("n", type_environment->getIntType(), NULL);
	func_and_var_names.add("substring", &(functions.back()));

	functions.push_back(Function("concat", type_environment->getStringType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment->addLabel("__concat")));
	functions.back().addArgument("s1", type_environment->getStringType(), NULL);
	functions.back().addArgument("s2", type_environment->getStringType(), NULL);
	func_and_var_names.add("concat", &(functions.back()));

	functions.push_back(Function("not", type_environment->getIntType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment->addLabel("__not")));
	functions.back().addArgument("i", type_environment->getIntType(), NULL);
	func_and_var_names.add("not", &(functions.back()));

	functions.push_back(Function("exit", type_environment->getVoidType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment->addLabel("exit")));
	functions.back().addArgument("i", type_environment->getIntType(), NULL);
	func_and_var_names.add("exit", &(functions.back()));

	functions.push_back(Function("getmem", type_environment->getIntType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment->addLabel("__getmem")));
	functions.back().addArgument("size", type_environment->getIntType(), NULL);
	//func_and_var_names.add("getmem", &(functions.back()));
	getmem_func = &(functions.back());

	functions.push_back(Function("getmem_fill", type_environment->getIntType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment->addLabel("__getmem_fill")));
	functions.back().addArgument("elemcount", type_environment->getIntType(), NULL);
	functions.back().addArgument("value", type_environment->getIntType(), NULL);
	//func_and_var_names.add("getmem_fill", &(functions.back()));
	getmem_fill_func = &(functions.back());

	functions.push_back(Function("strcmp", type_environment->getIntType(),
		NULL, NULL, framemanager->rootFrame(), IRenvironment->addLabel("__strcmp")));
	functions.back().addArgument("size", type_environment->getIntType(), NULL);
	strcmp_func = &(functions.back());

}

TranslatorPrivate::~TranslatorPrivate()
{
	delete undefined_variable;
	delete type_environment;
	delete IRtransformer;
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
				(((ArrayType *)left)->id ==
				((ArrayType *)right)->id);
		case TYPE_RECORD:
			return 
				(right->basetype == TYPE_NIL) ||
				((right->basetype == left->basetype) &&
				(((RecordType *)left)->id ==
				((RecordType *)right)->id));
		case TYPE_ERROR:
			return true;
		default:
			Error::fatalError(std::string("Not supposed to assign to type ") + TypeToStr(left));
	}
	return false;
}

void TranslatorPrivate::processVariableDeclaration(
	std::shared_ptr<Syntax::VariableDeclaration> declaration, IR::AbstractFrame *currentFrame,
	std::list<Variable *> &new_vars)
{
	Type *vartype;
	if (declaration->type != NULL)
		vartype = type_environment->getType(declaration->type, false)->resolve();
	else
		vartype = NULL;
	Type *exprtype;
	IR::Code translatedValue;
	translateExpression(declaration->value, translatedValue, exprtype, NULL,
		currentFrame, true);
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
// 	printf("Variable %s at line %d is %s by address\n",
// 		declaration->name->name.c_str(),
// 		declaration->linenumber,
// 		variables_extra_info.isAccessedByAddress(declaration) ? "accessed" : "not accessed");
	variables.push_back(Variable(declaration->name->name, vartype,
		translatedValue,
		currentFrame->addVariable(declaration->name->name,
			type_environment->getTypeSize(vartype))));
	func_and_var_names.add(declaration->name->name, &(variables.back()));
	new_vars.push_back(&(variables.back()));
}

// void TranslatorPrivate::prependPrologue(IR::Code& translated,
// 	IR::AbstractFrame* func_frame)
// {
// 	std::shared_ptr<IR::StatementSequence> sequence = std::make_shared<IR::StatementSequence>();
// 	
// 	for (const IR::AbstractFrame::ParameterMovement &move:
// 			func_frame->getMovementPrologue()) {
// 		ToStatementSequence(sequence)->addStatement(std::make_shared<IR::MoveStatement>(
// 			move.where_store->createCode(func_frame),
// 			move.parameter->createCode(func_frame)
// 		));
// 	}
// 	
// 	switch (translated->kind) {
// 		case IR::CODE_EXPRESSION:
// 			std::static_pointer_cast<IR::ExpressionCode>(translated)->exp =
// 				std::make_shared<IR::StatExpSequence>(sequence,
// 					std::static_pointer_cast<IR::ExpressionCode>(translated)->exp);
// 			break;
// 		case IR::CODE_STATEMENT:
// 		case IR::CODE_JUMP_WITH_PATCHES:
// 			ToStatementSequence(sequence)->addStatement(
// 				std::static_pointer_cast<IR::StatementCode>(translated)->statm);
// 			std::static_pointer_cast<IR::StatementCode>(translated)->statm = sequence;
// 			break;
// 	}
// 	
// }


void TranslatorPrivate::processFunctionDeclarationBatch(
	std::list<Syntax::Tree>::iterator begin,
	std::list<Syntax::Tree>::iterator end, IR::AbstractFrame *currentFrame)
{
	std::list<Function *> recent_functions;
	std::set<std::string> names_in_batch;
	for (std::list<Syntax::Tree>::iterator f = begin; f != end; f++) {
		assert((*f)->type == Syntax::FUNCTION);
		Syntax::Function *declaration = std::static_pointer_cast<Syntax::Function>(*f).get();
		if (names_in_batch.find(declaration->name->name) != names_in_batch.end())
			Error::error("Function " + declaration->name->name +
				"redefined in a successive batch of functions", declaration->name->linenumber);
		names_in_batch.insert(declaration->name->name);
		Type *return_type;
		if (declaration->type == NULL)
			return_type = type_environment->getVoidType();
		else
			return_type = type_environment->getType(declaration->type, false)->resolve();
		IR::Label *function_label;
		if (declaration->body != NULL) {
			function_label = IRenvironment->addLabel();
			function_label->appendToName(std::string("_") + declaration->name->name);
		} else
			function_label = IRenvironment->addLabel(declaration->name->name.c_str());
		functions.push_back(Function(declaration->name->name, return_type,
			declaration->body, NULL,
			framemanager->newFrame(currentFrame, function_label->getName()),
			function_label));
		Function *function = &(functions.back());
		debug("New frame %s, id %d", function->frame->getName().c_str(),
				function->frame->getId());
		func_and_var_names.add(declaration->name->name, function);
		recent_functions.push_back(function);
		
		for (Syntax::Tree param: declaration->parameters->expressions) {
			assert(param->type == Syntax::PARAMETERDECLARATION);
			std::shared_ptr<Syntax::ParameterDeclaration> param_decl = 
				std::static_pointer_cast<Syntax::ParameterDeclaration>(param);
			Type *param_type = type_environment->getType(param_decl->type, false)->resolve();
			function->addArgument(param_decl->name->name, param_type,
				function->frame->addParameter(
					param_decl->name->name,
					type_environment->getTypeSize(param_type)
				)
			);
		}
	}
	for (Function *fcn: recent_functions) {
		if (fcn->raw_body == NULL)
			continue;
		
		Type *actual_return_type;
		func_and_var_names.newLayer();
		for (FunctionArgument &param: fcn->arguments)
			func_and_var_names.add(param.name, &param);
		translateExpression(fcn->raw_body, fcn->body, actual_return_type,
			NULL, fcn->frame, true);
		func_and_var_names.removeLastLayer();
		if ((fcn->return_type->basetype != TYPE_VOID) &&
				! CheckAssignmentTypes(fcn->return_type, actual_return_type))
			Error::error("Type of function body doesn't match specified return type",
				fcn->raw_body->linenumber);
		else if ((fcn->return_type->basetype == TYPE_VOID) &&
				(actual_return_type->basetype != TYPE_VOID) &&
				(actual_return_type->basetype != TYPE_ERROR))
			Error::error("Body of a function without return value produces a value",
				fcn->raw_body->linenumber);
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
			processVariableDeclaration(std::static_pointer_cast<Syntax::VariableDeclaration>(declaration),
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

void TranslatorPrivate::translateIntValue(int value, IR::Code &translated)
{
	translated = std::make_shared<IR::ExpressionCode>(std::make_shared<IR::IntegerExpression>(value));
}

void TranslatorPrivate::translateStringValue(Syntax::StringValue *expression,
	IR::Code &translated)
{
	IR::Blob *blob;
	BlobsMap::iterator existing_blob = blobs_by_string.find(expression->value);
	if (existing_blob != blobs_by_string.end())
		blob = (*existing_blob).second;
	else {
		blob = IRenvironment->addBlob();
		int intsize = framemanager->getIntSize();
		blob->data.resize(expression->value.size() + intsize);
		*((int *)blob->data.data()) = expression->value.size();
		memmove(blob->data.data() + intsize, expression->value.c_str(), 
			expression->value.size());
		blobs_by_string.insert(std::make_pair(expression->value, blob));
	}
	translated = std::make_shared<IR::ExpressionCode>(std::make_shared<IR::LabelAddressExpression>(
		blob->label));
}

IR::Code ErrorPlaceholderCode()
{
	return std::make_shared<IR::ExpressionCode>(nullptr);
}

void TranslatorPrivate::translateIdentifier(Syntax::Identifier *expression,
	IR::Code &translated, Type *&type, IR::AbstractFrame *currentFrame)
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
		translated = std::make_shared<IR::ExpressionCode>(
			((Variable *)var_or_function)->implementation->createCode(currentFrame)
		);
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
				((right->basetype == left->basetype) &&
				(((RecordType *)left)->id == ((RecordType *)right)->id));
		case TYPE_NIL:
			return right->basetype == TYPE_RECORD;
		default:
			return false;
	}
	return false;
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
	return NULL;
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
		default:
			return IR::OP_SHAR;
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
		default:
			return IR::OP_UGREATEQUAL;
	}
	return IR::OP_UGREATEQUAL;
}

void TranslatorPrivate::translateBinaryOperation(Syntax::BinaryOp *expression,
	IR::Code &translated, Type *&type, IR::Label *last_loop_exit,
	IR::AbstractFrame *currentFrame)
{
	IR::Code left, right;
	Type *leftType, *rightType;
	translateExpression(expression->left, left, leftType, last_loop_exit, currentFrame, true);
	translateExpression(expression->right, right, rightType, last_loop_exit, currentFrame, true);
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
			IR::Expression left_expr = IRenvironment->codeToExpression(left);
			IR::Expression right_expr = IRenvironment->codeToExpression(right);
			std::shared_ptr<IR::BinaryOpExpression> bin_op =
				std::make_shared<IR::BinaryOpExpression>(
					getIRBinaryOp(expression->operation), left_expr, right_expr);
			// In case we need to report constant division by zero
			bin_op->position = expression->linenumber;
			translated = std::make_shared<IR::ExpressionCode>(bin_op);
			break;
		}
		case SYM_EQUAL:
		case SYM_NONEQUAL:
		case SYM_LESS:
		case SYM_LESSEQUAL:
		case SYM_GREATER:
		case SYM_GREATEQUAL: {
			std::shared_ptr<IR::CondJumpStatement> compare_statm;
			IR::Expression left_expr, right_expr;
			if (leftType->basetype == TYPE_STRING) {
				std::list<IR::Code> args;
				args.push_back(left);
				args.push_back(right);
				IR::Code strcmp_call;
				makeCallCode(strcmp_func, args, strcmp_call, currentFrame);
				left_expr = IRenvironment->codeToExpression(strcmp_call);
				right_expr = std::make_shared<IR::IntegerExpression>(0);
			} else {
				left_expr = IRenvironment->codeToExpression(left);
				right_expr = IRenvironment->codeToExpression(right);
			}
			compare_statm = std::make_shared<IR::CondJumpStatement>(
				getIRComparisonOp(expression->operation), left_expr, right_expr,
				nullptr, nullptr);
			std::shared_ptr<IR::CondJumpPatchesCode> result =
				std::make_shared<IR::CondJumpPatchesCode>(compare_statm);
			result->replace_true.push_back(&compare_statm->true_dest);
			result->replace_false.push_back(&compare_statm->false_dest);
			translated = result;
			break;
		}
		case SYM_ASSIGN: {
			IR::Expression left_expr = IRenvironment->codeToExpression(left);
			IR::Expression right_expr = IRenvironment->codeToExpression(right);
			translated = std::make_shared<IR::StatementCode>(
				std::make_shared<IR::MoveStatement>(left_expr, right_expr));
			break;
		}
		default:
			Error::fatalError("I forgot to handle this operation",
				expression->left->linenumber);
	}
}

void TranslatorPrivate::translateSequence(const std::list<Syntax::Tree> &expressions,
	IR::Code &translated, Type *&type, IR::Label *last_loop_exit,
	IR::AbstractFrame *currentFrame,
	std::shared_ptr<IR::StatementSequence> existing_sequence)
{
	type = type_environment->getVoidType();
	std::shared_ptr<IR::StatementSequence> sequence = existing_sequence;
	if (! sequence)
		sequence = std::make_shared<IR::StatementSequence>();
	IR::Expression last_expression = nullptr;
	for (std::list<Syntax::Tree>::const_iterator e = expressions.begin();
			e != expressions.end(); ) {
		auto next = e;
		next++;
		IR::Code item_code;
		translateExpression(*e, item_code, type, last_loop_exit, currentFrame, false);
		if ((next != expressions.end()) || type->basetype == TYPE_VOID)
			sequence->addStatement(IRenvironment->codeToStatement(item_code));
		else
			last_expression = IRenvironment->codeToExpression(item_code);
		e = next;
	}
	if (! last_expression)
		translated = std::make_shared<IR::StatementCode>(sequence);
	else
		if (! sequence->statements.empty())
			translated = std::make_shared<IR::ExpressionCode>(
				std::make_shared<IR::StatExpSequence>(
				sequence, last_expression));
		else
			translated = std::make_shared<IR::ExpressionCode>(last_expression);
}

void TranslatorPrivate::translateArrayIndexing(
	Syntax::ArrayIndexing *expression,
	IR::Code &translated, Type *&type, IR::Label *last_loop_exit,
	IR::AbstractFrame *currentFrame)
{
	IR::Code array, index;
	Type *arrayType, *indexType;
	translateExpression(expression->array, array, arrayType, last_loop_exit, currentFrame, true);
	if (! IsArray(arrayType))
		Error::error("Not an array got indexed", expression->array->linenumber);
	translateExpression(expression->index, index, indexType, last_loop_exit, currentFrame, true);
	if (! IsInt(indexType))
		Error::error("Index is not an integer", expression->index->linenumber);
	if (arrayType->basetype == TYPE_ARRAY) {
		type = ((ArrayType *)arrayType)->elemtype->resolve();
		std::shared_ptr<IR::BinaryOpExpression> offset = std::make_shared<IR::BinaryOpExpression>(
			IR::OP_MUL, IRenvironment->codeToExpression(index),
			std::make_shared<IR::IntegerExpression>(type_environment->getTypeSize(type)));
		std::shared_ptr<IR::BinaryOpExpression> target_address = std::make_shared<IR::BinaryOpExpression>(
			IR::OP_PLUS, IRenvironment->codeToExpression(array), offset);
		translated = std::make_shared<IR::ExpressionCode>(
			std::make_shared<IR::MemoryExpression>(target_address));
	} else {
		type = type_environment->getErrorType();
		translated = ErrorPlaceholderCode();
	}
}

void TranslatorPrivate::translateArrayInstantiation(
	Syntax::ArrayInstantiation *expression, IR::Code &translated,
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
	IR::Code length;
	Type *lengthType;
	translateExpression(expression->arraydef->index, length, lengthType, last_loop_exit,
		currentFrame, true);
	if (! IsInt(lengthType))
		Error::error("Not an integer as array length", expression->arraydef->index->linenumber);

	IR::Code value;
	Type *valueType;
	translateExpression(expression->value, value, valueType, last_loop_exit, currentFrame, true);
	if ((elem_type != NULL) && ! CheckAssignmentTypes(elem_type, valueType))
		Error::error("Value type doesn't match array element type",
			expression->value->linenumber);
	
	if (elem_type != NULL) {
		std::list<IR::Code > args;
		args.push_back(length);
		args.push_back(value);
		makeCallCode(getmem_fill_func, args, translated, currentFrame);
	} else
		translated = ErrorPlaceholderCode();
}

/**
 * elseaction can be NULL for if-then without else
 */
bool TranslatorPrivate::translateIf_IfElse_Then_MaybeElse(Syntax::IfElse *condition,
	Syntax::Tree action, Syntax::Tree elseaction, bool converted_from_logic,
	IR::Code &translated,
	Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame)
{
	Type *conditionType;
	IR::Code condition_code;
	translateExpression(condition->condition, condition_code, conditionType, last_loop_exit,
		currentFrame, true);
	if (! IsInt(conditionType)) {
		if (condition->converted_from_logic)
			Error::error("Logical operand not integer", condition->condition->linenumber);
		else
			Error::error("If condition not integer", condition->condition->linenumber);
		return true;
	}
	Type *subcondition_type;
	IR::Code subcondition1_code;
	translateExpression(condition->action, subcondition1_code, subcondition_type,
		last_loop_exit, currentFrame, true);
	if (! IsInt(subcondition_type)) {
		if (condition->converted_from_logic)
			Error::error("Logical operand not integer", condition->action->linenumber);
		else
			Error::error("If condition not integer", condition->action->linenumber);
		return true;
	}
	IR::Code subcondition2_code;
	translateExpression(condition->elseaction, subcondition2_code, subcondition_type,
		last_loop_exit, currentFrame, true);
	if (! IsInt(subcondition_type)) {
		if (condition->converted_from_logic)
			Error::error("Logical operand not integer", condition->elseaction->linenumber);
		else
			Error::error("If condition not integer", condition->elseaction->linenumber);
		return true;
	}
	
	std::shared_ptr<IR::StatementSequence> sequence =
		std::make_shared<IR::StatementSequence>();
	std::list<IR::Label**> replace_true, replace_false;
	IR::Statement jump_by_outer = IRenvironment->codeToCondJump(
		condition_code, replace_true, replace_false);
	IR::Label *outer_true_label = IRenvironment->addLabel();
	IR::Label *outer_false_label = IRenvironment->addLabel();
	IR::putLabels(replace_true, replace_false, outer_true_label, outer_false_label);

	sequence->addStatement(jump_by_outer);
	sequence->addStatement(std::make_shared<IR::LabelPlacementStatement>(outer_true_label));
	//	statement->addStatement(IRenvironment->codeToStatement(action));
	IR::Statement jump_by_inner = IRenvironment->codeToCondJump(
		subcondition1_code, replace_true, replace_false);
	IR::Label *inner_true_label = IRenvironment->addLabel();
	IR::Label *inner_false_label = IRenvironment->addLabel();
	IR::putLabels(replace_true, replace_false, inner_true_label, inner_false_label);
	sequence->addStatement(jump_by_inner);
	sequence->addStatement(std::make_shared<IR::LabelPlacementStatement>(outer_false_label));
	jump_by_inner = IRenvironment->codeToCondJump(
		subcondition2_code, replace_true, replace_false);
	IR::putLabels(replace_true, replace_false, inner_true_label, inner_false_label);
	sequence->addStatement(jump_by_inner);
	
	IR::Code action_code, elseaction_code = nullptr;
	Type *action_type, *elseaction_type = NULL;
	translateExpression(action, action_code, action_type, last_loop_exit,
		currentFrame, converted_from_logic);
	if (elseaction)
		translateExpression(elseaction, elseaction_code, elseaction_type, last_loop_exit,
			currentFrame, converted_from_logic);
	if (! elseaction)
		type = type_environment->getVoidType();
	else {
		if (((action_type->basetype == TYPE_VOID) && (elseaction_type->basetype == TYPE_VOID)) ||
				CheckComparisonTypes(action_type, elseaction_type)) {
			if (conditionType->basetype == TYPE_ERROR)
				action_type = type_environment->getErrorType();
			else if (action_type->basetype == TYPE_NIL)
				type = elseaction_type;
			else
				type = action_type;
		} else {
			Error::error("Incompatible or improper types for then and else",
				elseaction->linenumber);
			type = type_environment->getErrorType();
		}
	}
	if (type->basetype == TYPE_ERROR) {
		translated = ErrorPlaceholderCode();
		return true;
	}
	
	sequence->addStatement(std::make_shared<IR::LabelPlacementStatement>(inner_true_label));
	IR::VirtualRegister *value_storage;
	if (type->basetype != TYPE_VOID) {
		value_storage = IRenvironment->addRegister();
		sequence->addStatement(std::make_shared<IR::MoveStatement>(
			std::make_shared<IR::RegisterExpression>(value_storage),
			IRenvironment->codeToExpression(action_code)));
	} else
		sequence->addStatement(IRenvironment->codeToStatement(action_code));

	IR::Label *finish_label = NULL;
	if (elseaction) {
		finish_label = IRenvironment->addLabel();
		sequence->addStatement(std::make_shared<IR::JumpStatement>(
			std::make_shared<IR::LabelAddressExpression>(finish_label), finish_label));
	}
	sequence->addStatement(std::make_shared<IR::LabelPlacementStatement>(inner_false_label));
	if (elseaction) {
		if (type->basetype != TYPE_VOID)
			sequence->addStatement(std::make_shared<IR::MoveStatement>(
				std::make_shared<IR::RegisterExpression>(value_storage),
				IRenvironment->codeToExpression(elseaction_code)));
		else
			sequence->addStatement(IRenvironment->codeToStatement(elseaction_code));
		sequence->addStatement(std::make_shared<IR::LabelPlacementStatement>(finish_label));
	}

	if (type->basetype == TYPE_VOID) {
		translated = std::make_shared<IR::StatementCode>(sequence);
	} else {
		translated = std::make_shared<IR::ExpressionCode>(std::make_shared<IR::StatExpSequence>(
			sequence, std::make_shared<IR::RegisterExpression>(value_storage)));
	}
	return true;
	//if ((actionType->basetype != TYPE_VOID) && (actionType->basetype != TYPE_ERROR))
	//	Error::error("Body of if-then statement is not valueless", expression->action->linenumber);
	
}

void TranslatorPrivate::translateIf(
	Syntax::If *expression, IR::Code &translated,
	Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame)
{
	type = type_environment->getVoidType();
	if (expression->condition->type == Syntax::IFELSE) {
		if (translateIf_IfElse_Then_MaybeElse((Syntax::IfElse *)expression->condition.get(),
				expression->action, nullptr, false, translated, type,
				last_loop_exit, currentFrame))
			return;
	}
	Type *conditionType, *actionType;
	IR::Code condition, action;
	translateExpression(expression->condition, condition, conditionType, last_loop_exit,
		currentFrame, true);
	if (! IsInt(conditionType))
		Error::error("If condition not integer", expression->condition->linenumber);
	translateExpression(expression->action, action, actionType, last_loop_exit,
		currentFrame, false);
	if ((actionType->basetype != TYPE_VOID) && (actionType->basetype != TYPE_ERROR))
		Error::error("Body of if-then statement is not valueless", expression->action->linenumber);
	
	if (conditionType->basetype == TYPE_INT) {
		std::list<IR::Label**> replace_true, replace_false;
		IR::Statement jump_by_condition = IRenvironment->codeToCondJump(
			condition, replace_true, replace_false);
		IR::Label *true_label = IRenvironment->addLabel();
		IR::Label *false_label = IRenvironment->addLabel();
		IR::putLabels(replace_true, replace_false, true_label, false_label);
		std::shared_ptr<IR::StatementSequence> statement = std::make_shared<IR::StatementSequence>();
		statement->addStatement(jump_by_condition);
		statement->addStatement(std::make_shared<IR::LabelPlacementStatement>(true_label));
		statement->addStatement(IRenvironment->codeToStatement(action));
		statement->addStatement(std::make_shared<IR::LabelPlacementStatement>(false_label));
		translated = std::make_shared<IR::StatementCode>(statement);
	} else
		translated = ErrorPlaceholderCode();
}

void TranslatorPrivate::translateIfElse(
	Syntax::IfElse *expression, IR::Code &translated,
	Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame)
{
	if (expression->condition->type == Syntax::IFELSE) {
		if (translateIf_IfElse_Then_MaybeElse((Syntax::IfElse *)expression->condition.get(),
				expression->action, expression->elseaction,
				expression->converted_from_logic, translated, type,
				last_loop_exit, currentFrame))
			return;
	}
	Type *conditionType, *actionType, *elseType;
	IR::Code condition_code, action_code, elseaction_code;
	translateExpression(expression->condition, condition_code, conditionType, last_loop_exit,
		currentFrame, true);
	if (! IsInt(conditionType))
		Error::error("If condition not integer", expression->condition->linenumber);
	translateExpression(expression->action, action_code, actionType, last_loop_exit,
		currentFrame, expression->converted_from_logic);
	translateExpression(expression->elseaction, elseaction_code, elseType, last_loop_exit,
		currentFrame, expression->converted_from_logic);
	if (((actionType->basetype == TYPE_VOID) && (elseType->basetype == TYPE_VOID)) ||
			CheckComparisonTypes(actionType, elseType)) {
		if (conditionType->basetype == TYPE_ERROR)
			actionType = type_environment->getErrorType();
		else if (actionType->basetype == TYPE_NIL)
			type = elseType;
		else
			type = actionType;
		if (conditionType->basetype == TYPE_INT) {
			IR::Label *true_label = IRenvironment->addLabel();
			IR::Label *false_label = IRenvironment->addLabel();
			IR::Label *finish_label = IRenvironment->addLabel();
			IR::VirtualRegister *value_storage = NULL;
			if (type->basetype != TYPE_VOID)
				value_storage = IRenvironment->addRegister();
			std::list<IR::Label**> replace_true, replace_false;
			IR::Statement condition_jump = IRenvironment->codeToCondJump(
				condition_code, replace_true, replace_false);
			IR::putLabels(replace_true, replace_false, true_label, false_label);
			std::shared_ptr<IR::StatementSequence> calculation =
				std::make_shared<IR::StatementSequence>();
			calculation->addStatement(condition_jump);
			calculation->addStatement(std::make_shared<IR::LabelPlacementStatement>(true_label));
			if (type->basetype != TYPE_VOID) {
				calculation->addStatement(std::make_shared<IR::MoveStatement>(
					std::make_shared<IR::RegisterExpression>(value_storage),
					IRenvironment->codeToExpression(action_code)));
			} else {
				calculation->addStatement(IRenvironment->codeToStatement(action_code));
			}
			calculation->addStatement(std::make_shared<IR::JumpStatement>(
				std::make_shared<IR::LabelAddressExpression>(finish_label), finish_label));
			calculation->addStatement(std::make_shared<IR::LabelPlacementStatement>(false_label));
			if (type->basetype != TYPE_VOID) {
				calculation->addStatement(std::make_shared<IR::MoveStatement>(
					std::make_shared<IR::RegisterExpression>(value_storage),
					IRenvironment->codeToExpression(elseaction_code)));
			} else {
				calculation->addStatement(IRenvironment->codeToStatement(elseaction_code));
			}
			calculation->addStatement(std::make_shared<IR::LabelPlacementStatement>(finish_label));
			if (type->basetype == TYPE_VOID) {
				translated = std::make_shared<IR::StatementCode>(calculation);
			} else {
				translated = std::make_shared<IR::ExpressionCode>(std::make_shared<IR::StatExpSequence>(
					calculation, std::make_shared<IR::RegisterExpression>(value_storage)));
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
	Syntax::While *expression, IR::Code &translated,
	Type *&type, IR::AbstractFrame *currentFrame)
{
	type = type_environment->getVoidType();
	Type *conditionType, *actionType;
	IR::Code condition, action;
	IR::Label *exit_label = IRenvironment->addLabel();
	translateExpression(expression->condition, condition, conditionType, exit_label,
		currentFrame, true);
	if (! IsInt(conditionType))
		Error::error("While condition not integer", expression->condition->linenumber);
	translateExpression(expression->action, action, actionType, exit_label,
		currentFrame, false);
	if ((actionType->basetype != TYPE_VOID) && (actionType->basetype != TYPE_ERROR))
		Error::error("Body of while loop is not valueless", expression->action->linenumber);
	if (conditionType->basetype == TYPE_INT) {
		IR::Label *loop_label = IRenvironment->addLabel();
		IR::Label *proceed_label = IRenvironment->addLabel();
		std::list<IR::Label **> replace_true, replace_false;
		IR::Statement cond_jump = IRenvironment->codeToCondJump(condition,
			replace_true, replace_false);
		IR::putLabels(replace_true, replace_false, proceed_label, exit_label);
		if (cond_jump->kind == IR::IR_COND_JUMP)
			FlipComparison(ToCondJumpStatement(cond_jump).get());
		std::shared_ptr<IR::StatementSequence> sequence = std::make_shared<IR::StatementSequence>();
		sequence->addStatement(std::make_shared<IR::LabelPlacementStatement>(loop_label));
		sequence->addStatement(cond_jump);
		sequence->addStatement(std::make_shared<IR::LabelPlacementStatement>(proceed_label));
		sequence->addStatement(IRenvironment->codeToStatement(action));
		sequence->addStatement(std::make_shared<IR::JumpStatement>(
			std::make_shared<IR::LabelAddressExpression>(loop_label), loop_label));
		sequence->addStatement(std::make_shared<IR::LabelPlacementStatement>(exit_label));
		translated = std::make_shared<IR::StatementCode>(sequence);
	} else
		translated = ErrorPlaceholderCode();
}

void TranslatorPrivate::translateFor(
	std::shared_ptr<Syntax::For> expression, IR::Code &translated,
	Type *&type, IR::AbstractFrame *currentFrame)
{
	type = type_environment->getVoidType();
	Type *from_type, *to_type, *actionType;
	IR::Code from_code, to_code, action_code;
	IR::Label *exit_label = IRenvironment->addLabel();
	translateExpression(expression->start, from_code, from_type, exit_label,
		currentFrame, false);
	if (! IsInt(from_type))
		Error::error("For loop start not integer", expression->start->linenumber);
	translateExpression(expression->stop, to_code, to_type, exit_label,
		currentFrame, false);
	if (! IsInt(to_type))
		Error::error("For loop 'to' not integer", expression->stop->linenumber);
	func_and_var_names.newLayer();
// 	printf("Loop variable %s at line %d is %s by address\n",
// 		expression->variable->name.c_str(),
// 		expression->linenumber,
// 		variables_extra_info.isAccessedByAddress(expression) ? "accessed" : "not accessed");
	variables.push_back(Variable(
		expression->variable->name,
		type_environment->getLoopIntType(), NULL, currentFrame->addVariable(
			expression->variable->name,
			type_environment->getTypeSize(from_type)
		)));
	Variable *loopvar = &(variables.back());
	func_and_var_names.add(expression->variable->name, loopvar);
	translateExpression(expression->action, action_code, actionType, exit_label,
		currentFrame, false);
	if ((from_type->basetype == TYPE_INT) && (to_type->basetype == TYPE_INT)) {
		std::shared_ptr<IR::StatementSequence> sequence = std::make_shared<IR::StatementSequence>();
		IR::VirtualRegister *upper_bound = IRenvironment->addRegister();
		sequence->addStatement(std::make_shared<IR::MoveStatement>(
			std::make_shared<IR::RegisterExpression>(upper_bound),
			IRenvironment->codeToExpression(to_code)));
		IR::Expression loopvar_expression = loopvar->implementation->createCode(currentFrame);
		sequence->addStatement(std::make_shared<IR::MoveStatement>(
			loopvar_expression,
			IRenvironment->codeToExpression(from_code)));
		IR::Label *loop_label = IRenvironment->addLabel();
		loopvar_expression = loopvar->implementation->createCode(currentFrame);
		sequence->addStatement(std::make_shared<IR::CondJumpStatement>(IR::OP_LESSEQUAL,
			loopvar_expression,
			std::make_shared<IR::RegisterExpression>(upper_bound),
			loop_label, exit_label));
		sequence->addStatement(std::make_shared<IR::LabelPlacementStatement>(loop_label));
		sequence->addStatement(IRenvironment->codeToStatement(action_code));
		IR::Label *proceed_label = IRenvironment->addLabel();
		loopvar_expression = loopvar->implementation->createCode(currentFrame);
		sequence->addStatement(std::make_shared<IR::CondJumpStatement>(IR::OP_LESS,
			loopvar_expression,
			std::make_shared<IR::RegisterExpression>(upper_bound),
			proceed_label, exit_label));
		sequence->addStatement(std::make_shared<IR::LabelPlacementStatement>(proceed_label));
		loopvar_expression = loopvar->implementation->createCode(currentFrame);
		IR::Expression loopvar_expression2 = loopvar->implementation->createCode(currentFrame);
		sequence->addStatement(std::make_shared<IR::MoveStatement>(
			loopvar_expression,
			std::make_shared<IR::BinaryOpExpression>(IR::OP_PLUS,
				loopvar_expression2,
				std::make_shared<IR::IntegerExpression>(1)
			)
		));
		sequence->addStatement(std::make_shared<IR::JumpStatement>(
			std::make_shared<IR::LabelAddressExpression>(loop_label), loop_label));
		sequence->addStatement(std::make_shared<IR::LabelPlacementStatement>(exit_label));
		translated = std::make_shared<IR::StatementCode>(sequence);
	} else
		translated = ErrorPlaceholderCode();
	func_and_var_names.removeLastLayer();
}

void TranslatorPrivate::translateBreak(IR::Code &translated, IR::Label *loop_exit)
{
	translated = std::make_shared<IR::StatementCode>(std::make_shared<IR::JumpStatement>(
		std::make_shared<IR::LabelAddressExpression>(loop_exit), loop_exit));
}

void TranslatorPrivate::translateScope(
	Syntax::Scope *expression, IR::Code &translated,
	Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame)
{
	newLayer();
	std::list<Variable *> new_vars;
	processDeclarations(expression->declarations.get(), currentFrame, new_vars);
	std::shared_ptr<IR::StatementSequence> sequence = std::make_shared<IR::StatementSequence>();
	for (Variable *newvar: new_vars)
		if (newvar->type->basetype != TYPE_ERROR) {
			IR::Code var_code = std::make_shared<IR::ExpressionCode>(
				newvar->implementation->createCode(currentFrame));
			sequence->addStatement(std::make_shared<IR::MoveStatement>(
				IRenvironment->codeToExpression(var_code),
				IRenvironment->codeToExpression(newvar->value)
			));
		}
	translateSequence(expression->action->expressions, translated, type,
		NULL, currentFrame, sequence);
	removeLastLayer();
}

void TranslatorPrivate::translateRecordField(
	Syntax::RecordField *expression, IR::Code &translated,
	Type *&type, IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame)
{
	Type *recType;
	IR::Code record_code;
	translateExpression(expression->record, record_code, recType, last_loop_exit,
		currentFrame, true);
	if (recType->basetype != TYPE_RECORD) {
		if (recType->basetype != TYPE_ERROR)
			Error::error("What is being got field of is not a record",
				expression->record->linenumber);
		type = type_environment->getErrorType();
		translated = ErrorPlaceholderCode();
	} else {
		RecordType *record = (RecordType *)recType;
		auto field = record->fields.find(expression->field->name);
		if (field == record->fields.end()) {
			Error::error("This field is not in that record",
				expression->field->linenumber);
			type = type_environment->getErrorType();
			translated = ErrorPlaceholderCode();
		} else {
			type = (*field).second->type->resolve();
			std::shared_ptr<IR::BinaryOpExpression> target_address =
				std::make_shared<IR::BinaryOpExpression>(
					IR::OP_PLUS, IRenvironment->codeToExpression(record_code),
					std::make_shared<IR::IntegerExpression>((*field).second->offset));
			translated = std::make_shared<IR::ExpressionCode>(
				std::make_shared<IR::MemoryExpression>(target_address));
		}
	}
}

void TranslatorPrivate::notifyFrameWithParentFp(IR::AbstractFrame *frame) 
{
	frames_with_added_parentfps.push_back(frame);
}

void TranslatorPrivate::makeCallCode(Function *function,
	std::list<IR::Code> arguments, IR::Code &result,
	IR::AbstractFrame *currentFrame)
{
	std::shared_ptr<IR::CallExpression> call = std::make_shared<IR::CallExpression>(
		std::make_shared<IR::LabelAddressExpression>(function->label), nullptr);
	auto call_list = calls_to_frames.find(function->frame->getId());
	if (call_list == calls_to_frames.end()) {
		auto ret = calls_to_frames.insert(std::make_pair(
			function->frame->getId(), std::list<CallRecord>()));
		assert(ret.second);
		call_list = ret.first;
	}
	call_list->second.push_back(CallRecord(call, currentFrame));
	for (IR::Code arg: arguments)
		 call->addArgument(IRenvironment->codeToExpression(arg));
	result = std::make_shared<IR::ExpressionCode>(call);
}

void TranslatorPrivate::translateFunctionCall(
	Syntax::FunctionCall *expression, IR::Code &translated,
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
	currentFrame->addFunctionCall();
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
	bool already_too_many = false;
	std::list<IR::Code> arguments_code;
	for (Syntax::Tree passed_arg: expression->arguments->expressions) {
		Type *argument_type = type_environment->getErrorType();
		Type *passed_type;
		IR::Code argument_code;
		translateExpression(passed_arg, argument_code, passed_type, NULL,
			currentFrame, true);
		arguments_code.push_back(argument_code);
		if (! already_too_many && (function_arg == function->arguments.end())) {
			already_too_many = true;
			Error::error("Too many arguments to the function",
				passed_arg->linenumber);
		}
		
		if (! already_too_many) {
			argument_type = (*function_arg).type->resolve();
			if (! CheckAssignmentTypes(argument_type, passed_type)) {
				Error::error("Incompatible type passed as an argument",
					passed_arg->linenumber);
			}
			function_arg++;
		}
	}
	if (function_arg != function->arguments.end())
		Error::error("Not enough arguments to the function",
			expression->linenumber);
	makeCallCode(function, arguments_code, translated, currentFrame);
}

void TranslatorPrivate::translateRecordInstantiation(
	Syntax::RecordInstantiation *expression, IR::Code &translated,
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
	
	IR::VirtualRegister *record_address = IRenvironment->addRegister();
	std::shared_ptr<IR::StatementSequence> sequence = std::make_shared<IR::StatementSequence>();
	std::list<IR::Code> alloc_argument;
	alloc_argument.push_back(std::make_shared<IR::ExpressionCode>(
		std::make_shared<IR::IntegerExpression>(record->data_size)));
	IR::Code alloc_code;
	makeCallCode(getmem_func, alloc_argument, alloc_code, currentFrame);
	sequence->addStatement(std::make_shared<IR::MoveStatement>(
		std::make_shared<IR::RegisterExpression>(record_address),
		IRenvironment->codeToExpression(alloc_code)));
		
	RecordType::FieldsList::iterator record_field = record->field_list.begin();
	for (auto set_field = expression->fieldvalues->expressions.begin();
			set_field != expression->fieldvalues->expressions.end(); set_field++) {
		if (record_field == record->field_list.end()) {
			Error::error("More field initialized than there are in the record",
				(*set_field)->linenumber);
			break;
		}
		Type *field_type = (*record_field).type->resolve();
		assert((*set_field)->type == Syntax::BINARYOP);
		Syntax::BinaryOp *fieldvalue = std::static_pointer_cast<Syntax::BinaryOp>(*set_field).get();
		assert(fieldvalue->operation == SYM_ASSIGN);
		assert(fieldvalue->left->type == Syntax::IDENTIFIER);
		Syntax::Identifier *field = std::static_pointer_cast<Syntax::Identifier>(fieldvalue->left).get();
		if (field->name != (*record_field).name) {
			Error::error(std::string("Wrong field name ") + field->name +
				" expected " + (*record_field).name,
				fieldvalue->left->linenumber);
		} else {
			Type *valueType;
			IR::Code value_code;
			translateExpression(fieldvalue->right, value_code, valueType, last_loop_exit,
				currentFrame, true);
			if (! CheckAssignmentTypes(field_type, valueType)) {
				Error::error("Incompatible type for this field value",
					fieldvalue->right->linenumber);
			} else {
				sequence->addStatement(std::make_shared<IR::MoveStatement>(
					std::make_shared<IR::MemoryExpression>(std::make_shared<IR::BinaryOpExpression>(IR::OP_PLUS,
						std::make_shared<IR::RegisterExpression>(record_address),
						std::make_shared<IR::IntegerExpression>((*record_field).offset)
					)), IRenvironment->codeToExpression(value_code)
				));
			}
		}
		record_field++;
	}
	translated = std::make_shared<IR::ExpressionCode>(std::make_shared<IR::StatExpSequence>(sequence,
		std::make_shared<IR::RegisterExpression>(record_address)));
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
	IR::Code &translated, Type*& type,
	IR::Label *last_loop_exit, IR::AbstractFrame *currentFrame,
	bool expect_comparison)
{
	translated = NULL;
	switch (expression->type) {
		case Syntax::INTVALUE:
			translateIntValue(std::static_pointer_cast<Syntax::IntValue>(expression)->value, translated);
			type = type_environment->getIntType();
			break;
		case Syntax::STRINGVALUE:
			translateStringValue(std::static_pointer_cast<Syntax::StringValue>(expression).get(), translated);
			type = type_environment->getStringType();
			break;
		case Syntax::IDENTIFIER:
			translateIdentifier(std::static_pointer_cast<Syntax::Identifier>(expression).get(),
				translated, type, currentFrame);
			break;
		case Syntax::NIL:
			type = type_environment->getNilType();
			translateIntValue(0, translated);
			break;
		case Syntax::BINARYOP: {
			if (! expect_comparison &&
					std::static_pointer_cast<Syntax::BinaryOp>(expression)->operation == SYM_EQUAL)
				Error::warning("Might have written comparison instead of assignment",
					expression->linenumber);
			translateBinaryOperation(std::static_pointer_cast<Syntax::BinaryOp>(expression).get(),
				translated, type, last_loop_exit, currentFrame);
			break;
		}
		case Syntax::SEQUENCE:
			translateSequence(std::static_pointer_cast<Syntax::Sequence>(expression)->content->expressions,
				translated, type, last_loop_exit, currentFrame);
			break;
		case Syntax::ARRAYINDEXING:
			translateArrayIndexing(std::static_pointer_cast<Syntax::ArrayIndexing>(expression).get(),
				translated, type, last_loop_exit, currentFrame);
			break;
		case Syntax::ARRAYINSTANTIATION:
			translateArrayInstantiation(std::static_pointer_cast<Syntax::ArrayInstantiation>(expression).get(),
				translated, type, last_loop_exit, currentFrame);
			break;
		case Syntax::IF:
			translateIf(std::static_pointer_cast<Syntax::If>(expression).get(),
				translated, type, last_loop_exit, currentFrame);
			break;
		case Syntax::IFELSE:
			translateIfElse(std::static_pointer_cast<Syntax::IfElse>(expression).get(),
				translated, type, last_loop_exit, currentFrame);
			break;
		case Syntax::WHILE:
			translateWhile(std::static_pointer_cast<Syntax::While>(expression).get(),
				translated, type, currentFrame);
			break;
		case Syntax::FOR:
			translateFor(std::static_pointer_cast<Syntax::For>(expression),
				translated, type, currentFrame);
			break;
		case Syntax::BREAK:
			type = type_environment->getVoidType();
			if (last_loop_exit == NULL)
				Error::error("Break outside of a loop", expression->linenumber);
			else
				translateBreak(translated, last_loop_exit);
			break;
		case Syntax::SCOPE:
			translateScope(std::static_pointer_cast<Syntax::Scope>(expression).get(),
				translated, type, last_loop_exit, currentFrame);
			break;
		case Syntax::RECORDFIELD:
			translateRecordField(std::static_pointer_cast<Syntax::RecordField>(expression).get(),
				translated, type, last_loop_exit, currentFrame);
			break;
		case Syntax::FUNCTIONCALL:
			translateFunctionCall(std::static_pointer_cast<Syntax::FunctionCall>(expression).get(),
				translated, type, last_loop_exit, currentFrame);
			break;
		case Syntax::RECORDINSTANTIATION:
			translateRecordInstantiation(std::static_pointer_cast<Syntax::RecordInstantiation>(expression).get(),
				translated, type, last_loop_exit, currentFrame);
			break;
		default:
			Error::fatalError(std::string("Unexpected ") + NODETYPENAMES[expression->type]);
	}
}

void TranslatorPrivate::prespillRegisters(IR::Code code, IR::AbstractFrame *frame)
{
	std::list<IR::VarLocation *> to_spill;
	for (IR::VarLocation *param: frame->getParameters())
		if (param->isRegister() &&
				param->getRegister()->isPrespilled())
			to_spill.push_back(param);
	for (IR::VarLocation *local: frame->getVariables())
		if (local->isRegister() &&
				local->getRegister()->isPrespilled())
			to_spill.push_back(local);
	IR::VarLocation *parentfp = frame->getParentFpForUs();
	if ((parentfp != NULL) && parentfp->isRegister() &&
			parentfp->getRegister()->isPrespilled())
		to_spill.push_back(parentfp);

	std::shared_ptr<IR::StatementSequence> sequence =
		std::make_shared<IR::StatementSequence>();
	std::map<int, IR::VarLocation *> spilled_locations;
	
	for (IR::VarLocation *var: to_spill) {
		assert (var->isRegister());
		IR::VirtualRegister *reg = var->getRegister();
		assert (reg->isPrespilled());
		if (! var->read_only)
			spilled_locations.insert(std::make_pair(
				reg->getIndex(), reg->getPrespilledLocation()));
		if (var->isPredefined())
			sequence->addStatement(std::make_shared<IR::MoveStatement>(
				reg->getPrespilledLocation()->createCode(frame),
				std::make_shared<IR::RegisterExpression>(reg)));
	}
	
	if (code->kind == IR::CODE_EXPRESSION) {
		std::shared_ptr<IR::ExpressionCode> exp_code =
			std::static_pointer_cast<IR::ExpressionCode>(code);
		if (! spilled_locations.empty())
			frame->prespillRegisters(exp_code->exp,	spilled_locations);
		if (! sequence->statements.empty())
			exp_code->exp = std::make_shared<IR::StatExpSequence>(
				sequence, exp_code->exp);
	} else {
		std::shared_ptr<IR::StatementCode> statm_code =
			std::static_pointer_cast<IR::StatementCode>(code);
		if (! spilled_locations.empty())
			frame->prespillRegisters(statm_code->statm, spilled_locations);
		
		if (! sequence->statements.empty()) {
			sequence->addStatement(statm_code->statm);
			statm_code->statm = sequence;
		}
	}
}

void TranslatorPrivate::addParentFpToCalls()
{
	for (auto frame_iter = frames_with_added_parentfps.begin();
			frame_iter != frames_with_added_parentfps.end();
			++frame_iter) {
		IR::AbstractFrame *callee_frame = *frame_iter;
		debug("Frame %s needs parent FP parameter, adding to all calls",
			callee_frame->getName().c_str());
		
		auto call_list = calls_to_frames.find(callee_frame->getId());
		if (call_list == calls_to_frames.end())
			continue;

		for (const CallRecord &call: call_list->second) {
			IR::AbstractFrame *calling_frame = call.calling_frame;
			if (callee_frame->getParentFpForUs() != NULL) {
				IR::Expression callee_parentfp;
				if (callee_frame->getParent()->getId() == calling_frame->getId())
					callee_parentfp = std::make_shared<IR::RegisterExpression>(
						calling_frame->getFramePointer());
				else if (callee_frame->getParent()->getId() ==
						calling_frame->getParent()->getId()) {
					if (calling_frame->getParentFpForUs() == NULL) {
						debug("Frame %s needs parent FP because it calls sibling %s who needs parent FP",
							calling_frame->getName().c_str(),
							callee_frame->getName().c_str());
						calling_frame->addParentFpParameter();
					}
					callee_parentfp = 
						calling_frame->getParentFpForUs()->createCode(calling_frame);
				} else {
					IR::AbstractFrame *parent_sibling_with_callee = calling_frame;
					while (parent_sibling_with_callee->getParent()->getId() !=
							callee_frame->getParent()->getId())
						parent_sibling_with_callee =
							parent_sibling_with_callee->getParent();
					
					callee_parentfp = parent_sibling_with_callee->
						getParentFpForChildren()->createCode(calling_frame);
				}
				call.call->callee_parentfp = callee_parentfp;
			}
		}
	}
}

Translator::Translator(IR::IREnvironment *ir_inv,
	IR::AbstractFrameManager * _framemanager)
{
	impl = new TranslatorPrivate(ir_inv, _framemanager);
}

Translator::~Translator()
{
	delete impl;
}

void Translator::translateProgram(Syntax::Tree expression,
	IR::Statement &result, IR::AbstractFrame *&frame)
{
	Type *type;
	IR::Code code;
	frame = impl->framemanager->newFrame(impl->framemanager->rootFrame(), ".global");
	impl->translateExpression(expression, code, type, NULL, frame, false);
	impl->addParentFpToCalls();
	for (Semantic::Function &fcn: impl->functions)
		if (fcn.body)
			impl->prespillRegisters(fcn.body, fcn.frame);
	impl->prespillRegisters(code, frame);
	result = impl->IRenvironment->codeToStatement(code);

	for (Semantic::Function &func: impl->functions)
		func.raw_body = nullptr;
}

void Translator::printFunctions(FILE *out)
{
	for (Function &func: impl->functions)
		if (func.body != NULL) {
			fprintf(out, "Function %s\n", func.label->getName().c_str());
			IR::PrintCode(out, func.body);
		}
}

void Translator::canonicalizeProgram(IR::Statement &statement)
{
	impl->IRtransformer->canonicalizeStatement(statement);
	if (statement->kind == IR::IR_STAT_SEQ)
		impl->IRtransformer->arrangeJumps(IR::ToStatementSequence(statement));
}

void Translator::canonicalizeFunctions()
{
	for (Function &func: impl->functions) {
		if (func.body == NULL)
			continue;
		switch (func.body->kind) {
			case IR::CODE_EXPRESSION: {
				IR::ExpressionCode *exp_code = std::static_pointer_cast<IR::ExpressionCode>(func.body).get();
				impl->IRtransformer->canonicalizeExpression(
					exp_code->exp, NULL, NULL);
				impl->IRtransformer->arrangeJumpsInExpression(exp_code->exp);
				break;
			}
			case IR::CODE_STATEMENT: {
				IR::StatementCode *statm_code = std::static_pointer_cast<IR::StatementCode>(func.body).get();
				canonicalizeProgram(statm_code->statm);
				break;
			}
			case IR::CODE_JUMP_WITH_PATCHES: {
				IR::Expression expr = impl->IRenvironment->
					codeToExpression(func.body);
				impl->IRtransformer->canonicalizeExpression(expr, NULL, NULL);
				impl->IRtransformer->arrangeJumpsInExpression(expr);
				func.body = std::make_shared<IR::ExpressionCode>(expr);
			}
		}
	}
}

const std::list< Function >& Translator::getFunctions()
{
	return impl->functions;
}

}
