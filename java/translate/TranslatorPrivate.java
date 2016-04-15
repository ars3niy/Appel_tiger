package translate;

import error.Error;
import frame.VarLocation;
import ir.ExpCode;
import ir.IREnvironment;
import ir.StatmCode;

import java.util.LinkedList;
import java.util.ListIterator;
import java.util.TreeSet;
import java.util.TreeMap;

class TypesEnvironment {
	private util.LayeredMap typenames = new util.LayeredMap();
	private LinkedList<NamedType> unknown_types = new LinkedList<>();
	private util.IdProvider id_provider = new util.IdProvider();
	private frame.AbstractFrameManager framemanager;
	
	private Type createArrayType(parse.ArrayTypeDefinition definition,
			boolean allow_forward_references) {
		return new ArrayType(id_provider.getId(),
				getType(definition.elemtype, allow_forward_references));
	}
	
	private Type createRecordType(parse.RecordTypeDefinition definition,
			boolean allow_forward_references) {
		RecordType type = new RecordType(id_provider.getId());
		for (parse.Node field: definition.fields.expressions) {
			assert field instanceof parse.ParameterDeclaration;
			parse.ParameterDeclaration param = ((parse.ParameterDeclaration)field);
			type.addField(param.name.name, getType(param.type,
					allow_forward_references));
		}
		return type;
	}
	
	public int getTypeSize(Type type) {
		if (type instanceof NamedType)
			return getTypeSize(type.resolve());
		
		if (type.kind == Type.INT)
			return framemanager.getIntSize();
		else
			return framemanager.getPointerSize();
	}
	
	public TypesEnvironment(frame.AbstractFrameManager _framemanager) {
		framemanager = _framemanager;
		typenames.add("int", intType());
		typenames.add("string", stringType());
	}
	
	public Type intType() {return Type.IntType;}
	public Type loopedIntType() {return Type.LoopedIntType;}
	public Type stringType() {return Type.StringType;}
	public Type errorType() {return Type.ErrorType;}
	public Type voidType() {return Type.VoidType;}
	public Type nilType() {return Type.NilType;}
	public Type pointerType() {return Type.StringType;}
	
	public Type getType(parse.Node definition, boolean allow_forward_references) {
		if (definition instanceof parse.ArrayTypeDefinition)
			return createArrayType((parse.ArrayTypeDefinition)definition,
					allow_forward_references);
		else if (definition instanceof parse.RecordTypeDefinition)
			return createRecordType((parse.RecordTypeDefinition)definition,
					allow_forward_references);
		else if (definition instanceof parse.Identifier) {
			String identifier = ((parse.Identifier) definition).name;
			Type type = (Type)typenames.lookup(identifier);
			if (type == null) {
				if (! allow_forward_references) {
					Error.current.error("Undefined type " + identifier,
							definition.position);
					return errorType();
				}
				type = new NamedType(identifier, definition);
				unknown_types.add((NamedType)type);
				typenames.add(identifier, type);
			}
			return type;
		} else {
			Error.current.fatalError("TypesEnvironment.getType got not a type definition",
					definition.position);
			return errorType();
		}
	}
	
	public void newLayer() {typenames.newLayer();}
	public void removeLastLayer() {typenames.removeLastLayer();}
	
	public void processTypesDeclarationBatch(ListIterator<parse.Node> first,
			ListIterator<parse.Node> afterlast) {
		unknown_types.clear();
		LinkedList<RecordType> new_records = new LinkedList<>();
		TreeSet<String> names_in_the_batch = new TreeSet<>();
		
		for (ListIterator<parse.Node> i = first; i.nextIndex() < afterlast.nextIndex(); ) {
			parse.Node d = i.next();
			assert d instanceof parse.TypeDeclaration;
			parse.TypeDeclaration declaration = (parse.TypeDeclaration)d;
			
			if (names_in_the_batch.contains(declaration.name.name))
				Error.current.error("Type " + declaration.name.name +
					" is redefined in a same batch of consequtive types",
					declaration.name.position);
			names_in_the_batch.add(declaration.name.name);
			
			Type type = getType(declaration.definition, true);
			assert type != null;
			if (type instanceof RecordType)
				new_records.add((RecordType)type);
			
			Type existing = (Type)typenames.lookupLastLayer(declaration.name.name);
			if (existing != null) {
				if ((existing instanceof NamedType) &&
						(((NamedType)existing).meaning == null)) {
					((NamedType)existing).meaning = type;
				}
			} else
				typenames.add(declaration.name.name, type);
		}

		for (NamedType t: unknown_types) {
			if (t.meaning == null) {
				Error.current.error("Type not defined in this scope: " + t.name,
							 t.declaration.position);
				t.meaning = errorType();
			}
		}

		for (NamedType t: unknown_types)
			t.meaning = t.resolve();

		for (RecordType record: new_records) {
			record.size = 0;
			for (RecordField field: record.fields) {
				field.offset = record.size;
				record.size = framemanager.updateRecordSize(record.size,
					getTypeSize(field.type));
			}
		}
	}
	
	
}

class TranslatorPrivate extends util.DebugPrinter implements frame.ParentFpHandler {
	private util.LayeredMap func_and_var_names = new util.LayeredMap();
	private TypesEnvironment types;
	private TreeMap<String, ir.Blob> blobs_by_string = new TreeMap<>();
	private ir.IREnvironment IRenv;
	private ir.Canonicalizer canon;
	private frame.AbstractFrameManager framemanager;
	private LinkedList<Function> functions = new LinkedList<>();
	
	private Function getmem_func, getmem_fill_func, strcmp_func;
	
	private void newLayer() {
		func_and_var_names.newLayer();
	}
	
	private void removeLastLayer() {
		func_and_var_names.removeLastLayer();
	}
	
	private static boolean IsInt(Type type)
	{
		return (type.kind == Type.INT) || (type.kind == Type.ERROR);
	}

	private static boolean IsArray(Type type)
	{
		return (type instanceof ArrayType) || (type.kind == Type.ERROR);
	}

	private boolean CheckAssignmentTypes(Type left, Type right)
	{
		if (right.kind == Type.ERROR)
			return true;
		if ((left.kind == Type.INT) || (left.kind == Type.STRING))
			return right.kind == left.kind;
		else if (left instanceof ArrayType)
				return (right instanceof ArrayType) &&
					((ArrayType)left).id == ((ArrayType)right).id;
		else if (left instanceof RecordType)
				return (right.kind == Type.NIL) ||
					(right instanceof RecordType) &&
					((RecordType )left).id == ((RecordType)right).id;
		else if (left.kind == Type.ERROR)
				return true;
		else {
			Error.current.fatalError("Inexpected type for type checking");
			return true;
		}
	}

