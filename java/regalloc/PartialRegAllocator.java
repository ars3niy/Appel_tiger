package regalloc;

import java.util.Iterator;
import java.util.LinkedList;
import error.Error;

class Intlist extends LinkedList<Integer> {
	private static final long serialVersionUID = 1L;
	
	void ListRemove(int value)
	{
		for (Iterator<Integer> x = iterator(); x .hasNext(); )
			if (x.next() == value) {
				x.remove();
				return;
			}
		assert(false);
	}

	void ListRemoveReverse(int value)
	{
		for (Iterator<Integer >x = descendingIterator(); x.hasNext(); ) {
			if (x.next() == value) {
				x.remove();
				return;
			}
		}
		assert(false);
	}

}

enum NodeStatus {
	UNPROCESSED,
	PRECOLORED,
	REMOVABLE,
	FREEZEABLE,
	SPILLABLE,
	SPILLED,
	COALESCED,
	COLORED,
	SELECTED,
};

enum MoveStatus {
	UNPROCESSED,
	COALESCED,
	CONSTRAINED,
	FROZEN,
	COALESCABLE,
	ACTIVE,
};

class NodeMoveManager {
	private Intlist[] node_lists = new Intlist[NodeStatus.values().length];
	private NodeStatus[] node_status;
	
	private Intlist[] move_lists = new Intlist[MoveStatus.values().length];
	private MoveStatus[] move_status;
	
	void init(int nodecount, int movecount) {
		node_status = new NodeStatus[nodecount];
		move_status = new MoveStatus[movecount];
		for (int i = 0; i < node_lists.length; i++)
			node_lists[i] = new Intlist();
		for (int i = 0; i < move_lists.length; i++)
			move_lists[i] = new Intlist();
	}
	
	NodeStatus nodeStatus(int n) {return node_status[n];}
	MoveStatus moveStatus(int m) {return move_status[m];}
	
	final Intlist getNodes(NodeStatus status) {
		return node_lists[status.ordinal()];
	}
	
	final Intlist getMoves(MoveStatus status) {
		return move_lists[status.ordinal()];
	}
	
	void setNodeStatus(int n, NodeStatus status) {
		if (node_status[n] == NodeStatus.SELECTED)
			node_lists[node_status[n].ordinal()].ListRemoveReverse(n);
		else
			node_lists[node_status[n].ordinal()].ListRemove(n);
		node_status[n] = status;
		node_lists[status.ordinal()].add(n);
	}

	void setMoveStatus(int m, MoveStatus status) {
		move_lists[move_status[m].ordinal()].ListRemove(m);
		move_status[m] = status;
		move_lists[status.ordinal()].add(m);
	}
	
	void addNode(int n, NodeStatus status)
	{
		node_status[n] = status;
		node_lists[status.ordinal()].add(n);
	}
	
	void addMove(int m, MoveStatus status)
	{
		move_status[m] = status;
		move_lists[status.ordinal()].add(m);
	}
};

class PartialRegAllocator extends util.DebugPrinter {
	private LivenessInfo liveness;
	
	private boolean[] table;
	private Intlist[] list;
	private boolean[] no_list;
	private boolean[] used_in_moves;
	private int nodecount;
	private int colorcount;

	private void setNoAdjacencyList(int n) {
		list[n].clear();
		no_list[n] = true;
	}
	
	private void connect(int n1, int n2) {
		assert(n1 != n2);
		if (! table[n1*nodecount+n2] && ! table[n2*nodecount+n1]) {
			table[n1*nodecount+n2] = true;
			table[n2*nodecount+n1] = true;
			if (! no_list[n1]) {
				list[n1].add(n2);
				node_degrees[n1]++;
			}
			if (! no_list[n2]) {
				list[n2].add(n1);
				node_degrees[n2]++;
			}
		}
	}
	
	private boolean isAdjacent(int n1, int n2) {
		return table[n1*nodecount+n2];
	}
	
	private final Intlist getAdjacent(int n) {
		assert(! no_list[n]);
		return list[n];
	}
	
