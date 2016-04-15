package ir;

public class RegisterExp extends Expression {
	public VirtualRegister reg;
	
	public RegisterExp(VirtualRegister _reg) {reg = _reg;}
}
