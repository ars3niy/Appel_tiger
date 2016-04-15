package ir;

public class VirtualRegister {
	private int index;
	private String name;
	/**
	 * If a local variable or parameter was initially created to be stored
	 * in a register but then got accessed from a nested function or by pointer,
	 * this will be set no non-null value 
	 */
	private frame.VarLocation prespilled_location = null;
	
	public VirtualRegister(int _index, String _name) {
		index = _index;
		name = _name;
	}
	
	public VirtualRegister(int _index) {
		index = _index;
		name = String.format("t%d", index);
	}
	
	public void prespill(frame.VarLocation location) {
		prespilled_location = location;
	}
	
	public boolean isPrespilled() {
		return prespilled_location != null;
	}
	
	public frame.VarLocation getPrespilledLocation() {
		return prespilled_location;
	}
	
	public int getIndex() {return index;}
	
	public String getName() {return name;}
}
