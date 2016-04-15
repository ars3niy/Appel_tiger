package asm;

public class Instruction {
	public static String Input(int number)
	{
		return "&i" + (char)('0' + number);
	}
	public static String Output(int number)
	{
		return "&o" + (char)('0' + number);
	}
	
	public String notation;
	public ir.VirtualRegister[] inputs, outputs;
	public ir.Label label = null;
	public boolean is_reg_to_reg_assign;
	
	/**
	 * NULL element means fall through to the next instruction
	 */
	public ir.Label[] destinations;
	
	public Instruction(String _notation) {
		this(_notation, false);
	}
	
	public Instruction(String _notation,
			boolean _reg_to_reg_assign) {
		notation = _notation;
		is_reg_to_reg_assign = _reg_to_reg_assign;
		destinations = new ir.Label[1];
		destinations[0] = null;
		inputs = new ir.VirtualRegister[0];
		outputs = inputs;
	}
	
	public Instruction(String _notation,
			ir.VirtualRegister[] _inputs,
			ir.VirtualRegister[] _outputs,
			boolean _reg_to_reg_assign) {
		this(_notation, _inputs, _outputs, _reg_to_reg_assign, new ir.Label[0]);
	}
	
	public Instruction(String _notation,
			ir.VirtualRegister[] _inputs,
			ir.VirtualRegister[] _outputs,
			boolean _reg_to_reg_assign,
			ir.Label[] _destinations) {
		notation = _notation;
		inputs = _inputs;
		outputs = _outputs;
		is_reg_to_reg_assign = _reg_to_reg_assign;
		if (_destinations.length == 0) {
			destinations = new ir.Label[1];
			destinations[0] = null;
		} else
			destinations = _destinations;
	}
}
