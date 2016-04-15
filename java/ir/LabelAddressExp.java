package ir;

public class LabelAddressExp extends Expression {
	private Label[] label = new Label[1];
	
	public LabelAddressExp(Label _label) {
		label[0] = _label;
	}
	
	public Label label() {return label[0];}
	
	public Label[] labelRef() {return label;}
}
