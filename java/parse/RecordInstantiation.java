package parse;

public class RecordInstantiation extends Node {
	public Identifier type;
	public ExpressionList fieldvalues;
	
	RecordInstantiation(Node _type, Node _values) {
		type = (Identifier)_type;
		fieldvalues = (ExpressionList)_values;
	}
}
