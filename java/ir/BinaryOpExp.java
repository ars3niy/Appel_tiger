package ir;

public class BinaryOpExp extends Expression {
	public static final int
		PLUS = 0,
		MINUS = 1,
		MUL = 2,
		DIV = 3,
		AND = 4,
		OR = 5,
		XOR = 6,
		SHL = 7,
		SHR = 8,
		SHAR = 9,
		MAX = 10;
	
	public int operation;
	public Expression left, right;
	
	public BinaryOpExp(int _op, Expression _left, Expression _right) {
		operation = _op;
		left = _left;
		right = _right;
	}
}
