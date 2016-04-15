package translate;

import java.util.LinkedList;

class Declaration {
	String name;
	
	Declaration(String _name) {name = _name;}
}

class Variable extends Declaration {
	Type type;
	ir.Code value;
	frame.VarLocation implementation;
	
	Variable(String _name, Type _type, ir.Code _value,
			frame.VarLocation _impl) {
		super(_name);
		type = _type;
		value = _value;
		implementation = _impl;
	}
}

class FuncArgument extends Variable {
	Function func;
	
	FuncArgument(Function owner, String _name, Type _type, 
			frame.VarLocation _impl) {
		super(_name, _type, null, _impl);
		func = owner;
	}
}

class Function extends Declaration {
	Type return_type;
	LinkedList<FuncArgument> arguments = new LinkedList<>();
	parse.Node raw_body;
	ir.Function implementation;
	
	Function(String _name, Type _return_type, parse.Node _raw,
			ir.Code _body, frame.AbstractFrame _frame, ir.Label _label) {
		super(_name);
		return_type = _return_type;
		raw_body = _raw;
		implementation = new ir.Function(_name,  _body, _frame, _label);
	}
	
	FuncArgument addArgument(String name, Type type, frame.VarLocation impl) {
		arguments.add(new FuncArgument(this, name, type, impl));
		return arguments.getLast();
	}
}