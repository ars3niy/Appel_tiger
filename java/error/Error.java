package error;

import java.util.LinkedList;

class ErrorMessage {
	Position pos;
	String kind;
	String message;
	boolean iserror;
	
	ErrorMessage(Position _pos, String _kind, String _msg, boolean _iserror) {
		pos = _pos;
		kind = _kind;
		message = _msg;
		iserror = _iserror;
	}
}

public class Error extends Source {
	
	public static Error current;
	
	private String filename;
	private int errorcount = 0;
	private LinkedList<ErrorMessage> messages = new LinkedList<>();
	
	public Error(String _filename) {
		filename = _filename;
		current = this;
	}
	
	public int errorCount() {return errorcount;}
	
	public void error(String message) {
		error(message, currentPosition());
	}
	
	public void fatalError(String message) {
		fatalError(message, currentPosition());
	}
	
	public void error(String message, Position pos) {
		addError("Error", message, pos, true);
	}
	
	public void warning(String message, Position pos) {
		addError("Warning", message, pos, false);
	}
	
	public void fatalError(String message, Position pos) {
		addError("Fatal error", message, pos, true);
		eof();
		System.exit(2);
	}
	
	public void globalError(String message) {
		System.out.println("Error: " + message);
		errorcount++;
	}
	
	private void addError(String kind, String message, Position pos, boolean iserror) {
		messages.add(new ErrorMessage(pos, kind, message, iserror));
	}
	
	public void flush() {
		while (! messages.isEmpty()) {
			doError(messages.getFirst().kind, messages.getFirst().message,
					messages.getFirst().pos, messages.getFirst().iserror);
			messages.removeFirst();
		}
	}
	
	public void newline() {
		flush();
		super.newline();
	}
	
	public void eof() {
		flush();
		super.eof();
	}
	
	private void doError(String kind, String message, Position pos, boolean iserror) {
		if (iserror)
			errorcount++;
		if (pos.line().length() > 0) {
			System.out.println(pos.line());
			for (int i = 0; i < pos.charIndex(); i++)
				System.out.print(' ');
			System.out.println('^');
		}
		System.out.println(String.format("%s %s, line %d: %s", filename, kind, pos.lineNumber(), message));
	}
}
