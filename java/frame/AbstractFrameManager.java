package frame;

public abstract class AbstractFrameManager {
	private ir.IREnvironment IRenv;
	private ParentFpHandler parent_fp_handler = null;
	
	public abstract AbstractFrame rootFrame();
	public abstract AbstractFrame newFrame(AbstractFrame parent, String name);
	public abstract int getIntSize();
	public abstract int getPointerSize();
	public abstract int updateRecordSize(int old_size, int field_size);
	/**
	 * Abstract because of possible endianness variability
	 */
	public abstract void placeInt(int value, byte[] dest);
	
	protected void placeIntBigEndian(int value, byte[] dest) {
		for (int i = 0; i < 8; i++) {
			dest[i] = (byte)(value % 0x100);
			value /= 0x100;
		}
	}
	
	public AbstractFrameManager(ir.IREnvironment _irenv) {
		IRenv = _irenv;
	}
	
	public ir.IREnvironment getIREnvironment() {return IRenv;}
	
	public void setFrameParentFpNotificationHandler(ParentFpHandler handler) {
		parent_fp_handler = handler;
	}
	
	public void notifyFrameWithParentFp(AbstractFrame frame) {
		if (parent_fp_handler != null)
			parent_fp_handler.notifyFrameWithParentFp(frame);
	}
}