	private void processVariableDeclaration(parse.VariableDeclaration declaration,
			frame.AbstractFrame current_frame, LinkedList<Variable> new_vars) {
		Type vartype;
		if (declaration.type != null)
			vartype = types.getType(declaration.type, false).resolve();
		else
			vartype = null;
		
		Result value = translateExpression(declaration.value, null,
			current_frame, true);
		if (value.type.kind == Type.VOID) {
			Error.current.error("Statement without value assigned to a variable",
				declaration.value.position);
			value.type = types.errorType();
		}
		if (vartype == null) {
			if (value.type.kind == Type.NIL) {
				Error.current.error("Variable without explicit type cannot be initializod to nil",
					declaration.value.position);
				value.type = types.errorType();
			}
			vartype = value.type;
		} else
			if (! CheckAssignmentTypes(vartype, value.type))
				Error.current.error("Incompatible type for variable initialization",
					declaration.value.position);
		Variable newvar = new Variable(declaration.name.name, vartype,
			value.code,	current_frame.addVariable(declaration.name.name,
			types.getTypeSize(vartype)));
		func_and_var_names.add(declaration.name.name, newvar);
		new_vars.add(newvar);
	}
	
	private void processFunctionDeclarationBatch(ListIterator<parse.Node> begin,
			ListIterator<parse.Node> end, frame.AbstractFrame current_frame) {
		LinkedList<Function> recent_functions = new LinkedList<>();
		TreeSet<String> names_in_batch = new TreeSet<>();
		for (ListIterator<parse.Node> f = begin; f.nextIndex() < end.nextIndex(); ) {
			parse.Function declaration = (parse.Function)f.next();
			if (names_in_batch.contains(declaration.name.name))
				Error.current.error("Function " + declaration.name.name +
					"redefined in a successive batch of functions", declaration.name.position);
			names_in_batch.add(declaration.name.name);
			Type return_type;
			if (declaration.type == null)
				return_type = types.voidType();
			else
				return_type = types.getType(declaration.type, false).resolve();
			ir.Label function_label;
			if (declaration.body != null) {
				function_label = IRenv.addLabel();
				function_label.appendToName("_" + declaration.name.name);
			} else
				function_label = IRenv.addLabel(declaration.name.name);
			Function function = new Function(declaration.name.name, return_type,
				declaration.body, null,
				framemanager.newFrame(current_frame, function_label.getName()),
				function_label);
			functions.add(function);
			debug(String.format("New frame %s, id %d", function.implementation.frame.getName(),
				function.implementation.frame.getId()));
			func_and_var_names.add(declaration.name.name, function);
			recent_functions.add(function);
			
			for (parse.Node param: declaration.parameters.expressions) {
				assert param instanceof parse.ParameterDeclaration;
				parse.ParameterDeclaration param_decl = 
					(parse.ParameterDeclaration)param;
				Type param_type = types.getType(param_decl.type, false).resolve();
				function.addArgument(param_decl.name.name, param_type,
					function.implementation.frame.addParameter(
						param_decl.name.name,
						types.getTypeSize(param_type))
				);
			}
		}
		
		for (Function fcn: recent_functions) {
			if (fcn.raw_body == null)
				continue;
			
			func_and_var_names.newLayer();
			for (FuncArgument param: fcn.arguments)
				func_and_var_names.add(param.name, param);
			Result translated_body = 
				translateExpression(fcn.raw_body, null, fcn.implementation.frame, true);
			
			func_and_var_names.removeLastLayer();
			
			if ((fcn.return_type.kind != Type.VOID) &&
					! CheckAssignmentTypes(fcn.return_type, translated_body.type))
				Error.current.error("Type of function body doesn't match specified return type",
					fcn.raw_body.position);
			else if ((fcn.return_type.kind == Type.VOID) &&
					(translated_body.type.kind != Type.VOID) &&
					(translated_body.type.kind != Type.ERROR))
				Error.current.error("Body of a function without return value produces a value",
					fcn.raw_body.position);
			fcn.implementation.body = translated_body.code;
		}
		
	}
	
	private void processDeclarations(parse.ExpressionList declarations,
			frame.AbstractFrame current_frame, LinkedList<Variable> new_vars) {
		ListIterator<parse.Node> d = declarations.expressions.listIterator();
		ListIterator<parse.Node> java_fucking_sucks = declarations.expressions.listIterator();
		while (d.hasNext()) {
			parse.Node declaration = d.next();
			if (declaration instanceof parse.VariableDeclaration) {
				debug(String.format("Processing variable at %d of %d",
						d.nextIndex()-1,
						declarations.expressions.size()));
				processVariableDeclaration((parse.VariableDeclaration)declaration,
					current_frame, new_vars);
			} else if (declaration instanceof parse.Function || 
				       (declaration instanceof parse.TypeDeclaration)) {
				boolean is_func = declaration instanceof parse.Function;
				while (d.hasNext()) {
					parse.Node fuck = d.next();
					if ((is_func && !(fuck instanceof parse.Function)) ||
						(!is_func && !(fuck instanceof parse.TypeDeclaration))) {
						d.previous();
						break;
					}
				}
				if (is_func) {
					debug(String.format("Processing fuctions at %d-%d of %d",
						java_fucking_sucks.nextIndex(), d.nextIndex()-1,
						declarations.expressions.size()));
					processFunctionDeclarationBatch(java_fucking_sucks, d, current_frame);
				} else {
					debug(String.format("Processing types at %d-%d of %d",
							java_fucking_sucks.nextIndex(), d.nextIndex()-1,
							declarations.expressions.size()));
					types.processTypesDeclarationBatch(java_fucking_sucks, d);
				}
			} else
				Error.current.fatalError("Not a declaration inside declaration block",
					declaration.position);
			while (java_fucking_sucks.nextIndex() < d.nextIndex())
				java_fucking_sucks.next();
		}
	}
	
	private ir.Code translateIntValue(int value) {
		return new ir.ExpCode(new ir.IntegerExp(value));
	}
	
	private ir.Code translateStringValue(String value) {
		ir.Blob blob = blobs_by_string.get(value);
		if (blob == null) {
			blob = IRenv.addBlob();
			int intsize = framemanager.getIntSize();
			
			byte[] source = value.getBytes();
			int len = source.length;
			blob.data = new byte[len + intsize];
			framemanager.placeInt(len, blob.data);
			
			System.arraycopy(source, 0, blob.data, intsize, len);
			blobs_by_string.put(value, blob);
		}
		return new ir.ExpCode(new ir.LabelAddressExp(blob.label));
	}
	
	private static ir.Code ErrorPlaceholderCode() {
		return new ir.ExpCode(null);
	}
	