	private NodeMoveManager status = new NodeMoveManager();
	
	private int[] node_degrees, original_degrees;
	private Intlist[] moves_per_node;
	private int[] coalesced_with;
	private int[] colors;
	
	private final String NodeName(int n)
	{
		return liveness.virtuals[n].getName();
	}
	
	private void buildGraph() {
		for (int i = 0; i < liveness.nodecount; i++) {
			int assignment_source = -1;
			if (liveness.node_is_reg_reg_move[i]) {
				assert(liveness.used_at_node[i].length == 1);
				assert(liveness.assigned_at_node[i].length == 1);
				
				int r1 = liveness.used_at_node[i][0];
				int r2 = liveness.assigned_at_node[i][0];
				if (r1 != r2) {
					debug(String.format("move %s -> %s", NodeName(r1), NodeName(r2)));
					assignment_source = r1;
					used_in_moves[r1] = true;
					used_in_moves[r2] = true;
					status.addMove(i, MoveStatus.COALESCABLE);
					moves_per_node[r1].add(i);
					moves_per_node[r2].add(i);
				}
			}
			
			for (int assign_ind = 0; assign_ind < liveness.assigned_at_node[i].length;
					assign_ind++) {
				int assigned_here = liveness.assigned_at_node[i][assign_ind];
				//if (liveness.isLiveAfterNode(i, assigned_here))
					for (int livehere: liveness.live_after_node[i]) {
						if ((livehere != assigned_here) &&
								(livehere != assignment_source)) {
							debug(String.format(
									"Line %d, Assigned node %s, collides with live node %s",
								  i, NodeName(assigned_here),
								  NodeName(livehere)));
							connect(assigned_here, livehere);
						}
					}
			}
		}
		original_degrees = new int[nodecount];
		for (int i = 0; i < nodecount; i++)
			original_degrees[i] = node_degrees[i];
	}
	
	private void classifyNodes() {
		for (int n = 0; n < nodecount; n++)
			if (colors[n] < 0) {
				if (node_degrees[n] >= colorcount) {
					debug(String.format("Node %s is SPILLABLE", NodeName(n)));
					status.addNode(n, NodeStatus.SPILLABLE);
				} else if (used_in_moves[n]) {
					debug(String.format("Node %s is FREEZEABLE", NodeName(n)));
					status.addNode(n, NodeStatus.FREEZEABLE);
				} else
					status.addNode(n, NodeStatus.REMOVABLE);
			}
		printStatus();
	}
	
	private void getRemainingAdjacent(int n, Intlist nodes) {
		final Intlist core_list = getAdjacent(n);
		nodes.clear();
		for (int node: core_list)
			if ((status.nodeStatus(node) != NodeStatus.SELECTED) && (status.nodeStatus(node) != NodeStatus.COALESCED))
				nodes.add(node);
	}
	
	private boolean hasRemainingMoves(int n) {
		for (int move: moves_per_node[n])
			if ((status.moveStatus(move) == MoveStatus.COALESCABLE) ||
				(status.moveStatus(move) == MoveStatus.ACTIVE))
				return true;
		return false;
	}
	
	private void getRemainingMoves(int n, Intlist moves) {
		moves.clear();
		for (int move: moves_per_node[n])
			if ((status.moveStatus(move) == MoveStatus.COALESCABLE) ||
				(status.moveStatus(move) == MoveStatus.ACTIVE))
				moves.add(move);
	}
	
	private void remove_one_removable() {
		if (status.getNodes(NodeStatus.REMOVABLE).isEmpty())
			return;
		int n = status.getNodes(NodeStatus.REMOVABLE).getFirst();
		debug(String.format("Removing node %s", NodeName(n)));
		status.setNodeStatus(n, NodeStatus.SELECTED);
		Intlist adjacent = new Intlist();
		getRemainingAdjacent(n, adjacent);
		for (int n2: adjacent)
			decrementDegree(n2);
	}
	
