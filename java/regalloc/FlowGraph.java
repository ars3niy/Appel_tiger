package regalloc;

import java.util.LinkedList;
import java.util.ListIterator;
import java.util.TreeMap;

class FlowGraphNode {
	private asm.Instruction instruction;
	
	/**
	 * Instruction outputs without the ignored node
	 */
	private ir.VirtualRegister[] used;
	
	/**
	 * Same as instruction->is_reg_to_reg_assign, except if assignment
	 * source is the ignored register this will be false instead
	 */
	private boolean is_reg_to_reg_assign;

	int index;
	LinkedList<FlowGraphNode> previous = new LinkedList<>(),
	                          next = new LinkedList<>();
	
	FlowGraphNode(int _index, final asm.Instruction _instruction,
			ir.VirtualRegister ignored_register) {
		index = _index;
		instruction = _instruction;
		is_reg_to_reg_assign = instruction.is_reg_to_reg_assign;
		for (ir.VirtualRegister output: instruction.outputs)
			assert output.getIndex() != ignored_register.getIndex();
		
		if (ignored_register == null)
			used = _instruction.inputs;
		else {
			int nreg = 0;
			for (ir.VirtualRegister input: instruction.inputs)
				if (input.getIndex() != ignored_register.getIndex())
					nreg++;
			used = new ir.VirtualRegister[nreg];
			
			nreg = 0;
						
			for (ir.VirtualRegister input: instruction.inputs)
				if (input.getIndex() != ignored_register.getIndex())
					used[nreg++] = input;
				else
					is_reg_to_reg_assign = false;
		}
	}
	
	ir.VirtualRegister[] usedRegisters() {
		return used;
	}
	
	ir.VirtualRegister[] assignedRegisters() {
		return instruction.outputs;
	}
	
	boolean isRegToRegAssignment() {return is_reg_to_reg_assign;}
	
	final ir.Label[] getDestinations() {return instruction.destinations;}
};

class NodeList extends LinkedList<FlowGraphNode> {
	private static final long serialVersionUID = 1;
}

class FlowGraph {
	private NodeList nodes = new NodeList();
	private int nodecount;

	FlowGraphNode last_instruction;
	
	/**
	 * Frame pointer should be ignored_register because it's not
	 * handled by the register allocator
	 */
	FlowGraph(final asm.Instructions code,
			ir.VirtualRegister ignored_register) {
		TreeMap<String, FlowGraphNode> label_positions = new TreeMap<>();
		nodecount = 0;
		FlowGraphNode newnode = null;
		
		for (asm.Instruction inst: code) {
			nodes.add(new FlowGraphNode(nodecount, inst, ignored_register));
			if (inst.label != null)
				label_positions.put(inst.label.getName(), nodes.getLast());
			nodecount++;
		}
		
		last_instruction = newnode;
		
		for (ListIterator<FlowGraphNode> p_node = nodes.listIterator();
				p_node.hasNext(); ) {
			FlowGraphNode current = p_node.next();
			for (ir.Label label: current.getDestinations())  {
				FlowGraphNode nextnode = null;
				if (label == null) {
					if (p_node.hasNext()) {
						nextnode = p_node.next();
						p_node.previous();
					}
				} else
					nextnode = label_positions.get(label.getName());
				
				if (nextnode != null) {
					current.next.add(nextnode);
					nextnode.previous.add(current);
				}
			}
		}
	}
	
	int nodeCount() {return nodecount;}
	final NodeList getNodes() {return nodes;}
};