	private Result translateIdentifier(parse.Identifier expression,
			frame.AbstractFrame current_frame) {
		Result result = new Result();
		Declaration var_or_function = (Declaration)func_and_var_names.lookup(
			expression.name);
		if (var_or_function == null) {
			Error.current.error("Undeclared variable/function identifier", expression.position);
			result.type = types.errorType();
			result.code = ErrorPlaceholderCode();
		} else if (var_or_function instanceof Function) {
			Error.current.error("Function used as a variable", expression.position);
			result.type = types.errorType();
			result.code = ErrorPlaceholderCode();
		} else {
			result.type = ((Variable)var_or_function).type.resolve();
			result.code = new ir.ExpCode(
				((Variable)var_or_function).implementation.createCode(current_frame)
			);
		}
		return result;
	}
	
	private static boolean CheckComparisonTypes(Type left, Type right)
	{
		if ((left.kind == Type.ERROR) || (right.kind == Type.ERROR))
			return true;
		if ((left.kind == Type.INT) || (left.kind == Type.STRING))
			return right.kind == left.kind;
		else if (left instanceof ArrayType)
			return (right instanceof ArrayType) &&
				(((ArrayType)left).id == ((ArrayType)right).id);
		else if (left instanceof RecordType)
			return (right.kind == Type.NIL) ||
				(right instanceof RecordType) &&
				(((RecordType)left).id == ((RecordType)right).id);
		else if (left.kind == Type.NIL)
			return right instanceof RecordType;
		else {
			Error.current.fatalError("Should have handled this error in a better way");
			return true;
		}
	}

	public Type getOpResultType(Type leftType, Type rightType,
			parse.BinaryOp expression) {
		switch (expression.operation) {
		case parse.Parser.SYM_PLUS:
		case parse.Parser.SYM_MINUS:
		case parse.Parser.SYM_ASTERISK:
		case parse.Parser.SYM_SLASH:
		case parse.Parser.SYM_AND:
		case parse.Parser.SYM_OR:
			if (! IsInt(leftType))
				Error.current.error("Operand for arithmetic is not integer",
							 expression.left.position);
			if (! IsInt(rightType))
				Error.current.error("Operand for arithmetic is not integer",
							 expression.right.position);
			return types.intType();
		case parse.Parser.SYM_EQUAL:
		case parse.Parser.SYM_NONEQUAL:
			if (! CheckComparisonTypes(leftType, rightType))
				Error.current.error("Incompatible types for equality comparison",
							 expression.position);
			return types.intType();
		case parse.Parser.SYM_LESS:
		case parse.Parser.SYM_LESSEQUAL:
		case parse.Parser.SYM_GREATER:
		case parse.Parser.SYM_GREATEQUAL:
			if ((leftType.kind != Type.INT) && (leftType.kind != Type.STRING))
				Error.current.error("Operand for inequality is not integer or string",
					 expression.left.position);
			if ((rightType.kind != Type.INT) && (rightType.kind != Type.STRING))
				Error.current.error("Operand for inequality is not integer or string",
					 expression.right.position);
			if (leftType.kind != rightType.kind)
				Error.current.error("Incompatible types for comparison",
					 expression.right.position);
			return types.intType();
		case parse.Parser.SYM_ASSIGN:
			if (! CheckAssignmentTypes(leftType, rightType))
				Error.current.error("Incompatible types for assignment",
					 expression.position);
			if (leftType.is_loop_var)
				Error.current.error("Assignment to for loop variable",
					 expression.left.position);
			return types.voidType();
		default:
			Error.current.fatalError("I forgot to handle this operation",
				expression.left.position);
			return null;
		}
	}
	
	private static int getIRBinaryOp(int op_token)
	{
		switch (op_token) {
			case parse.Parser.SYM_PLUS:
				return ir.BinaryOpExp.PLUS;
			case parse.Parser.SYM_MINUS:
				return ir.BinaryOpExp.MINUS;
			case parse.Parser.SYM_ASTERISK:
				return ir.BinaryOpExp.MUL;
			case parse.Parser.SYM_SLASH:
				return ir.BinaryOpExp.DIV;
			default:
				return 0;
		}
	}

	private static int getIRComparisonOp(int op_token)
	{
		switch (op_token) {
			case parse.Parser.SYM_EQUAL:
				return ir.CondJumpStatm.EQUAL;
			case parse.Parser.SYM_NONEQUAL:
				return ir.CondJumpStatm.NONEQUAL;
			case parse.Parser.SYM_LESS:
				return ir.CondJumpStatm.LESS;
			case parse.Parser.SYM_LESSEQUAL:
				return ir.CondJumpStatm.LESSEQUAL;
			case parse.Parser.SYM_GREATER:
				return ir.CondJumpStatm.GREATER;
			case parse.Parser.SYM_GREATEQUAL:
				return ir.CondJumpStatm.GREATEQUAL;
			default:
				return 0;
		}
	}

	private Result translateBinaryOperation(parse.BinaryOp expression,
			ir.Label last_loop_exit, frame.AbstractFrame current_frame) {
		Result left = translateExpression(expression.left, last_loop_exit, current_frame, true);
		Result right = translateExpression(expression.right, last_loop_exit, current_frame, true);
		Result result = new Result();
		
		boolean bullshit = false;
		if (left.type.kind == Type.VOID) {
			Error.current.error("Statement without value used in expression",
				expression.left.position);
			bullshit = true;
		}
		if (right.type.kind == Type.VOID) {
			Error.current.error("Statement without value used in expression",
						 expression.right.position);
			bullshit = true;
		}
		if (bullshit || (left.type.kind == Type.ERROR) || (right.type.kind == Type.ERROR)) {
			result.type = types.errorType();
			result.code = ErrorPlaceholderCode();
			return result;
		}
		result.type = getOpResultType(left.type, right.type, expression);
		switch (expression.operation) {
			case parse.Parser.SYM_PLUS:
			case parse.Parser.SYM_MINUS:
			case parse.Parser.SYM_ASTERISK:
			case parse.Parser.SYM_SLASH: {
				ir.Expression left_expr = IRenv.codeToExpression(left.code);
				ir.Expression right_expr = IRenv.codeToExpression(right.code);
				result.code = new ir.ExpCode(new ir.BinaryOpExp(
					getIRBinaryOp(expression.operation), left_expr, right_expr));
				break;
			}
			case parse.Parser.SYM_EQUAL:
			case parse.Parser.SYM_NONEQUAL:
			case parse.Parser.SYM_LESS:
			case parse.Parser.SYM_LESSEQUAL:
			case parse.Parser.SYM_GREATER:
			case parse.Parser.SYM_GREATEQUAL: {
				ir.CondJumpStatm compare_statm;
				ir.Expression left_expr, right_expr;
				if (left.type.kind == Type.STRING) {
					LinkedList<ir.Code> args = new LinkedList<>();
					args.add(left.code);
					args.add(right.code);
					ir.Code strcmp_call = makeCallCode(strcmp_func, args, current_frame);
					left_expr = IRenv.codeToExpression(strcmp_call);
					right_expr = new ir.IntegerExp(0);
				} else {
					left_expr = IRenv.codeToExpression(left.code);
					right_expr = IRenv.codeToExpression(right.code);
				}
				compare_statm = new ir.CondJumpStatm(
					getIRComparisonOp(expression.operation), left_expr, right_expr,
					null, null);
				ir.CondJumpPatchesCode code =
					new ir.CondJumpPatchesCode(compare_statm);
				code.replace_true.add(compare_statm.trueDestRef());
				code.replace_false.add(compare_statm.falseDestRef());
				result.code = code;
				break;
			}
			case parse.Parser.SYM_ASSIGN: {
				ir.Expression left_expr = IRenv.codeToExpression(left.code);
				ir.Expression right_expr = IRenv.codeToExpression(right.code);
				result.code = new ir.StatmCode(
					new ir.MoveStatm(left_expr, right_expr));
				break;
			}
			default:
				Error.current.fatalError("I forgot to handle this operation",
					expression.left.position);
		}
		return result;
	}
	
