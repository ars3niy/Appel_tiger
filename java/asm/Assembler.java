package asm;

import java.io.PrintWriter;
import java.util.LinkedList;

public interface Assembler {

	public void translateFunction(ir.Code code,
			frame.AbstractFrame frame, Instructions result);
	
	public void translateProgram(ir.Code code,
			frame.AbstractFrame frame, Instructions result);
	
	public void implementFrameSize(frame.AbstractFrame frame,
			Instructions result);
	
	public void finishFunction(ir.Label label, Instructions result);
	
	public void finishProgram(Instructions result); 
		
	public void outputCode(PrintWriter output,
			final LinkedList<Instructions> code,
			final ir.RegisterMap register_map);
	
	public void outputBlobs(PrintWriter output, final LinkedList<ir.Blob> blobs);
	
	/**
	 * Frame pointer register must NOT be one of them
	 */
	public ir.VirtualRegister[] getAvailableRegisters();
	
	public void spillRegister(frame.AbstractFrame frame, Instructions code,
		final ir.VirtualRegister reg);
}
