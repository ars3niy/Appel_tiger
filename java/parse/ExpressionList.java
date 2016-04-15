package parse;

import java.util.LinkedList;

public class ExpressionList extends Node {
	public LinkedList<Node> expressions = new LinkedList<>();
	
	void prepend(Node expression) {
		expressions.addFirst(expression);
	}
}