	private void decrementDegree(int n) {
		if (status.nodeStatus(n) == NodeStatus.PRECOLORED)
			return;
		debug(String.format("decrementDegree: node %s is initially %d", 
			  NodeName(n), node_degrees[n]));
		node_degrees[n]--;
		if (node_degrees[n] == colorcount-1) {
			Intlist neighbours = new Intlist();
			getRemainingAdjacent(n, neighbours);
			neighbours.addFirst(n);
			enableMoves(neighbours);
			assert(status.nodeStatus(n) == NodeStatus.SPILLABLE);
			if (hasRemainingMoves(n)) {
				debug(String.format("DecrementDegree: setting node %s to freezeable",
					NodeName(n)));
				status.setNodeStatus(n, NodeStatus.FREEZEABLE);
			} else {
				debug(String.format("DecrementDegree: Can now remove node %s", NodeName(n)));
				status.setNodeStatus(n, NodeStatus.REMOVABLE);
			}
		}
	}
	
	private void enableMoves(final Intlist nodes) {
		for (int node: nodes) {
			Intlist moves = new Intlist();
			getRemainingMoves(node, moves);
			for (int move: moves) {
				if (status.moveStatus(move) == MoveStatus.ACTIVE)
					status.setMoveStatus(move, MoveStatus.COALESCABLE);
			}
		}
	}
	
	private int getRemaingFromCoalescedGroup(int n) {
		if (status.nodeStatus(n) == NodeStatus.COALESCED)
			return getRemaingFromCoalescedGroup(coalesced_with[n]);
		else
			return n;
	}
	
	private void coalesce_one() {
		if (status.getMoves(MoveStatus.COALESCABLE).isEmpty())
			return;
		int move = status.getMoves(MoveStatus.COALESCABLE).getFirst();
		int n1 = liveness.used_at_node[move][0];
		int n2 = liveness.assigned_at_node[move][0];
		debug(String.format("coalesce_one: merging %s and %s",
			NodeName(n1),
			NodeName(n2)));
		
		n1 = getRemaingFromCoalescedGroup(n1);
		n2 = getRemaingFromCoalescedGroup(n2);
		
		if (status.nodeStatus(n2) == NodeStatus.PRECOLORED) {
			int x = n1;
			n1 = n2;
			n2 = x;
		}
		
		if (n1 == n2) {
			debug("Already coalesced");
			status.setMoveStatus(move, MoveStatus.COALESCED);
			possiblyUnfreeze(n1);
		} else if ((status.nodeStatus(n2) == NodeStatus.PRECOLORED) ||
				isAdjacent(n1, n2)) {
			if (status.nodeStatus(n2) == NodeStatus.PRECOLORED)
				debug("Cannot coalesce, two precolored nodes");
			else
				debug("Cannot coalesce, two interfering nodes");
			status.setMoveStatus(move, MoveStatus.CONSTRAINED);
			possiblyUnfreeze(n1);
			possiblyUnfreeze(n2);
		} else if (
				(status.nodeStatus(n1) == NodeStatus.PRECOLORED) &&
					canCoalesce_noAdjacentListfor1(n1, n2) ||
				(status.nodeStatus(n1) != NodeStatus.PRECOLORED) &&
					canCoalesce_bothAdjacentLists(n1, n2)
			   ) {
			debug("Safe to coalesce, proceed");
			status.setMoveStatus(move, MoveStatus.COALESCED);
			combineNodes(n1, n2);
			possiblyUnfreeze(n1);
		} else {
			debug("Afraid to coalesce, maybe later");
			status.setMoveStatus(move, MoveStatus.ACTIVE);
		}
	}
	
	private void possiblyUnfreeze(int n) {
		if ((status.nodeStatus(n) != NodeStatus.PRECOLORED) && ! hasRemainingMoves(n) &&
				(node_degrees[n] < colorcount)) {
			assert(status.nodeStatus(n) == NodeStatus.FREEZEABLE);
			status.setNodeStatus(n, NodeStatus.REMOVABLE);
		}
	}
	
