package ir;

public class StatExpSequence extends Expression {
	public Statement stat;
	public Expression exp;
	
	public StatExpSequence(Statement _stat, Expression _exp) {
		stat = _stat;
		exp = _exp;
	}
}
