package parse;

public class ArrayTypeDefinition extends Node {
	public Identifier elemtype;
	
	ArrayTypeDefinition(Node _type) {
		elemtype = (Identifier) _type;
		position = elemtype.position;
	}
}