	private boolean canCoalesce_noAdjacentListfor1(int n1, int n2) {
		Intlist adjacent2 = new Intlist();
		getRemainingAdjacent(n2, adjacent2);
		for (int neighbour2: adjacent2) {
			if (!(
				(node_degrees[neighbour2] < colorcount) ||
				(status.nodeStatus(neighbour2) == NodeStatus.PRECOLORED) ||
				isAdjacent(neighbour2, n1)
			)) {
				debug(String.format("Afraid to coalesce %s and %s",
					NodeName(n1), NodeName(n2)));
				debug(String.format(
						"... because %s has neighbour %s of degree %d not precolored and not adjacent to %s",
						NodeName(n2), NodeName(neighbour2), node_degrees[neighbour2],
						NodeName(n1)));
				return false;
			}
		}
		return true;
	}
	
	private boolean canCoalesce_bothAdjacentLists(int n1, int n2) {
		Intlist adjacent1 = new Intlist(), adjacent2 = new Intlist();
		getRemainingAdjacent(n1, adjacent1);
		getRemainingAdjacent(n2, adjacent2);
		int n_bigdegree = 0;
		for (int neighbour: adjacent1)
			if (node_degrees[neighbour] >= colorcount)
				n_bigdegree++;
		for (int neighbour: adjacent2)
			if (node_degrees[neighbour] >= colorcount)
				n_bigdegree++;
		return n_bigdegree < colorcount;
	}
	
	private void combineNodes(int remain, int remove) {
		assert((status.nodeStatus(remove) == NodeStatus.FREEZEABLE) ||
				(status.nodeStatus(remove) == NodeStatus.SPILLABLE));
		status.setNodeStatus(remove, NodeStatus.COALESCED);
		coalesced_with[remove] = remain;
		Intlist removeds_moves = new Intlist();
		getRemainingMoves(remove, removeds_moves);
		for (int move: removeds_moves)
			moves_per_node[remain].add(move);
		
		Intlist adjacent = new Intlist();
		getRemainingAdjacent(remove, adjacent);
		for (int neighbour: adjacent) {
			if (! isAdjacent(neighbour, remain)) {
				debug(String.format(
					"combineNodes: Connecting %s with %s, degrees initially %d and %d",
					NodeName(neighbour),
					NodeName(remain),
					node_degrees[neighbour], node_degrees[remain]
					));
				connect(neighbour, remain);
				//decrementDegree(*neighbour);
				if (status.nodeStatus(neighbour) != NodeStatus.PRECOLORED)
					node_degrees[neighbour]--;
				debug(String.format(
					"combineNodes: Connected, degrees are now %s = %d  %s = %d",
					NodeName(neighbour),
					node_degrees[neighbour], 
					NodeName(remain),
					node_degrees[remain]
					));
			}
		}
		if ((node_degrees[remain] >= colorcount) &&
				status.nodeStatus(remain) == NodeStatus.FREEZEABLE) {
			debug(String.format(
					"CombineNodes: node %s has now degree %d thus SPILLABLE",
				  NodeName(remain), node_degrees[remain]));
			status.setNodeStatus(remain, NodeStatus.SPILLABLE);
		}
	}
	
	private void freezeOne() {
		if (status.getNodes(NodeStatus.FREEZEABLE).isEmpty())
			return;
		int node = status.getNodes(NodeStatus.FREEZEABLE).getFirst();
		status.setNodeStatus(node, NodeStatus.REMOVABLE);
		freezeMoves(node);
	}
	
	private void freezeMoves(int node) {
		debug(String.format("freezeMoves for node %s", NodeName(node)));
		Intlist moves = new Intlist();
		getRemainingMoves(node, moves);
		for (int move: moves) {
			int n1 = getRemaingFromCoalescedGroup(liveness.used_at_node[move][0]);
			int n2 = getRemaingFromCoalescedGroup(liveness.assigned_at_node[move][0]);
			int node2;
			if (n2 == getRemaingFromCoalescedGroup(node))
				node2 = n1;
			else
				node2 = n2;
			debug(String.format("Freezing move %s -> %s",
				NodeName(liveness.used_at_node[move][0]),
				NodeName(liveness.assigned_at_node[move][0])));
			assert(status.moveStatus(move) == MoveStatus.ACTIVE);
			status.setMoveStatus(move, MoveStatus.FROZEN);
			if (! hasRemainingMoves(node2) && (node_degrees[node2] < colorcount) &&
					(status.nodeStatus(node2) != NodeStatus.PRECOLORED)) {
				assert(status.nodeStatus(node2) == NodeStatus.FREEZEABLE);
				status.setNodeStatus(node2, NodeStatus.REMOVABLE);
			}
		}
	}
	
