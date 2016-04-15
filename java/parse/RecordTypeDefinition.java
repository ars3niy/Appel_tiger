package parse;

public class RecordTypeDefinition extends Node {
	public ExpressionList fields;
	
	RecordTypeDefinition(Node _fields) {
		fields = (ExpressionList)_fields;
	}
}
