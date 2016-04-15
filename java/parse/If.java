package parse;

public class If extends Node {
	public Node condition, action;
	
	If(Node _condition, Node _action) {
		condition = _condition;
		action = _action;
		position = action.position;
	}
}
