package util;

public class IdProvider {
	private int last_id = 0;
	
	public int getId() {return last_id++;}
}