	private Result translateSequence(LinkedList<parse.Node> expressions,
			ir.Label last_loop_exit, frame.AbstractFrame current_frame,
			ir.StatmSequence existing_sequence) {
		Result result = new Result();
		result.type = types.voidType();
		ir.StatmSequence sequence = existing_sequence;
		if (sequence == null)
			sequence = new ir.StatmSequence();
		ir.Expression last_expression = null;
		for (ListIterator<parse.Node> e = expressions.listIterator();
				e.hasNext(); ) {
			parse.Node elem = e.next();
			Result partial = translateExpression(elem, last_loop_exit, current_frame, false);
			result.type = partial.type;
			if (e.hasNext() || (partial.type.kind == Type.VOID))
				sequence.addStatement(IRenv.codeToStatement(partial.code));
			else
				last_expression = IRenv.codeToExpression(partial.code);
		}
		if (last_expression == null)
			result.code = new ir.StatmCode(sequence);
		else
			if (! sequence.statements.isEmpty())
				result.code = new ir.ExpCode(new ir.StatExpSequence(
					sequence, last_expression));
			else
				result.code = new ir.ExpCode(last_expression);
		return result;
	}
	
	private Result translateArrayIndexing(parse.ArrayIndexing expression,
			ir.Label last_loop_exit, frame.AbstractFrame current_frame) {
		Result result = new Result();
		Result array = translateExpression(expression.array, last_loop_exit, current_frame, true);
		if (! IsArray(array.type))
			Error.current.error("Not an array got indexed", expression.array.position);
		Result index = translateExpression(expression.index, last_loop_exit, current_frame, true);
		if (! IsInt(index.type))
			Error.current.error("Index is not an integer", expression.index.position);
		if (array.type instanceof ArrayType) {
			result.type = ((ArrayType)array.type).elemtype.resolve();
			ir.BinaryOpExp offset = new ir.BinaryOpExp(
				ir.BinaryOpExp.MUL, IRenv.codeToExpression(index.code),
				new ir.IntegerExp(types.getTypeSize(result.type)));
			ir.BinaryOpExp target_address = new ir.BinaryOpExp(
				ir.BinaryOpExp.PLUS, IRenv.codeToExpression(array.code), offset);
			result.code = new ir.ExpCode(
				new ir.MemoryExp(target_address));
		} else {
			result.type = types.errorType();
			result.code = ErrorPlaceholderCode();
		}
		return result;
	}
	
	private Result translateArrayInstantiation(parse.ArrayInstantiation expression,
			ir.Label last_loop_exit, frame.AbstractFrame current_frame) {
		assert expression.arrayIsSingleId();
		Result result = new Result();
		result.type = types.getType(expression.arraydef.array, false).resolve();
		Type elem_type = null;
		if (result.type == null) {
			Error.current.error("Undefined type identifier in array instantiation",
				expression.arraydef.array.position);
			result.type = types.errorType();
		} else {
			if (! (result.type instanceof ArrayType)) {
				if (result.type.kind != Type.ERROR)
					Error.current.error("Not an array result.type in array instantiation",
						expression.arraydef.array.position);
				result.type = types.errorType();
			} else {
				elem_type = ((ArrayType)result.type).elemtype.resolve();
			}
		}
		Result length = translateExpression(expression.arraydef.index,
			last_loop_exit,	current_frame, true);
		if (! IsInt(length.type))
			Error.current.error("Not an integer as array length", expression.arraydef.index.position);

		Result value = translateExpression(expression.value, 
			last_loop_exit, current_frame, true);
		if ((elem_type != null) && ! CheckAssignmentTypes(elem_type, value.type))
			Error.current.error("Value result.type doesn't match array element type",
				expression.value.position);
		
		if (elem_type != null) {
			LinkedList<ir.Code> args = new LinkedList<>();
			args.add(length.code);
			args.add(value.code);
			result.code = makeCallCode(getmem_fill_func, args, current_frame);
		} else
			result.code = ErrorPlaceholderCode();
		
		return result;
	}
	
	private Result translateIf(parse.If expression, ir.Label last_loop_exit,
			frame.AbstractFrame current_frame) {
		Result result = new Result();
		result.type = types.voidType();
		
		Result condition = translateExpression(expression.condition,
			last_loop_exit,	current_frame, true);
		if (! IsInt(condition.type))
			Error.current.error("If condition not integer", expression.condition.position);
		Result action = translateExpression(expression.action,
			last_loop_exit,	current_frame, false);
		if ((action.type.kind != Type.VOID) && (action.type.kind != Type.ERROR))
			Error.current.error("Body of if-then statement is not valueless", expression.action.position);
		
		if (condition.type.kind == Type.INT) {
			IREnvironment.CondJump jump_by_condition = IRenv.codeToCondJump(condition.code);
			ir.Label true_label = IRenv.addLabel();
			ir.Label false_label = IRenv.addLabel();
			ir.CondJumpPatchesCode.putLabels(jump_by_condition.replace_true,
				jump_by_condition.replace_false, true_label, false_label);
			ir.StatmSequence statement = new ir.StatmSequence();
			statement.addStatement(jump_by_condition.statm);
			statement.addStatement(new ir.LabelPlacementStatm(true_label));
			statement.addStatement(IRenv.codeToStatement(action.code));
			statement.addStatement(new ir.LabelPlacementStatm(false_label));
			result.code = new ir.StatmCode(statement);
		} else
			result.code = ErrorPlaceholderCode();
		return result;
	}
	
