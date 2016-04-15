package x86_64;

import frame.AbstractFrame;
import frame.VarLocation;

class Frame extends AbstractFrame {
	private int frame_size = 0;
	private int param_count = 0;
	private int param_stack_size = 0;

	public VarLocation createMemoryVariable(int size, String name) {
		assert size <= 8;
		frame_size += size + (8 - size % 8) % 8;
		return new VarLocation(this, size, -frame_size, name, false);
	}
	
	protected VarLocation createParameter(String name, int size) {
		VarLocation result;
		if (param_count >= 6) {
			result = new VarLocation(this, size, 8+param_stack_size, // 8 for return address on stack
					name, true);
			assert(size <= 8);
			param_stack_size += size + (8 - size % 8) % 8;
		} else
			result = new VarLocation(this, framemanager.getPointerSize(),
				framemanager.getIREnvironment().addRegister(this.name + "::" + name),
				name, true);
		param_count++;
		return result;
	}
	
	protected VarLocation createParentFpParameter(String name) {
		return new VarLocation(this, framemanager.getPointerSize(),
			framemanager.getIREnvironment().addRegister(this.name + "::.parent_fp"),
			name, true);
	}

	Frame(FrameManager _framemanager,
			String _name, int _id, Frame _parent) {
		super(_framemanager, _name, _id, _parent);
	}
	
	int getFrameSize() {
		return frame_size + (24 - frame_size % 16) % 16;
	}
}
