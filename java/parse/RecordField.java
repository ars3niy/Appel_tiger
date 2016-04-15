package parse;

public class RecordField extends Node {
	public Node record;
	public Identifier field;
	
	RecordField(Node _record, Node _field) {
		record = _record;
		field = (Identifier)_field;
		position = field.position;
	}
}
