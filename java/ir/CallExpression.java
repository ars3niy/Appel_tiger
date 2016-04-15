package ir;

import java.util.LinkedList;

public class CallExpression extends Expression {
	public Expression function;
	public LinkedList<Expression> arguments = new LinkedList<>();
	public Expression callee_parentfp;
	
	public CallExpression(Expression _func, Expression _callee_parentfp) {
		function = _func;
		callee_parentfp = _callee_parentfp;
	}
	
	public void addArgument(Expression arg) {
		arguments.add(arg);
	}
}