	private float getSpillBadness(int node) {
		return (float)liveness.getInvocationCount(node) / original_degrees[node];
	}
	
	private void prespillOne() {
		int node_to_spill = -1;
		float best_badness = 1e6f;
		
		for (int node: status.getNodes(NodeStatus.SPILLABLE)) {
			float badness = getSpillBadness(node);
			debug(String.format("Spillable node %s, badness %g",
				  NodeName(node), badness));
			if ((node_to_spill < 0) || (badness < best_badness)) {
				node_to_spill = node;
				best_badness = badness;
			}
		}
		debug(String.format("Spilling node %s, (badness %g)",
				NodeName(node_to_spill),
				best_badness));
		status.setNodeStatus(node_to_spill, NodeStatus.REMOVABLE);
		freezeMoves(node_to_spill);
	}
	
	private void assignColors() {
		while (! status.getNodes(NodeStatus.SELECTED).isEmpty()) {
			int n = status.getNodes(NodeStatus.SELECTED).getLast();
			
			boolean[] colors_available = new boolean[colorcount];
			java.util.Arrays.fill(colors_available, true);
			
			Intlist neighbours = list[n];
			for (int neighbour: neighbours) {
				int uncoalesced = getRemaingFromCoalescedGroup(neighbour);
				if ((status.nodeStatus(uncoalesced) == NodeStatus.PRECOLORED) ||
						(status.nodeStatus(uncoalesced) == NodeStatus.COLORED))
					colors_available[colors[uncoalesced]] = false;
			}
			
			int next_color = -1;
			for (int i = 0; i < colorcount; i++)
				if (colors_available[i]) {
					next_color = i;
					break;
				}
			if (next_color < 0)
				status.setNodeStatus(n, NodeStatus.SPILLED);
			else {
				status.setNodeStatus(n, NodeStatus.COLORED);
				colors[n] = next_color;
			}
		}
		for (int coalesced: status.getNodes(NodeStatus.COALESCED))
			colors[coalesced] = colors[getRemaingFromCoalescedGroup(coalesced)];
	}
	
	private void printRegisters() {
		
	}
	private void printColors() {
		debug(String.valueOf(colorcount) + " colors");
	}
	
	private final String[] node_status_names = {
		"unprocessed",
		"precolored",
		"removable",
		"freezeable",
		"spillable",
		"spilled",
		"coalesced",
		"colored",
		"selected",
	};

	private final String[] move_status_names = {
		"unprocessed",
		"coalesced",
		"finalrained",
		"frozen",
		"coalescable",
		"active",
	};
	private void printStatus() {
		for (NodeStatus ns: NodeStatus.values()) {
			debug(node_status_names[ns.ordinal()] + " nodes:");
			final Intlist nodes = status.getNodes(ns);
			for (int n: nodes) {
				String message = NodeName(n);
				if ((ns == NodeStatus.COLORED) || (ns == NodeStatus.PRECOLORED)) {
					message = message + ", color " + String.valueOf(colors[n]);
				}
				if (ns != NodeStatus.PRECOLORED)
					message = message + ", degree " + String.valueOf(node_degrees[n]);
				message = message + " interferes with";
				for (int n2 = 0; n2 < nodecount; n2++)
					if ((n2 != n) && isAdjacent(n, n2))
						message = message + " " + NodeName(n2);
				debug(message);
			}
		}
		for (MoveStatus ms : MoveStatus.values()) {
			debug(move_status_names[ms.ordinal()] + " moves:");
			final Intlist moves = status.getMoves(ms);
			for (int m: moves) {
				debug(String.format("%d: %s -> %s", m,
					NodeName(liveness.used_at_node[m][0]),
					NodeName(liveness.assigned_at_node[m][0])));
			}
		}
	}
	
