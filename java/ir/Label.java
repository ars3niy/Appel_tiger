package ir;

public class Label {
	private int index;
	private String name;
	
	public Label(int _index) {
		index = _index;
		name = String.format(".L%d", _index);
	}
	
	public Label(int _index, String _name) {
		index = _index;
		name = _name;
	}
	
	public int getIndex() {return index;}
	
	public String getName() {return name;}
	
	public void appendToName(String s) {
		name += s;
	}
}
