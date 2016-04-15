package parse;
import error.Error;

public class Node {
	public error.Position position;
	public ir.Code code = null;
	
	protected Node() {
		position = Error.current.currentPosition();
	}
}
