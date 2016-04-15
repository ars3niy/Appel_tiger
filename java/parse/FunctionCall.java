package parse;

public class FunctionCall extends Node {
	public Identifier function;
	public ExpressionList arguments;
	
	FunctionCall(Node _func, Node _args) {
		function = (Identifier)_func;
		arguments = (ExpressionList)_args;
	}
}