	private Result translateIfElse(parse.IfElse expression,
			ir.Label last_loop_exit, frame.AbstractFrame current_frame) {
		Result result = new Result();
		Result condition = translateExpression(expression.condition,
			last_loop_exit, current_frame, true);
		if (! IsInt(condition.type))
			Error.current.error("If condition not integer", expression.condition.position);
		Result action = translateExpression(expression.action, 
			last_loop_exit, current_frame, expression.converted_from_logic);
		Result elseaction = translateExpression(expression.elseaction,
			last_loop_exit, current_frame, expression.converted_from_logic);
		if (((action.type.kind == Type.VOID) && (elseaction.type.kind == Type.VOID)) ||
				CheckComparisonTypes(action.type, elseaction.type)) {
			if (condition.type.kind == Type.ERROR)
				action.type = types.errorType();
			else if (action.type.kind == Type.NIL)
				result.type = elseaction.type;
			else
				result.type = action.type;
			if (condition.type.kind == Type.INT) {
				ir.Label true_label = IRenv.addLabel();
				ir.Label false_label = IRenv.addLabel();
				ir.Label finish_label = IRenv.addLabel();
				ir.VirtualRegister value_storage = null;
				if (result.type.kind != Type.VOID)
					value_storage = IRenv.addRegister();
				IREnvironment.CondJump condition_jump = IRenv.codeToCondJump(
					condition.code);
				ir.CondJumpPatchesCode.putLabels(condition_jump.replace_true,
					condition_jump.replace_false, true_label, false_label);
				ir.StatmSequence calculation = new ir.StatmSequence();
				calculation.addStatement(condition_jump.statm);
				calculation.addStatement(new ir.LabelPlacementStatm(true_label));
				if (result.type.kind != Type.VOID) {
					calculation.addStatement(new ir.MoveStatm(
						new ir.RegisterExp(value_storage),
						IRenv.codeToExpression(action.code)));
				} else {
					calculation.addStatement(IRenv.codeToStatement(action.code));
				}
				calculation.addStatement(new ir.JumpStatm(finish_label));
				calculation.addStatement(new ir.LabelPlacementStatm(false_label));
				if (result.type.kind != Type.VOID) {
					calculation.addStatement(new ir.MoveStatm(
						new ir.RegisterExp(value_storage),
						IRenv.codeToExpression(elseaction.code)));
				} else {
					calculation.addStatement(IRenv.codeToStatement(elseaction.code));
				}
				calculation.addStatement(new ir.LabelPlacementStatm(finish_label));
				if (result.type.kind == Type.VOID) {
					result.code = new ir.StatmCode(calculation);
				} else {
					result.code = new ir.ExpCode(new ir.StatExpSequence(
						calculation, new ir.RegisterExp(value_storage)));
				}
			} else
				result.code = ErrorPlaceholderCode();
		} else {
			Error.current.error("Incompatible or improper types for then and else", expression.position);
			result.type = types.errorType();
			result.code = ErrorPlaceholderCode();
		}
		return result;
	}
	
	private Result translateWhile(parse.While expression,
			frame.AbstractFrame current_frame) {
		Result result = new Result();
		result.type = types.voidType();
		ir.Label exit_label = IRenv.addLabel();
		Result condition = translateExpression(expression.condition, 
			exit_label, current_frame, true);
		if (! IsInt(condition.type))
			Error.current.error("While condition not integer", expression.condition.position);
		Result action = translateExpression(expression.action, exit_label,
			current_frame, false);
		if ((action.type.kind != Type.VOID) && (action.type.kind != Type.ERROR))
			Error.current.error("Body of while loop is not valueless", expression.action.position);
		if (condition.type.kind == Type.INT) {
			ir.Label loop_label = IRenv.addLabel();
			ir.Label proceed_label = IRenv.addLabel();
			IREnvironment.CondJump cond_jump = IRenv.codeToCondJump(condition.code);
			ir.CondJumpPatchesCode.putLabels(cond_jump.replace_true,
				cond_jump.replace_false, proceed_label, exit_label);
			if (cond_jump.statm instanceof ir.CondJumpStatm)
				((ir.CondJumpStatm)cond_jump.statm).flip();
			ir.StatmSequence sequence = new ir.StatmSequence();
			sequence.addStatement(new ir.LabelPlacementStatm(loop_label));
			sequence.addStatement(cond_jump.statm);
			sequence.addStatement(new ir.LabelPlacementStatm(proceed_label));
			sequence.addStatement(IRenv.codeToStatement(action.code));
			sequence.addStatement(new ir.JumpStatm(loop_label));
			sequence.addStatement(new ir.LabelPlacementStatm(exit_label));
			result.code = new ir.StatmCode(sequence);
		} else
			result.code = ErrorPlaceholderCode();
		return result;
	}
	
	private Result translateFor(parse.For expression, frame.AbstractFrame current_frame) {
		Result result = new Result();
		result.type = types.voidType();
		ir.Label exit_label = IRenv.addLabel();
		Result from = translateExpression(expression.start, exit_label,
			current_frame, false);
		if (! IsInt(from.type))
			Error.current.error("For loop start not integer", expression.start.position);
		Result to = translateExpression(expression.stop, exit_label,
			current_frame, false);
		if (! IsInt(to.type))
			Error.current.error("For loop 'to' not integer", expression.stop.position);
		func_and_var_names.newLayer();
//	 	printf("Loop variable %s at line %d is %s by address\n",
//	 		expression.variable.name.c_str(),
//	 		expression.position,
//	 		variables_extra_info.isAccessedByAddress(expression) ? "accessed" : "not accessed");
		Variable loopvar = new Variable(
			expression.variable.name,
			types.loopedIntType(), null, current_frame.addVariable(
				expression.variable.name,
				types.getTypeSize(from.type)
			));
		func_and_var_names.add(expression.variable.name, loopvar);
		Result action = translateExpression(expression.action, exit_label,
			current_frame, false);
		if ((from.type.kind == Type.INT) && (to.type.kind == Type.INT)) {
			ir.StatmSequence sequence = new ir.StatmSequence();
			ir.VirtualRegister upper_bound = IRenv.addRegister();
			sequence.addStatement(new ir.MoveStatm(
				new ir.RegisterExp(upper_bound),
				IRenv.codeToExpression(to.code)));
			ir.Expression loopvar_expression = loopvar.implementation.createCode(current_frame);
			sequence.addStatement(new ir.MoveStatm(
				loopvar_expression,
				IRenv.codeToExpression(from.code)));
			ir.Label loop_label = IRenv.addLabel();
			loopvar_expression = loopvar.implementation.createCode(current_frame);
			sequence.addStatement(new ir.CondJumpStatm(ir.CondJumpStatm.LESSEQUAL,
				loopvar_expression,
				new ir.RegisterExp(upper_bound),
				loop_label, exit_label));
			sequence.addStatement(new ir.LabelPlacementStatm(loop_label));
			sequence.addStatement(IRenv.codeToStatement(action.code));
			ir.Label proceed_label = IRenv.addLabel();
			loopvar_expression = loopvar.implementation.createCode(current_frame);
			sequence.addStatement(new ir.CondJumpStatm(ir.CondJumpStatm.LESS,
				loopvar_expression,
				new ir.RegisterExp(upper_bound),
				proceed_label, exit_label));
			sequence.addStatement(new ir.LabelPlacementStatm(proceed_label));
			loopvar_expression = loopvar.implementation.createCode(current_frame);
			ir.Expression loopvar_expression2 = loopvar.implementation.createCode(current_frame);
			sequence.addStatement(new ir.MoveStatm(
				loopvar_expression,
				new ir.BinaryOpExp(ir.BinaryOpExp.PLUS,
					loopvar_expression2,
					new ir.IntegerExp(1)
				)
			));
			sequence.addStatement(new ir.JumpStatm(loop_label));
			sequence.addStatement(new ir.LabelPlacementStatm(exit_label));
			result.code = new ir.StatmCode(sequence);
		} else
			result.code = ErrorPlaceholderCode();
		func_and_var_names.removeLastLayer();
		return result;
	}
	
