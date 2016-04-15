package parse;

public class TypeDeclaration extends Node {
	public Identifier name;
	public Node definition;
	
	TypeDeclaration(Node _name, Node _def) {
		name = (Identifier)_name;
		definition = _def;
		assert (_def instanceof Identifier) || (_def instanceof ArrayTypeDefinition) ||
		       (_def instanceof RecordTypeDefinition);
		position = definition.position;
	}
}
