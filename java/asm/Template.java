package asm;

import java.util.LinkedList;
import java.util.Vector;

class InstructionTemplate {
	ir.Code code;

	InstructionTemplate(ir.Expression exp) {
		code = new ir.ExpCode(exp);
	}
	
	InstructionTemplate(ir.Statement exp) {
		code = new ir.StatmCode(exp);
	}
	
	public InstructionTemplate(ir.Code _code) {
		code = _code;
	}
}

class TemplateStorage {
}

class TemplateList extends TemplateStorage {
	LinkedList<InstructionTemplate> list = new LinkedList<>();
}

class TemplateOpMap extends TemplateStorage {
	private Vector<LinkedList<InstructionTemplate>> map = new Vector<>();
	
	TemplateOpMap(int op_count) {
		map.setSize(op_count);
		for (int i = 0; i < op_count; i++)
			map.set(i, new LinkedList<>());
	}
	
	LinkedList<InstructionTemplate> getList(int op) {
		return map.get(op);
	}
}