	private ir.Code translateBreak(ir.Label loop_exit) {
		return new ir.StatmCode(new ir.JumpStatm(loop_exit));
	}
	
	private Result translateScope(parse.Scope expression, ir.Label last_loop_exit,
			frame.AbstractFrame current_frame) {
		newLayer();
		LinkedList<Variable> new_vars = new LinkedList<>();
		processDeclarations(expression.declarations, current_frame, new_vars);
		ir.StatmSequence sequence = new ir.StatmSequence();
		for (Variable newvar: new_vars)
			if (newvar.type.kind != Type.ERROR) {
				ir.Code var_code = new ir.ExpCode(
					newvar.implementation.createCode(current_frame));
				sequence.addStatement(new ir.MoveStatm(
					IRenv.codeToExpression(var_code),
					IRenv.codeToExpression(newvar.value)
				));
			}
		Result result = translateSequence(expression.actions.expressions,
			null, current_frame, sequence);
		removeLastLayer();
		return result;
	}
	
	private Result translateRecordField(parse.RecordField expression,
			ir.Label last_loop_exit, frame.AbstractFrame current_frame) {
		Result result = new Result();
		Result rec = translateExpression(expression.record, last_loop_exit,
			current_frame, true);
		if (! (rec.type instanceof RecordType)) {
			if (rec.type.kind != Type.ERROR)
				Error.current.error("What is being got field of is not a record",
					expression.record.position);
			result.type = types.errorType();
			result.code = ErrorPlaceholderCode();
		} else {
			RecordType record = (RecordType)rec.type;
			RecordField field = record.findField(expression.field.name);
			if (field == null) {
				Error.current.error("This field is not in that record",
					expression.field.position);
				result.type = types.errorType();
				result.code = ErrorPlaceholderCode();
			} else {
				result.type = field.type.resolve();
				ir.BinaryOpExp target_address =
					new ir.BinaryOpExp(
						ir.BinaryOpExp.PLUS, IRenv.codeToExpression(rec.code),
						new ir.IntegerExp(field.offset));
				result.code = new ir.ExpCode(
					new ir.MemoryExp(target_address));
			}
		}
		return result;
	}
	
	private class CallRecord {
		public ir.CallExpression call;
		public frame.AbstractFrame calling_frame;
		
		public CallRecord(ir.CallExpression _all, frame.AbstractFrame frame) {
			call = _all;
			calling_frame = frame;
		}
	}
	
	private class FrameList {
		public frame.AbstractFrame frame;
		public FrameList next;
	}
	
	private FrameList first_frame_with_added_parentfps = null;
	private FrameList last_frame_with_added_parentfps = null;
	private TreeMap<Integer, LinkedList<CallRecord>> calls_to_frames = new TreeMap<>();
	
	public void notifyFrameWithParentFp(frame.AbstractFrame frame) {
		FrameList node = new FrameList();
		node.next = null;
		node.frame = frame;
		if (last_frame_with_added_parentfps == null) {
			first_frame_with_added_parentfps = node;
			last_frame_with_added_parentfps = node;
		} else {
			last_frame_with_added_parentfps.next = node;
			last_frame_with_added_parentfps = node;
		}
	}

	private ir.Code makeCallCode(Function function, LinkedList<ir.Code>  arguments,
			frame.AbstractFrame current_frame) {
		ir.CallExpression call = new ir.CallExpression(
			new ir.LabelAddressExp(function.implementation.label),
			null);
		LinkedList<CallRecord> call_list =
			calls_to_frames.get(function.implementation.frame.getId());
		if (call_list == null) {
			call_list = new LinkedList<>();
			calls_to_frames.put(function.implementation.frame.getId(), call_list);
		}
		call_list.add(new CallRecord(call, current_frame));
		for (ir.Code arg: arguments)
			 call.addArgument(IRenv.codeToExpression(arg));
		return new ir.ExpCode(call);
	}
	
	private void addParentFpToCalls() {
		for (FrameList frame_iter =	first_frame_with_added_parentfps;
				frame_iter != null; frame_iter = frame_iter.next) {
			frame.AbstractFrame callee_frame = frame_iter.frame;
			debug(String.format("Frame %s needs parent FP parameter, adding to all calls",
				callee_frame.getName()));
			
			LinkedList<CallRecord> call_list =
				calls_to_frames.get(callee_frame.getId());
			if (call_list == null)
				continue;

			for (CallRecord call: call_list) {
				frame.AbstractFrame calling_frame = call.calling_frame;
				if (callee_frame.getParentFpForUs() != null) {
					ir.Expression callee_parentfp;
					if (callee_frame.getParent().getId() == calling_frame.getId())
						callee_parentfp = calling_frame.getFramePointer().createCode(calling_frame);
					else if (callee_frame.getParent().getId() ==
							calling_frame.getParent().getId()) {
						if (calling_frame.getParentFpForUs() == null) {
							debug(String.format("Frame %s needs parent FP because it calls sibling %s who needs parent FP",
								calling_frame.getName(), callee_frame.getName()));
							calling_frame.addParentFpParameter();
						}
						callee_parentfp = 
							calling_frame.getParentFpForUs().createCode(calling_frame);
					} else {
						frame.AbstractFrame parent_sibling_with_callee = calling_frame;
						while (parent_sibling_with_callee.getParent().getId() !=
								callee_frame.getParent().getId())
							parent_sibling_with_callee = parent_sibling_with_callee.getParent();
						
						callee_parentfp = parent_sibling_with_callee.getParentFpForChildren().
							createCode(calling_frame);
					}
					call.call.callee_parentfp = callee_parentfp;
				}
			}
		}
	}
	
