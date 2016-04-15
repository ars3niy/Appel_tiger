package ir;

import java.util.Vector;

public class RegisterMap {
	private Vector<VirtualRegister> mapping;
	
	public void map(VirtualRegister from, VirtualRegister to) {
		if (mapping.size() <= from.getIndex()) {
			int oldsize = mapping.size();
			mapping.setSize(from.getIndex()+1);
			for (int i = oldsize; i < mapping.size(); i++)
				mapping.set(i,  null);
		}
		mapping.set(from.getIndex(), to);
	}
	
	public VirtualRegister get(VirtualRegister from) {
		if ((mapping.size() > from.getIndex())) {
			VirtualRegister mapped = mapping.elementAt(from.getIndex());
			if (mapped != null)
				return mapped;
		}
		return from;
	}
	
	public void merge(final RegisterMap merge_from) {
		if (mapping.size() < merge_from.mapping.size()) {
			int oldsize = mapping.size();
			mapping.setSize(merge_from.mapping.size());
			for (int i = oldsize; i < mapping.size(); i++)
				mapping.set(i,  null);
		}
		for (int i = 0; i < merge_from.mapping.size(); i++)
			if (merge_from.mapping.elementAt(i) != null) {
				if (mapping.elementAt(i) != null)
					assert(merge_from.mapping.elementAt(i).getIndex() ==
						mapping.elementAt(i).getIndex());
				mapping.set(i, merge_from.mapping.elementAt(i));
			}
	}
	
	RegisterMap(int reg_count) {
		mapping = new Vector<>(reg_count);
		mapping.setSize(reg_count);
		for (int i = 0; i < reg_count; i++)
			mapping.set(i, null);
	}
}
