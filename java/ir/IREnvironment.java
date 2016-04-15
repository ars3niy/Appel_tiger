package ir;

import java.util.LinkedList;

public interface IREnvironment {
	public Label addLabel();
	public Label addLabel(String name);
	public int labelCount();
	public VirtualRegister addRegister();
	public VirtualRegister addRegister(String name);
	public int registerCount();
	public Blob addBlob();
	public LinkedList<Blob> getBlobs();
	public Expression codeToExpression(Code code);
	public Statement codeToStatement(Code code);
	
	public class CondJump {
		public Statement statm;
		public LinkedList<Label[]> replace_true, replace_false;
	}
	
	public CondJump codeToCondJump(Code code);
}

