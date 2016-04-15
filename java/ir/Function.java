package ir;

public class Function {
	public String name;
	public Code body;
	public frame.AbstractFrame frame;
	public Label label;
	
	public Function(String _name, Code _body, frame.AbstractFrame _frame,
			Label _label) {
		name = _name;
		body = _body;
		frame = _frame;
		label = _label;
	}
	
}
