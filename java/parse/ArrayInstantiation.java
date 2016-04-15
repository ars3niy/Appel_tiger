package parse;

public class ArrayInstantiation extends Node {
	public ArrayIndexing arraydef;
	public Node value;
	
	ArrayInstantiation(Node _def, Node _value) {
		arraydef = (ArrayIndexing)_def;
		value = _value;
		position = value.position;
	}
	
	public boolean arrayIsSingleId() {return arraydef.array instanceof Identifier;} 
}
