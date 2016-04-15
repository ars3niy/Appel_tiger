package regalloc;

import java.util.LinkedList;
import java.util.TreeMap;

class VarList extends LinkedList<Integer> {
	private static final long serialVersionUID = 1L;
}

public class LivenessInfo extends util.DebugPrinter {
	private NodeList[] nodes_using;
	private int[] number_invocations; // uses plus assigns for each virtual register

	private class VirtualRegInfo {
		int index; // int this->virtuals
		ir.VirtualRegister reg;
		
		VirtualRegInfo(int i, ir.VirtualRegister r) {
			index = i;
			reg = r;
		}
	}
	
	private TreeMap<Integer, VirtualRegInfo> virt_indices_by_id = new TreeMap<>();
	private int max_virt_reg_id;

	ir.VirtualRegister[] virtuals;
	int nodecount;
	VarList[] live_after_node;
	int[][] used_at_node;
	int[][] assigned_at_node;
	boolean[] node_is_reg_reg_move;
	
	boolean isLiveAfterNode(final FlowGraphNode node, int var) {
		return isLiveAfterNode(node.index, var);
	}
	
	boolean isLiveAfterNode(int node, int var) {
		final VarList vars = live_after_node[node];
		for (int v: vars)
			if (v == var)
				return true;
		return false;
	}
	
	boolean isAssignedAtNode(final FlowGraphNode node, int var) {
		for (int v: assigned_at_node[node.index])
			if (v == var)
				return true;
		return false;
	}
	
	int getInvocationCount(int node) {
		return number_invocations[node];
	}
	
	int getVirtualRegisterIndex(ir.VirtualRegister vreg) {
		VirtualRegInfo reginfo = virt_indices_by_id.get(vreg.getIndex());
		if (reginfo == null)
			return -1;
		else
			return reginfo.index;
	}
	
	int getMaxVirtualRegisterId() {
		return max_virt_reg_id;
	}
	
	private void findLiveness(int virt_reg, final FlowGraphNode node) {
		for (FlowGraphNode prev: node.previous) {
			if (! isLiveAfterNode(prev, virt_reg)) {
				live_after_node[prev.index].add(virt_reg);
				if (! isAssignedAtNode(prev, virt_reg))
					findLiveness(virt_reg, prev);
			}
		}
	}
	
	LivenessInfo(final FlowGraph flowgraph) {
		super("liveness.log");
		nodecount = flowgraph.nodeCount();
		int n_virtreg = 0;
		used_at_node = new int[nodecount][];
		assigned_at_node = new int[nodecount][];
		node_is_reg_reg_move = new boolean[nodecount];
		java.util.Arrays.fill(node_is_reg_reg_move, false);
		max_virt_reg_id = -1;
		LinkedList<VirtualRegInfo> reginfos = new LinkedList<>();
		
		final NodeList nodes = flowgraph.getNodes();	
		for (final FlowGraphNode node: nodes) {
			debug(String.format("node %d, %d next, %d prev", node.index,
				  node.next.size(), node.previous.size()));
			{
				final ir.VirtualRegister[] regs = node.usedRegisters();
				if (node.isRegToRegAssignment())
					assert(regs.length == 1);
				node_is_reg_reg_move[node.index] = node.isRegToRegAssignment();
				used_at_node[node.index] = new int[regs.length];
				for (int i = 0; i < regs.length; i++) {
					if (virt_indices_by_id.get(regs[i].getIndex()) == null) {
						VirtualRegInfo newinfo = new VirtualRegInfo(n_virtreg, regs[i]);
						reginfos.add(newinfo);
						virt_indices_by_id.put(regs[i].getIndex(), newinfo);
						if (regs[i].getIndex() > max_virt_reg_id)
							max_virt_reg_id = regs[i].getIndex();
						n_virtreg++;
					}
					used_at_node[node.index][i] =
						virt_indices_by_id.get(regs[i].getIndex()).index;
				}
			}
			
			{
				ir.VirtualRegister[] regs = node.assignedRegisters();
				assigned_at_node[node.index] = new int[regs.length];
				for (int i = 0; i < regs.length; i++) {
					if (virt_indices_by_id.get(regs[i].getIndex()) == null) {
						VirtualRegInfo newinfo = new VirtualRegInfo(n_virtreg, regs[i]);
						reginfos.add(newinfo);
						virt_indices_by_id.put(regs[i].getIndex(), newinfo);
						if (regs[i].getIndex() > max_virt_reg_id)
							max_virt_reg_id = regs[i].getIndex();
						n_virtreg++;
					}
					assigned_at_node[node.index][i] =
						virt_indices_by_id.get(regs[i].getIndex()).index;
				}
			}
		}
		
		live_after_node = new VarList[flowgraph.nodeCount()];
		for (int i = 0; i < live_after_node.length; i++)
			live_after_node[i] = new VarList();
		nodes_using = new NodeList[n_virtreg];
		for (int i = 0; i < nodes_using.length; i++)
			nodes_using[i] = new NodeList();
		
		number_invocations = new int[n_virtreg];
		java.util.Arrays.fill(number_invocations, 0);
		virtuals = new ir.VirtualRegister[n_virtreg];
		
		for (VirtualRegInfo reg: reginfos)
			virtuals[reg.index] = reg.reg;
		
		for (final FlowGraphNode node: nodes) {
			for (ir.VirtualRegister reg: node.usedRegisters()) {
				int index = virt_indices_by_id.get(reg.getIndex()).index;
				nodes_using[index].add(node);
				number_invocations[index]++;
			}
			for (ir.VirtualRegister reg: node.assignedRegisters()) {
				int index = virt_indices_by_id.get(reg.getIndex()).index;
				number_invocations[index]++;
			}
		}
		
		for (int i = 0; i < n_virtreg; i++)
			for (final FlowGraphNode node: nodes_using[i])
				findLiveness(i, node);
	}
}
