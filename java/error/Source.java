package error;

import java.util.LinkedList;

public class Source {
	private LinkedList<SourceLine> content = new LinkedList<>();
	private int linenumber = 1;
	private int charindex = -1;
	private int prev_linenumber = 1;
	private int prev_charindex = -1;
	
	public Source() {
		content.add(new SourceLine(""));
	}
	
	public void add(String s) {
		prev_linenumber = linenumber;
		prev_charindex = charindex;
		content.getLast().line += s;
		charindex += s.length();
	}
	
	public void newline() {
		linenumber++;
		charindex = -1;
		content.add(new SourceLine(""));
	}
	
	public void eof() {
		prev_linenumber = linenumber;
		prev_charindex = charindex;
	}
	
	public int lineNumber() {return linenumber;}
	public int charIndex() {return charindex;}
	public SourceLine line() {return content.getLast();}
	
	public Position currentPosition() {
		return new Position(line(), linenumber, charindex);
	}

	public Position previousPosition() {
		return new Position(content.get(prev_linenumber-1), prev_linenumber, prev_charindex);
	}
}
