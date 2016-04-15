package error;

public class Position {
	private SourceLine m_line;
	private int linenumber;
	private int charindex;
	
	public Position(SourceLine _line, int _linenumber, int _charindex) {
		m_line = _line;
		linenumber = _linenumber;
		charindex = _charindex;
	}
	
	public String line() {return m_line.line;}
	public int lineNumber() {return linenumber;}
	public int charIndex() {return charindex;}
}
