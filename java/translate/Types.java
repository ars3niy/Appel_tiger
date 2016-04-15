package translate;

import error.Error;
import java.util.LinkedList;
import java.util.TreeMap;

class Type {
	static final int
		ERROR = 0,
		VOID = 1,
		NIL = 2,
		INT = 3,
		STRING = 4,
		COMPLEX = 5;
	
	static Type ErrorType = new Type(Type.ERROR);
	static Type VoidType = new Type(Type.VOID);
	static Type NilType = new Type(Type.NIL);
	static Type IntType = new Type(Type.INT);
	static Type LoopedIntType = new Type(Type.INT, true);
	static Type StringType = new Type(Type.STRING);
	
	int kind;
	boolean is_loop_var = false;
	
	Type(int _kind) {
		kind = _kind;
	}
	
	Type(int _kind, boolean _looped) {
		kind = _kind;
		is_loop_var = _looped;
	}
	
	Type resolve() {return resolve(null);}
	
	Type resolve(NamedType parent) {
		if (! (this instanceof NamedType))
			return this;
		
		NamedType named = (NamedType)this;
		if (named.visited) {
			assert parent != null;
			Error.current.error(String.format(
				"Type %s being defined as %s creates a circular reference",
				parent.name, named.name), parent.declaration.position);
			parent.meaning = ErrorType;
			return ErrorType;
		}
		
		if (named.meaning == null)
			Error.current.fatalError("Found stray unresolved type " +
					named.name, named.declaration.position);
		
		named.visited = true;
		Type result = named.meaning.resolve(named);
		named.visited = false;
		return result;
	}
}

class NamedType extends Type {
	String name;
	Type meaning = null;
	parse.Node declaration;
	boolean visited = false;
	
	NamedType(String _name, parse.Node _decl) {
		super(Type.COMPLEX);
		name = _name;
		declaration = _decl;
	}
}

class ArrayType extends Type {
	/**
	 * Unique within the TypesEnvironment for all arrays and records
	 * Used for checking if two Type * are the same type
	 */
	int id;
	
	Type elemtype;
	
	ArrayType(int _id, Type _elemtype) {
		super(Type.COMPLEX);
		id = _id;
		elemtype = _elemtype;
	}
}

class RecordField {
	String name;
	Type type;
	int offset;
	
	RecordField(String _name, Type _type) {
		name = _name;
		type = _type;
	}
}

class RecordType extends Type {
	/**
	 * Unique within the TypesEnvironment for all arrays and records
	 * Used for checking if two Type * are the same type
	 */
	int id;
	
	LinkedList<RecordField> fields = new LinkedList<>();
	private TreeMap<String, RecordField> fields_map = new TreeMap<>();
	int size;
	
	RecordType(int _id) {
		super(Type.COMPLEX);
		id = _id;
	}
	
	void addField(String name, Type type) {
		fields.add(new RecordField(name, type));
		fields_map.put(name, fields.getLast());
	}
	
	RecordField findField(String name) {
		return fields_map.get(name);
	}
}
