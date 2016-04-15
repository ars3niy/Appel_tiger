package parse;

public class ArrayIndexing extends Node {
	public Node array, index;
	
	ArrayIndexing(Node _array, Node _index) {
		array = _array;
		index = _index;
	}
}
