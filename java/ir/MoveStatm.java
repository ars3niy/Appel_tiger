package ir;

public class MoveStatm extends Statement {
	public Expression to, from;
	
	public MoveStatm(Expression _to, Expression _from) {
		to = _to;
		from = _from;
	}
}
