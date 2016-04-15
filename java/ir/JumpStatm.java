package ir;

import java.util.LinkedList;

public class JumpStatm extends Statement {
	public Expression dest;
	public LinkedList<Label[]> possible_results = new LinkedList<>();
	
	public JumpStatm(Expression _dest) {
		dest = _dest;
	}
	
	public JumpStatm(Label _dest) {
		LabelAddressExp d = new LabelAddressExp(_dest);
		dest = d;
		possible_results.add(d.labelRef());
	}
	
	void addPossibleResult(Label result) {
		Label[] ref = new Label[1];
		ref[0] = result;
		possible_results.add(ref);
	}
}
