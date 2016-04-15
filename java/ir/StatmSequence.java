package ir;

import java.util.LinkedList;

public class StatmSequence extends Statement {
	public LinkedList<Statement> statements = new LinkedList<>();
	
	public void addStatement(Statement statm) {
		statements.add(statm);
	}
}