	private Result translateFunctionCall(parse.FunctionCall expression,
			ir.Label last_loop_exit, frame.AbstractFrame current_frame) {
		Result result = new Result();
		Declaration var_or_function = (Declaration)
			func_and_var_names.lookup(expression.function.name);
		if (var_or_function == null) {
			Error.current.error("Undefined variable or function identifier " +
				expression.function.name,
				expression.function.position);
			result.type = types.errorType();
			result.code = ErrorPlaceholderCode();
			return result;
		}
		if (!(var_or_function instanceof Function)) {
			Error.current.error("Variable used as a function",
				expression.function.position);
			result.type = types.errorType();
			result.code = ErrorPlaceholderCode();
			return result;
		}
		Function function = (Function)var_or_function;
		result.type = function.return_type.resolve();
		ListIterator<FuncArgument> function_arg = function.arguments.listIterator();
		boolean already_too_many = false;
		LinkedList<ir.Code> arguments_code = new LinkedList<>();
		for (parse.Node passed_arg: expression.arguments.expressions) {
			Type argument_type = types.errorType();
			Result passed = translateExpression(passed_arg, null,
				current_frame, true);
			arguments_code.add(passed.code);
			if (! already_too_many && (! function_arg.hasNext())) {
				already_too_many = true;
				Error.current.error("Too many arguments to the function",
					passed_arg.position);
			}
			
			if (! already_too_many) {
				argument_type = function_arg.next().type.resolve();
				if (! CheckAssignmentTypes(argument_type, passed.type)) {
					Error.current.error("Incompatible result.type passed as an argument",
						passed_arg.position);
				}
			}
		}
		if (function_arg .hasNext())
			Error.current.error("Not enough arguments to the function",
				expression.position);
		result.code = makeCallCode(function, arguments_code, current_frame);
		return result;
	}
	
	private Result translateRecordInstantiation(parse.RecordInstantiation expression,
			ir.Label last_loop_exit, frame.AbstractFrame current_frame) {
		Result result = new Result();
		result.type = types.getType(expression.type, false).resolve();
		if (result.type == null) {
			Error.current.error("Unknown result.type identifier", expression.type.position);
			result.type = types.errorType();
			return result;
		}
		if (!(result.type instanceof RecordType)) {
			if (result.type.kind != Type.ERROR)
				Error.current.error("Not a record result.type being record-instantiated",
					expression.type.position);
			result.type = types.errorType();
			return result;
		}
		RecordType record = (RecordType)result.type;
		
		ir.VirtualRegister record_address = IRenv.addRegister();
		ir.StatmSequence sequence = new ir.StatmSequence();
		LinkedList<ir.Code> alloc_argument = new LinkedList<>();
		alloc_argument.add(new ir.ExpCode(
			new ir.IntegerExp(record.size)));
		ir.Code alloc_code = makeCallCode(getmem_func, alloc_argument, current_frame);
		sequence.addStatement(new ir.MoveStatm(
			new ir.RegisterExp(record_address),
			IRenv.codeToExpression(alloc_code)));
			
		ListIterator<RecordField> record_field_i = record.fields.listIterator();
		for (parse.Node set_field :expression.fieldvalues.expressions) {
			if (! record_field_i.hasNext()) {
				Error.current.error("More field initialized than there are in the record",
					set_field.position);
				break;
			}
			RecordField record_field = record_field_i.next();
			Type field_type = record_field.type.resolve();
			assert set_field instanceof parse.BinaryOp;
			parse.BinaryOp fieldvalue = (parse.BinaryOp)set_field;
			assert fieldvalue.operation == parse.Parser.SYM_ASSIGN;
			assert fieldvalue.left instanceof parse.Identifier;
			parse.Identifier field = (parse.Identifier)fieldvalue.left;
			if (! field.name.equals(record_field.name)) {
				Error.current.error("Wrong field name " + field.name +
					" expected " + record_field.name,
					fieldvalue.left.position);
			} else {
				Result value = translateExpression(fieldvalue.right, last_loop_exit,
					current_frame, true);
				if (! CheckAssignmentTypes(field_type, value.type)) {
					Error.current.error("Incompatible result.type for this field value",
						fieldvalue.right.position);
				} else {
					sequence.addStatement(new ir.MoveStatm(
						new ir.MemoryExp(new ir.BinaryOpExp(ir.BinaryOpExp.PLUS,
							new ir.RegisterExp(record_address),
							new ir.IntegerExp(record_field.offset)
						)), IRenv.codeToExpression(value.code)
					));
				}
			}
		}
		result.code = new ir.ExpCode(new ir.StatExpSequence(sequence,
			new ir.RegisterExp(record_address)));
		return result;
	}

	public class Result {
		ir.Code code;
		Type type;
	}
	
	private Result translateExpression(parse.Node expression, ir.Label last_loop_exit,
			frame.AbstractFrame current_frame, boolean expect_comparison) {
		Result res;
		if (expression instanceof parse.IntValue) {
			res = new Result();
			res.code = translateIntValue(((parse.IntValue)expression).value);
			res.type = types.intType();
		} else if (expression instanceof parse.StringValue) {
			res = new Result();
			res.code = translateStringValue(((parse.StringValue)expression).value);
			res.type = types.stringType();
		} else if (expression instanceof parse.Identifier)
			res = translateIdentifier((parse.Identifier)expression, current_frame);
		else if ((expression instanceof parse.Trivial) &&
				(((parse.Trivial)expression).kind == parse.Trivial.NIL)) {
			res = new Result();
			res.type = types.nilType();
			res.code = translateIntValue(0);
		} else if (expression instanceof parse.BinaryOp) {
			if (! expect_comparison &&
					((parse.BinaryOp)expression).operation == parse.Parser.SYM_EQUAL)
				Error.current.warning("Might have written comparison instead of assignment",
					expression.position);
			res = translateBinaryOperation((parse.BinaryOp)expression,
				last_loop_exit, current_frame);
		} else if (expression instanceof parse.Sequence)
			res = translateSequence(((parse.Sequence)expression).content.expressions,
				last_loop_exit, current_frame, null);
		else if (expression instanceof parse.ArrayIndexing)
			res = translateArrayIndexing((parse.ArrayIndexing)expression,
				last_loop_exit, current_frame);
		else if (expression instanceof parse.ArrayInstantiation)
			res = translateArrayInstantiation((parse.ArrayInstantiation)expression,
				last_loop_exit, current_frame);
		else if (expression instanceof parse.If)
			res = translateIf((parse.If)expression,	last_loop_exit, current_frame);
		else if (expression instanceof parse.IfElse)
			res = translateIfElse((parse.IfElse)expression,
				last_loop_exit, current_frame);
		else if (expression instanceof parse.While)
			res = translateWhile((parse.While)expression, current_frame);
		else if (expression instanceof parse.For)
			res = translateFor((parse.For)expression, current_frame);
		else if ((expression instanceof parse.Trivial) &&
				(((parse.Trivial)expression).kind == parse.Trivial.BREAK)) {
			res = new Result();
			res.type = types.voidType();
			if (last_loop_exit == null)
				Error.current.error("Break outside of a loop", expression.position);
			else
				res.code = translateBreak(last_loop_exit);
		} else if (expression instanceof parse.Scope)
			res = translateScope((parse.Scope)expression,
				last_loop_exit, current_frame);
		else if (expression instanceof parse.RecordField)
			res = translateRecordField((parse.RecordField)expression,
				last_loop_exit, current_frame);
		else if (expression instanceof parse.FunctionCall)
			res = translateFunctionCall((parse.FunctionCall)expression,
				last_loop_exit, current_frame);
		else if (expression instanceof parse.RecordInstantiation)
			res = translateRecordInstantiation((parse.RecordInstantiation)expression,
				last_loop_exit, current_frame);
		else {
			Error.current.fatalError("Unexpected node as expression");
			res = null;
		}
		return res;
	}
	
