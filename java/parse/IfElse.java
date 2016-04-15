package parse;

public class IfElse extends Node {
	public Node condition, action, elseaction;
	public boolean converted_from_logic;
	
	IfElse(Node _condition, Node _action, Node _elseaction, boolean fromlogic) {
		condition = _condition;
		action = _action;
		elseaction = _elseaction;
		converted_from_logic = fromlogic;
		position = elseaction.position;
	}
}
