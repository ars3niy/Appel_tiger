package ir;

public class MemoryExp extends Expression {
	public Expression address;
	
	public MemoryExp(Expression _addr) {
		address = _addr;
	}
}
