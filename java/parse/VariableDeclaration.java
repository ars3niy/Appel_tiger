package parse;

public class VariableDeclaration extends Node {
	public Identifier name, type;
	public Node value;
	/**
	 * Unique within the program tree for all ParameterDeclarations,
	 * VariableDeclarations, For's and FunctionDeclaration
	 * Used to share information about the varible between multiple passes
	 * of the syntax tree
	 */
	public int id;
	
	VariableDeclaration(int _id, Node _name, Node _value) {
		this(_id, _name, _value, null);
	}
	
	VariableDeclaration(int _id, Node _name, Node _value, Node _type) {
		id = _id;
		name = (Identifier)_name;
		type = (Identifier)_type;
		value = _value;
		position = value.position;
	}
}
