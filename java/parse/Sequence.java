package parse;

public class Sequence extends Node {
	public ExpressionList content;
	
	Sequence(Node _content) {
		content = (ExpressionList) _content;
	}
}
