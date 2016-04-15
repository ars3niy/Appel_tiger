package ir;

import java.util.LinkedList;
import error.Error;

public class IREnvironmentImpl extends util.DebugPrinter implements IREnvironment {
	
	public IREnvironmentImpl() {
		super("registers.log");
	}
	
	private int nlabel = 0;
	public Label addLabel() {
		return new Label(nlabel++);
	}
	public Label addLabel(String name) {
		return new Label(nlabel++, name);
	}
	public int labelCount() {return nlabel;}

	private int nregister = 0;
	public VirtualRegister addRegister() {
		debug("Adding new unnamed register #" + String.valueOf(nregister));
		return new VirtualRegister(nregister++);
	}
	public VirtualRegister addRegister(String name) {
		debug("Adding new register " + name + " as #" + String.valueOf(nregister));
		return new VirtualRegister(nregister++, name);
	}
	public int registerCount() {return nregister;}
	
	private LinkedList<Blob> blobs = new LinkedList<>();
	public Blob addBlob() {
		Blob b = new Blob();
		b.label = addLabel();
		blobs.add(b);
		return b;
	}
	
	public final LinkedList<Blob> getBlobs() {return blobs;}
	
	public Expression codeToExpression(Code code) {
		if ((code instanceof StatmCode) && !(code instanceof CondJumpPatchesCode)) {
			Error.current.fatalError("Not supposed to convert statements to expressions");
			return null;
		} else if (code instanceof ExpCode)
			return ((ExpCode)code).exp;
		else {
			Label true_label = addLabel();
			Label false_label = addLabel();
			Label finish_label = addLabel();
			VirtualRegister value = addRegister();
			CondJumpPatchesCode patch_code = (CondJumpPatchesCode)code;
			CondJumpPatchesCode.putLabels(patch_code.replace_true,
				patch_code.replace_false, true_label, false_label);
			
			StatmSequence sequence = new StatmSequence();
			sequence.addStatement(patch_code.statm);
			sequence.addStatement(new LabelPlacementStatm(true_label));
			sequence.addStatement(new MoveStatm(new RegisterExp(value),
				new IntegerExp(1)));
			sequence.addStatement(new JumpStatm(finish_label));
			sequence.addStatement(new LabelPlacementStatm(false_label));
			sequence.addStatement(new MoveStatm(new RegisterExp(value),
				new IntegerExp(0)));
			sequence.addStatement(new LabelPlacementStatm(finish_label));
			
			return new StatExpSequence(sequence, new RegisterExp(value));
		}
	}
	
	public Statement codeToStatement(Code code) {
		if ((code instanceof StatmCode) && !(code instanceof CondJumpPatchesCode))
			return ((StatmCode)code).statm;
		else if (code instanceof ExpCode)
			return new ExpressionStatm(((ExpCode)code).exp);
		else {
			Label proceed = addLabel();
			CondJumpPatchesCode patch_code = (CondJumpPatchesCode)code;
			CondJumpPatchesCode.putLabels(patch_code.replace_true,
				patch_code.replace_false, proceed, proceed);
			StatmSequence seq = new StatmSequence();
			seq.addStatement(patch_code.statm);
			seq.addStatement(new LabelPlacementStatm(proceed));
			return seq;
		}
	}
	
	public CondJump codeToCondJump(Code code) {
		CondJump result = new CondJump();
		if ((code instanceof StatmCode) && !(code instanceof CondJumpPatchesCode)) {
			Error.current.fatalError("Not supposed to convert statements to conditional jumps");
			return null;
		} else if (code instanceof ExpCode) {
			Expression bool_exp = ((ExpCode)code).exp;
			result.replace_true = new LinkedList<>();
			result.replace_false = new LinkedList<>();
			if (bool_exp instanceof IntegerExp) {
				LabelAddressExp dest = new LabelAddressExp(null);
				JumpStatm jump = new JumpStatm(dest);
				jump.addPossibleResult(null);
				result.statm = jump;
				if (((IntegerExp)bool_exp).value != 0) {
					result.replace_true.add(dest.labelRef());
					result.replace_true.add(jump.possible_results.getFirst());
				} else {
					result.replace_false.add(dest.labelRef());
					result.replace_false.add(jump.possible_results.getFirst());
				}
			} else {
				CondJumpStatm condjump = new CondJumpStatm(CondJumpStatm.NONEQUAL,
					bool_exp, new IntegerExp(0), null, null);
				result.statm = condjump;
				result.replace_true.add(condjump.trueDestRef());
				result.replace_false.add(condjump.falseDestRef());
			}
		} else {
			CondJumpPatchesCode patches_code = (CondJumpPatchesCode)code;
			result.replace_true = patches_code.replace_true;
			result.replace_false = patches_code.replace_false;
			result.statm = patches_code.statm;
		}
		return result;
	}
	
	public RegisterMap initRegisterMap() {
		return new RegisterMap(nregister);
	}
}
