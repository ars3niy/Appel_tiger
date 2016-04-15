package parse;

public class For extends Node {
	public Identifier variable;
	public Node start, stop, action;
	/**
	 * Id for the loop variable
	 */
	public int variable_id;
	
	For(Node _variable, Node _start, Node _stop, Node _action, int _var_id) {
		variable = (Identifier)_variable;
		start = _start;
		stop = _stop;
		action = _action;
		variable_id = _var_id;
		position = action.position;
	}
}
