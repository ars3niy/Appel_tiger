package frame;

import java.util.LinkedList;
import error.Error;

public class VarLocation extends util.DebugPrinter {
	/**
	 * null if this variable is in the memory from the beginning
	 * 
	 * If the register is prespilled, the variable will be inconsistently used
	 * sometimes as a register, sometimes a memory location. The resolution is:
	 * if the register is read only, keep the duality and just copy from the
	 * register to memory in the beginning of the function. It can still get
	 * spilled later by the register allocator, in which case that assignment to
	 * memory becomes assignment of a memory location to itself. The redundant
	 * assignment can be removed by a speciall read-only prespilled-spilling
	 * logic of the assembler.
	 * If the register is not read-only, it will be actually spilled for sure
	 * and only ever used as a memory location. 
	 */
	private ir.VirtualRegister reg;
	private AbstractFrame owner_frame;	
	
	/**
	 * Offset from frame pointer
	 */
	private int offset;
	private int size;
	private String name;
	/**
	 * True for function parameters including the parent frame pointer parameter,
	 * meaning that the variable holds a meaningful value before our code does
	 * anything
	 */
	private boolean predefined;
	/**
	 * If true, it means that the variable is assigned only once before
	 * it is ever used. This happens to the register where the parent frame
	 * pointer parameter is moved to from its original register, which exists
	 * is the parameter was passed in a register.
	 */
	public boolean read_only = false;
	public boolean isPredefined() {return predefined;}
	
	public VarLocation(AbstractFrame owner, int _size, int _ofset, String _name,
			boolean _predefined) {
		super("translator.log");
		owner_frame = owner;
		size = _size;
		offset = _ofset;
		name = _name;
		reg = null;
		predefined = _predefined;
	}
	
	public VarLocation(AbstractFrame owner, int _size, ir.VirtualRegister _reg,
			String _name, boolean _predefined) {
		super("translator.log");
		owner_frame = owner;
		size = _size;
		reg = _reg;
		name = _name;
		offset = -1;
		predefined = _predefined;
	}
	
	public boolean isRegister() {return reg != null;}
	public ir.VirtualRegister getRegister() {return reg;}
	public void prespillRegister() {
		if (reg != null)
			reg.prespill(owner_frame.createMemoryVariable(size, ".store." + reg.getName()));
	}
	public AbstractFrame getOwnerFrame() {return owner_frame;}
	
	private interface ExpressionPointer {
		public void set(ir.Expression value);
	}
	
	private class ExpressionArrayPointer implements ExpressionPointer {
		private ir.Expression[] data;
				
		public ExpressionArrayPointer(ir.Expression[] _data) {data = _data;}
		public void set(ir.Expression value) {data[0] = value;}
	}
	
	private class BinopLeftPointer implements ExpressionPointer {
		private ir.BinaryOpExp data;
				
		public BinopLeftPointer (ir.BinaryOpExp _data) {data = _data;}
		public void set(ir.Expression value) {data.left = value;}
	}
	
	public ir.Expression createCode(AbstractFrame calling_frame) {
		if (reg != null) {
			if (calling_frame.getId() == owner_frame.getId())
				return new ir.RegisterExp(reg);
			else {
				if (! reg.isPrespilled()) {
					debug(String.format("Prespilling register %s: owner frame %s, accessed from %s",
						reg.getName(), owner_frame.getName(), calling_frame.getName()));
					prespillRegister();
				}
				return reg.getPrespilledLocation().createCode(calling_frame);
			}
		}
		
		LinkedList<AbstractFrame> frame_stack = new LinkedList<>();
		{
			AbstractFrame frame = calling_frame;
			while (frame != null) {
				frame_stack.addFirst(frame);
				if (frame == owner_frame)
					break;
				frame = frame.getParent();
			}
			if (frame != owner_frame)
				Error.current.fatalError("Variable has been used outside its function without syntax error??");
		}
		
		ir.Expression[] result = {null};
		ExpressionPointer put_access_to_owner_frame =
			new ExpressionArrayPointer(result);
		
		for (AbstractFrame frame: frame_stack) {
			VarLocation var_location;
			if (frame.getId() == owner_frame.getId())
				var_location = this;
			else if (frame == calling_frame) {
				if (frame.getParentFpForUs() == null) {
					debug(String.format(
						"Frame %s needs parent FP because it uses variable %s of parent frame %s",
						frame.getName(), this.name, owner_frame.getName()));
					frame.addParentFpParameter();
				}
				var_location = frame.getParentFpForUs();
			} else {
				if (frame.getParentFpForUs() == null) {
					debug(String.format(
						"Frame %s needs parent FP because its child %s uses variable %s of its parent frame %s",
						frame.getName(), calling_frame.getName(), this.name,
						owner_frame.getName()));
					frame.addParentFpParameter();
				}
				var_location = frame.getParentFpForChildren();
			}
					
			assert var_location != null;
			assert put_access_to_owner_frame != null;
		
			ir.Expression access_to_owner_frame;
			ExpressionPointer put_access_to_next_owner_frame;
			
			if (var_location.isRegister() && (frame.getId() == calling_frame.getId())) {
				access_to_owner_frame = var_location.createCode(frame);
				put_access_to_next_owner_frame = null;
			} else {
				if (var_location.isRegister()) {
					if (! var_location.getRegister().isPrespilled()) {
						debug(String.format("Prespilling register %s: owner frame %s, accessed from %s",
							var_location.getRegister().getName(),
							frame.getName(), calling_frame.getName()));
						var_location.prespillRegister();
					}
					var_location = var_location.getRegister().getPrespilledLocation();
				}
				assert ! var_location.isRegister();
				ir.BinaryOpExp address_expr = new ir.BinaryOpExp(
					ir.BinaryOpExp.PLUS, null, new ir.IntegerExp(var_location.offset));
				access_to_owner_frame = new ir.MemoryExp(address_expr);
				put_access_to_next_owner_frame = new BinopLeftPointer(address_expr);
			}
			
			put_access_to_owner_frame.set(access_to_owner_frame);
			put_access_to_owner_frame = put_access_to_next_owner_frame;
		}
		if (put_access_to_owner_frame != null) {
			put_access_to_owner_frame.set(
				calling_frame.getFramePointer().createCode(calling_frame));
		}
		
		assert result[0] != null;
		return result[0];
	}
}
