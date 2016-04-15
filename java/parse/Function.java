package parse;

public class Function extends Node {
	public Identifier name, type;
	public ExpressionList parameters;
	public Node body;
	/**
	 * Unique within the program tree for all ParameterDeclarations,
	 * VariableDeclarations, For's and FunctionDeclaration
	 * Used to share information about the varible between multiple passes
	 * of the syntax tree
	 */
	public int id;
	
	Function(int _id, Node _name, Node _type, Node _parameters, Node _body) {
		id = _id;
		name = (Identifier)_name;
		type = (Identifier)_type;
		parameters = (ExpressionList)_parameters;
		for (Node p: parameters.expressions)
			assert p instanceof ParameterDeclaration;
		body = _body;
		position = name.position;
	}
}
