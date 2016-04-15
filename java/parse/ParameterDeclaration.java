package parse;

public class ParameterDeclaration extends Node {
	public Identifier name, type;
	
	/**
	 * Unique within the program tree for all ParameterDeclarations,
	 * VariableDeclarations, For's and FunctionDeclaration
	 * Used to share information about the varible between multiple passes
	 * of the syntax tree
	 */
	public int id;

	ParameterDeclaration(int _id, Node _name, Node _type) {
		id = _id;
		name = (Identifier)_name;
		type = (Identifier)_type;
		position = name.position;
	}
}
