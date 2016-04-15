package frame;

import java.util.LinkedList;
import java.util.TreeMap;
import java.util.ListIterator;

public abstract class AbstractFrame extends util.DebugPrinter {
	protected AbstractFrameManager framemanager;
	private LinkedList<VarLocation> parameters = new LinkedList<>();
	private LinkedList<VarLocation> local_variables = new LinkedList<>();
	protected AbstractFrame parent;
	protected int id;
	protected String name;
	private VarLocation parent_fp_parameter = null;
	private VarLocation framepointer;
	
	public abstract VarLocation createMemoryVariable(int size, String name);
	protected abstract VarLocation createParameter(String name,
		int size);
	protected abstract VarLocation createParentFpParameter(String name);
	
	public AbstractFrame(AbstractFrameManager _framemanager,
			String _name, int _id, AbstractFrame _parent) {
		super("translator.log");
		framemanager = _framemanager;
		name = _name;
		id = _id;
		parent = _parent;
		framepointer = new VarLocation(this, 
			framemanager.getPointerSize(),
			framemanager.getIREnvironment().addRegister("fp"), "fp", true);
	}
	
	public VarLocation addVariable(String name, int size) {
		VarLocation var = new VarLocation(this, size, framemanager.
			getIREnvironment().addRegister(this.name + "::" + name),
			name, false);
		local_variables.add(var);
		return var;
	}
	
	public VarLocation addParameter(String name, int size) {
		VarLocation p = createParameter(name, size);
		parameters.add(p);
		return p;
	}
	
	public void addParentFpParameter() {
		framemanager.notifyFrameWithParentFp(this);
		parent_fp_parameter = createParentFpParameter(".parent_fp");
		assert parent_fp_parameter.getOwnerFrame() == this;
	}
	
	public final LinkedList<VarLocation> getParameters() {
		return parameters;
	}
	
	public final LinkedList<VarLocation> getLocalVariables() {
		return local_variables;
	}
	
	public VarLocation getFramePointer() {return framepointer;}
	public String getName() {return name;}
	public int getId() {return id;}
	public AbstractFrame getParent() {return parent;}
	public VarLocation getParentFpForUs() {return parent_fp_parameter;}
	
	public VarLocation getParentFpForChildren() {
		if (parent_fp_parameter == null)
			return null;
		if (parent_fp_parameter.isRegister()) {
			if (! parent_fp_parameter.getRegister().isPrespilled()) {
				debug(String.format("Prespilling frame %s parent FP register %s, accessed by child function",
						getName(), parent_fp_parameter.getRegister().getName()));
				parent_fp_parameter.prespillRegister();
			}
			return parent_fp_parameter.getRegister().getPrespilledLocation();
		} else
			return parent_fp_parameter;
	}
	
	public ir.Expression prespillRegisters(ir.Expression exp,
			TreeMap<Integer, VarLocation> spills) {
		if (exp instanceof ir.RegisterExp) {
			VarLocation spill = spills.get(((ir.RegisterExp)exp).reg.getIndex());
			if (spill != null) {
				assert spill.getOwnerFrame().getId() == getId();
				return spill.createCode(this);
			}
		} else if (exp instanceof ir.BinaryOpExp) {
			ir.BinaryOpExp binop = (ir.BinaryOpExp)exp;
			binop.left = prespillRegisters(binop.left, spills);
			binop.right = prespillRegisters(binop.right, spills);
		} else if (exp instanceof ir.MemoryExp) {
			ir.MemoryExp mem = (ir.MemoryExp)exp;
			mem.address = prespillRegisters(mem.address, spills);
		} else if (exp instanceof ir.CallExpression) {
			ir.CallExpression call = (ir.CallExpression)exp;
			call.function = prespillRegisters(call.function, spills);
			for (ListIterator<ir.Expression> arg = call.arguments.listIterator();
					arg.hasNext(); ) {
				ir.Expression arg_exp = arg.next();
				arg.set(prespillRegisters(arg_exp, spills));
			}
		} else if (exp instanceof ir.StatExpSequence) {
			ir.StatExpSequence seq = (ir.StatExpSequence) exp;
			prespillRegisters(seq.stat, spills);
			seq.exp = prespillRegisters(seq.exp, spills);
		}
		return exp;
	}

	public void prespillRegisters(ir.Statement statm,
			TreeMap<Integer, VarLocation> spills) {
		if (statm instanceof ir.MoveStatm) {
			ir.MoveStatm move = (ir.MoveStatm)statm;
			move.from = prespillRegisters(move.from, spills);
			move.to = prespillRegisters(move.to, spills);
		} else if (statm instanceof ir.ExpressionStatm) {
			ir.ExpressionStatm expstatm = (ir.ExpressionStatm)statm;
			
			// Don't need to spill the register if its value is ignored 
			prespillRegisters(expstatm.exp, spills);
		} else if (statm instanceof ir.JumpStatm) {
			ir.JumpStatm jump = (ir.JumpStatm)statm;
			jump.dest = prespillRegisters(jump.dest, spills);
		} else if (statm instanceof ir.CondJumpStatm) {
			ir.CondJumpStatm condjump = (ir.CondJumpStatm)statm;
			condjump.left = prespillRegisters(condjump.left, spills);
			condjump.right = prespillRegisters(condjump.right, spills);
		} else if (statm instanceof ir.StatmSequence){
			for (ir.Statement element: ((ir.StatmSequence)statm).statements)
				prespillRegisters(element, spills);
		}
	}
}
