package regalloc;

import java.io.PrintWriter;
public class Allocator {
	public static void PrintLivenessInfo(PrintWriter f, final asm.Instructions code,
			final frame.AbstractFrame frame) {
		FlowGraph graph = new FlowGraph(code, frame.getFramePointer().getRegister());
		LivenessInfo liveness = new LivenessInfo(graph);
		
		for (int i = 0; i < graph.nodeCount(); i++) {
			f.printf("Line %d:", i);
			for (int v: liveness.live_after_node[i])
				f.printf(" %s", liveness.virtuals[v].getName());
			f.println();
		}
	}
	
	public static void AssignRegisters(asm.Instructions code,
			asm.Assembler assembler,
			frame.AbstractFrame frame,
			final ir.VirtualRegister[] machine_registers,
			ir.RegisterMap id_to_machine_map) {
		FlowGraph graph = null;
		LivenessInfo liveness = null;
		PartialRegAllocator allocator = null;
		int[] colors = null;
		Intlist spilled = null;
		
		do {
			graph = null;
			liveness = null;
			allocator = null;
			graph = new FlowGraph(code, frame.getFramePointer().getRegister());
			liveness = new LivenessInfo(graph);
			allocator = new PartialRegAllocator(liveness, machine_registers.length,
				frame.getName());
			allocator.debug(String.format("%d colors", machine_registers.length));
			for (int i = 0; i < machine_registers.length; i++)
				allocator.debug(String.format("%d: %s", i, machine_registers[i].getName()));
			for (int i = 0; i < machine_registers.length; i++) {
				int index = liveness.getVirtualRegisterIndex(machine_registers[i]);
				if (index >= 0) {
					allocator.precolor(index, i);
				}
			}
			allocator.tryColoring();
			spilled = allocator.getSpilled();
			colors = allocator.getColors();
			
			if (! spilled.isEmpty()) {
				allocator.debug("================================ SPILLING AND RESTARTING");
			}
			
			for (int n: spilled)
				assembler.spillRegister(frame, code, liveness.virtuals[n]);
		} while (! spilled.isEmpty());
		
			assert(colors.length == liveness.virtuals.length);
			for (int i = 0; i < colors.length; i++) {
				assert(colors[i] < (int)machine_registers.length);
				if (spilled.isEmpty())
					assert(colors[i] >= 0);
				if (colors[i] >= 0)
					id_to_machine_map.map(liveness.virtuals[i],
						machine_registers[colors[i]]);
			}

		graph = null;
		liveness = null;
		allocator = null;
	}

}
