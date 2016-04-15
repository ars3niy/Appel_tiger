package parse;

public class Trivial extends Node {
	public static final int
		NIL = 0,
		BREAK = 1;
	
	public int kind;
	
	Trivial(int _kind) {kind = _kind;}
}
