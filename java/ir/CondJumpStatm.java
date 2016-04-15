package ir;

public class CondJumpStatm extends Statement {
	public static final int
		EQUAL = 0,
		NONEQUAL = 1,
		LESS = 2,
		LESSEQUAL = 3,
		GREATER = 4,
		GREATEQUAL = 5,
		ULESS = 6,
		ULESSEQUAL = 7,
		UGREATER = 8,
		UGREATEQUAL = 9,
		COMPMAX = 10;
	
	public static final int INVERSE[] = {
		NONEQUAL,
		EQUAL,
		GREATEQUAL,
		GREATER,
		LESSEQUAL,
		LESS,
		UGREATEQUAL,
		UGREATER,
		ULESSEQUAL,
		ULESS
	};
	
	public int comparison;
	public Expression left, right;
	private Label[] true_dest = new Label[1];
	private Label[] false_dest = new Label[1];
	
	public CondJumpStatm(int _comp, Expression _left, Expression _right,
			Label _true_dest, Label _false_dest) {
		comparison = _comp;
		left = _left;
		right = _right;
		true_dest[0] = _true_dest;
		false_dest[0] = _false_dest;
	}
	
	public Label trueDest() {return true_dest[0];}
	public Label[] trueDestRef() {return true_dest;}
	public Label falseDest() {return false_dest[0];}
	public Label[] falseDestRef() {return false_dest;}
	
	public void flip() {
		Label tmp = true_dest[0];
		true_dest[0] = false_dest[0];
		false_dest[0] = tmp;
		comparison = INVERSE[comparison];
	}
	
}