	private void checkColors() {
		for (int i = 0; i < liveness.nodecount; i++) {
			int assignment_source = -1;
			if (liveness.node_is_reg_reg_move[i]) {
				assert(liveness.used_at_node[i].length == 1);
				assert(liveness.assigned_at_node[i].length == 1);
				
				int r1 = liveness.used_at_node[i][0];
				int r2 = liveness.assigned_at_node[i][0];
				if (r1 != r2) { // TODO: eliminate useless moves somehow
					assignment_source = r1;
				}
			}
			for (int assign_ind = 0; assign_ind < liveness.assigned_at_node[i].length;
					assign_ind++) {
				int assigned_here = liveness.assigned_at_node[i][assign_ind];
				if (liveness.isLiveAfterNode(i, assigned_here))
					for (int livehere: liveness.live_after_node[i]) {
						if ((livehere != assigned_here) &&
								(livehere != assignment_source)) {
							if ((colors[livehere] >= 0) &&
									(colors[assigned_here] >= 0) &&
									(colors[livehere] == colors[assigned_here]))
								Error.current.fatalError(
									"Colliding nodes " + NodeName(livehere) +
									" and " + NodeName(assigned_here) +
									" colored the same");
						}
					}
			}
		}
	}
	
	public PartialRegAllocator(final LivenessInfo _liveness, int _colorcount,
			String funcname) {
		super("allocator_" + funcname + ".log");
		liveness = _liveness;
		colorcount = _colorcount;

		nodecount = liveness.virtuals.length;
		table = new boolean[nodecount*nodecount];
		java.util.Arrays.fill(table,  false);
		list = new Intlist[nodecount];
		for (int i = 0; i < nodecount; i++)
			list[i] = new Intlist();
		no_list = new boolean[nodecount];
		java.util.Arrays.fill(no_list,  false);
		used_in_moves = new boolean[nodecount];
		java.util.Arrays.fill(used_in_moves,  false);
		node_degrees = new int[nodecount];
		java.util.Arrays.fill(node_degrees,  0);
		moves_per_node = new Intlist[nodecount];
		for (int i = 0; i < nodecount; i++)
			moves_per_node[i] = new Intlist();
		coalesced_with = new int[nodecount];
		java.util.Arrays.fill(coalesced_with, -1);
		colors = new int[nodecount];
		java.util.Arrays.fill(colors, -1);
		status.init(nodecount, liveness.nodecount);
		
		printRegisters();
		printColors();
	}
	
	public void precolor(int node, int color) {
		colors[node] = color;
		status.addNode(node, NodeStatus.PRECOLORED);
		setNoAdjacencyList(node);
	}
	
	public void tryColoring() {
		buildGraph();
		classifyNodes();
		
		do {
			if (! status.getNodes(NodeStatus.REMOVABLE).isEmpty())
				remove_one_removable();
			else if (! status.getMoves(MoveStatus.COALESCABLE).isEmpty())
				coalesce_one();
			else if (! status.getNodes(NodeStatus.FREEZEABLE).isEmpty()) {
				debug("================ Freezing phase");
				printStatus();
				freezeOne();
				debug("================ Freezing done");
			} else if (! status.getNodes(NodeStatus.SPILLABLE).isEmpty())
				prespillOne();
		} while (
			! status.getNodes(NodeStatus.REMOVABLE).isEmpty() ||
			! status.getMoves(MoveStatus.COALESCABLE).isEmpty() ||
			! status.getNodes(NodeStatus.FREEZEABLE).isEmpty() ||
			! status.getNodes(NodeStatus.SPILLABLE).isEmpty()
		);
		printStatus();
		assignColors();
		printStatus();
		checkColors();
	}
	
	public final Intlist getSpilled() {
		return status.getNodes(NodeStatus.SPILLED);
	}
	
	public final int[] getColors() {
		return colors;
	}
}
