package parse;
import error.Error;

public class BinaryOp extends Node {
	public int operation;
	public Node left, right;
	
	BinaryOp(int _operation, Node _left, Node _right) {
		position = _right.position;
		operation = _operation;
		left = _left;
		right = _right;
		
		switch (_operation) {
		case Parser.SYM_PLUS:
		case Parser.SYM_MINUS:
		case Parser.SYM_ASTERISK:
		case Parser.SYM_SLASH:
		case Parser.SYM_EQUAL:
		case Parser.SYM_NONEQUAL:
		case Parser.SYM_LESS:
		case Parser.SYM_LESSEQUAL:
		case Parser.SYM_GREATER:
		case Parser.SYM_GREATEQUAL:
		case Parser.SYM_ASSIGN:
		case Parser.SYM_AND:
		case Parser.SYM_OR:
			break;
		default:
			Error.current.fatalError("Unexpected operation token", right.position);
		}
	}
}
