package parse;

public class Scope extends Node {
	public ExpressionList declarations, actions;
	
	Scope(Node _decl, Node _actions) {
		declarations = (ExpressionList) _decl;
		actions = (ExpressionList) _actions;
	}
}