	private class PredefinedFunc {
		String name;
		Type return_type;
		String external_name;
		Type[] arguments;
		boolean available;
		
		public PredefinedFunc(String _name, Type _ret,
				String _external, Type[] _args) {
			name = _name;
			return_type = _ret;
			external_name = _external;
			arguments = _args;
			available = ! _name.startsWith(".");
		}
	}
	
	public TranslatorPrivate(ir.IREnvironment _ir_env,
			frame.AbstractFrameManager _framemanager) {
		super("translator.log");
		IRenv = _ir_env;
		framemanager = _framemanager;
		types = new TypesEnvironment(_framemanager);
		canon = new ir.Canonicalizer(_ir_env);
		framemanager.setFrameParentFpNotificationHandler(this);
		
		Type[] nothing = {};
		Type[] one_string = {types.stringType()};
		Type[] one_int = {types.intType()};
		Type[] int_int = {types.intType(), types.intType()};
		Type[] string_string = {types.stringType(), types.stringType()};
		Type[] s_int_int = {types.stringType(), types.intType(), types.intType()};
		
		PredefinedFunc funcs[] = {
			new PredefinedFunc("print", types.voidType(), "__print", one_string),
			new PredefinedFunc("flush", types.voidType(), "__flush", nothing),
			new PredefinedFunc("getchar", types.stringType(), "__getchar", nothing),
			new PredefinedFunc("ord", types.intType(), "__ord", one_string),
			new PredefinedFunc("chr", types.stringType(), "__chr", one_int),
			new PredefinedFunc("size", types.intType(), "__size", one_string),
			new PredefinedFunc("substring", types.stringType(), "__substring", s_int_int),
			new PredefinedFunc("concat", types.stringType(), "__concat", string_string),
			new PredefinedFunc("not", types.intType(), "__not", one_int),
			new PredefinedFunc("exit", types.voidType(), "exit", one_int),
			new PredefinedFunc(".getmem", types.pointerType(), "__getmem", int_int),
			new PredefinedFunc(".getmem_fill", types.pointerType(), "__getmem_fill", int_int),
			new PredefinedFunc(".strcmp", types.intType(), "__strcmp", string_string)
		};
		
		for (PredefinedFunc f: funcs) {
			Function func = new Function(f.name, f.return_type, null, null, 
				framemanager.rootFrame(), IRenv.addLabel(f.external_name));
			functions.add(func);
			for (Type arg: f.arguments)
				func.addArgument("LOL", arg, null);
			if (f.available)
				func_and_var_names.add(f.name, func);
			if (f.name.equals(".getmem"))
				getmem_func = func;
			else if (f.name.equals(".getmem_fill"))
				getmem_fill_func = func;
			else if (f.name.equals(".strcmp"))
				strcmp_func = func;
		}
	}
	
	private void prespillRegisters(ir.Code code, frame.AbstractFrame frame) {
		LinkedList<frame.VarLocation> to_spill = new LinkedList<>();
		for (VarLocation param: frame.getParameters())
			if (param.isRegister() &&
					param.getRegister().isPrespilled())
				to_spill.add(param);
		for (VarLocation local: frame.getLocalVariables())
			if (local.isRegister() &&
					local.getRegister().isPrespilled())
				to_spill.add(local);
		VarLocation parentfp = frame.getParentFpForUs();
		if ((parentfp != null) && parentfp.isRegister() &&
				parentfp.getRegister().isPrespilled())
			to_spill.add(parentfp);

		ir.StatmSequence sequence = new ir.StatmSequence();
		TreeMap<Integer, VarLocation> spilled_locations = new TreeMap<>();
		
		for (VarLocation var: to_spill) {
			assert var.isRegister();
			ir.VirtualRegister reg = var.getRegister();
			assert reg.isPrespilled();
			if (! var.read_only)
				spilled_locations.put(reg.getIndex(), reg.getPrespilledLocation());
			if (var.isPredefined())
				sequence.addStatement(new ir.MoveStatm(
					reg.getPrespilledLocation().createCode(frame),
					new ir.RegisterExp(reg)));
		}
		
		if (code instanceof ir.ExpCode) {
			ExpCode exp_code = (ir.ExpCode)code;
			if (! spilled_locations.isEmpty())
				exp_code.exp = frame.prespillRegisters(exp_code.exp,
					spilled_locations);
			if (! sequence.statements.isEmpty())
				exp_code.exp = new ir.StatExpSequence(sequence, exp_code.exp);
		} else {
			StatmCode statm_code = (ir.StatmCode)code;
			if (! spilled_locations.isEmpty())
				frame.prespillRegisters(statm_code.statm, spilled_locations);
			
			if (! sequence.statements.isEmpty()) {
				sequence.addStatement(statm_code.statm);
				statm_code.statm = sequence;
			}
		}
	}
	
	public Result translateProgram(parse.Node expression, frame.AbstractFrame root_frame) {
		Result res = translateExpression(expression, null, root_frame, false);
		addParentFpToCalls();
		for (Function fcn: functions)
			prespillRegisters(fcn.implementation.body, fcn.implementation.frame);
		prespillRegisters(res.code, root_frame);
		return res;
	}
	
	public void canonicalizeFunctionBody(ir.Function func) {
		if (func.body instanceof ir.ExpCode) {
			ir.ExpCode exp_code = (ir.ExpCode)func.body;
			exp_code.exp = canon.canonicalizeExpression(exp_code.exp, null, null);
			canon.arrangeJumpsInExpression(exp_code.exp);
		} else if ((func.body instanceof ir.StatmCode) &&
				!(func.body instanceof ir.CondJumpPatchesCode)) {
			ir.StatmCode statm_code = (ir.StatmCode)func.body;
			statm_code.statm = canon.canonicalizeStatement(statm_code.statm);
			if (statm_code.statm instanceof ir.StatmSequence)
				canon.arrangeJumps((ir.StatmSequence)statm_code.statm);
		} else {
			ir.Expression expr = IRenv.codeToExpression(func.body);
			expr = canon.canonicalizeExpression(expr, null, null);
			canon.arrangeJumpsInExpression(expr);
			func.body = new ir.ExpCode(expr);
		}
	}
	
	public LinkedList<Function> getFunctions() {return functions;}
}
