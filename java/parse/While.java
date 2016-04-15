package parse;

public class While extends Node {
	public Node condition, action;
	
	While(Node _condition, Node _action) {
		condition = _condition;
		action = _action;
		position = action.position;
	}
}
