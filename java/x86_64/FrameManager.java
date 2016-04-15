package x86_64;

import frame.AbstractFrame;
import frame.AbstractFrameManager;

public class FrameManager extends AbstractFrameManager {
	private Frame root_frame = new Frame(this, ".root", 0, null);
	private int framecount = 1;
	
	public FrameManager(ir.IREnvironment _irenv) {
		super(_irenv);
	}

	public AbstractFrame rootFrame() {return root_frame;}
	
	public AbstractFrame newFrame(AbstractFrame parent, String name) {
		return new Frame(this, name, framecount++, (Frame)parent);
	}
	
	public int getIntSize() {return 8;}
	public int getPointerSize() {return 8;}
	public int updateRecordSize(int old_size, int field_size) {return old_size + field_size;}
	
	public void placeInt(int value, byte[] dest) {placeIntBigEndian(value, dest);}
}
