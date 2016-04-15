package ir;

import java.util.LinkedList;

public class CondJumpPatchesCode extends StatmCode {
	public LinkedList<Label[]> replace_true = new LinkedList<>();
	public LinkedList<Label[]> replace_false = new LinkedList<>();
	
	public CondJumpPatchesCode(Statement statm) {
		super(statm);
	}
	
	public static void putLabels(LinkedList<Label[]> replace_true,
			LinkedList<Label[]> replace_false, Label true_label, Label false_label) {
		for (Label[] label: replace_true)
			label[0] = true_label;
		for (Label[] label: replace_false)
			label[0] = false_label;
	}
}
