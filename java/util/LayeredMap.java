package util;

import java.util.TreeMap;
import java.util.LinkedList;
import java.util.Iterator;

public class LayeredMap {
	private LinkedList<TreeMap<String, Object>> maps = new LinkedList<>();
	
	public LayeredMap() {
		newLayer();
	}
	
	public void add(String name, Object value) {
		maps.getLast().put(name, value);
	}
	
	public Object lookup(String name) {
		Iterator<TreeMap<String, Object>> i = maps.descendingIterator();
		while (i.hasNext()) {
			TreeMap<String, Object> map = i.next();
			Object obj = map.get(name);
			if (obj != null)
				return obj;
		}
		return null;
	}
	
	public Object lookupLastLayer(String name) {
		return maps.getLast().get(name);
	}
	
	public void newLayer() {
		maps.add(new TreeMap<>());
	}
	
	public void removeLastLayer() {
		assert ! maps.isEmpty();
		maps.removeLast();
	}
}
