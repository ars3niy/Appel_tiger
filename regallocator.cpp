#include "regallocator.h"
#include "flowgraph.h"
#include <vector>
#include <stdio.h>
#include <stdarg.h>

namespace Optimize {

class LivenessInfo: public DebugPrinter {
private:
	typedef std::list<const FlowGraphNode *> NodeList;
	std::vector<NodeList> nodes_using;
	std::vector<int> number_invocations; // uses plus assigns for each virtual register

	void findLiveness(int virt_reg, const FlowGraphNode *node);
	
	struct VirtualRegInfo {
		int index; // int this->virtuals
		IR::VirtualRegister *reg;
		
		VirtualRegInfo(int i, IR::VirtualRegister *r) : index(i), reg(r) {}
	};
	std::map<int, VirtualRegInfo> virt_indices_by_id;
	int max_virt_reg_id;
	const FlowGraph *flowgraph;
public:
	std::vector<IR::VirtualRegister *> virtuals;
	
	int nodecount;
	typedef std::list<int> VarList;
	typedef std::vector<int> VarArray;
	std::vector<VarList> live_after_node;
	std::vector<VarArray > used_at_node;
	std::vector<VarArray > assigned_at_node;
	std::vector<bool> node_is_reg_reg_move;
	
	bool isLiveAfterNode(const FlowGraphNode *node, int var) const;
	bool isLiveAfterNode(int node, int var) const;
	bool isAssignedAtNode(const FlowGraphNode *node, int var);
	int getInvocationCount(int node) const	{return number_invocations[node];}
	int getVirtualRegisterIndex(IR::VirtualRegister *vreg);
	int getMaxVirtualRegisterId() {return max_virt_reg_id;}
	
	LivenessInfo(const FlowGraph &flowgraph);
};

typedef std::list<int> Intlist;

enum NodeStatus {
	S_UNPROCESSED,
	S_PRECOLORED,
	S_REMOVABLE,
	S_FREEZEABLE,
	S_SPILLABLE,
	S_SPILLED,
	S_COALESCED,
	S_COLORED,
	S_SELECTED,
	MAX_NODESTATUS
};

enum MoveStatus {
	M_UNPROCESSED,
	M_COALESCED,
	M_CONSTRAINED,
	M_FROZEN,
	M_COALESCABLE,
	M_ACTIVE,
	MAX_MOVESTATUS
};

class NodeMoveManager {
private:
// 	Intlist precolored;
// 	Intlist removable;
// 	Intlist freezeable;
// 	Intlist spillable;
// 	Intlist spilled;
// 	Intlist coalesced;
// 	Intlist colored;
// 	Intlist selected;
	Intlist node_lists[MAX_NODESTATUS];
	std::vector<uint8_t> node_status;
	
// 	Intlist coalesced_moves;
// 	Intlist constrained_moves;
// 	Intlist frozen_moves;
// 	Intlist coalescable_moves;
// 	Intlist active_moves;
	Intlist move_lists[MAX_NODESTATUS];
	std::vector<uint8_t> move_status;
	
	
public:
	void init(int nodecount, int movecount);
	
	NodeStatus nodeStatus(int n) {return (NodeStatus)node_status[n];}
	MoveStatus moveStatus(int m) {return (MoveStatus)move_status[m];}
	
	const Intlist &getNodes(NodeStatus status)
	{
		return node_lists[(int)status];
	}
	
	const Intlist &getMoves(MoveStatus status)
	{
		return move_lists[(int)status];
	}
	
	void setNodeStatus(int n, NodeStatus status);
	void setMoveStatus(int m, MoveStatus status);
	
	void addNode(int n, NodeStatus status)
	{
		node_status[n] = status;
		node_lists[(int)status].push_back(n);
	}
	
	void addMove(int m, MoveStatus status)
	{
		move_status[m] = status;
		move_lists[(int)status].push_back(m);
	}
};

class PartialRegAllocator: public DebugPrinter {
private:
	const LivenessInfo *liveness;
	
	std::vector<bool> table;
	std::vector<Intlist > list;
	std::vector<bool> no_list;
	std::vector<bool> used_in_moves;
	int nodecount;
	int colorcount;

	void setNoAdjacencyList(int n);
	void connect(int n1, int n2);
	bool isAdjacent(int n1, int n2);
	const std::list<int> &getAdjacent(int n);
	
	NodeMoveManager status;
	
	std::vector<int> node_degrees, original_degrees;
	std::vector<Intlist> moves_per_node;
	std::vector<int> coalesced_with;
	std::vector<int> colors;
	
	const char *NodeName(int n)
	{
		return liveness->virtuals[n]->getName().c_str();
	}
	
	void buildGraph();
	void classifyNodes();
	void getRemainingAdjacent(int n, Intlist &nodes);
	bool hasRemainingMoves(int n);
	void getRemainingMoves(int n, Intlist &moves);
	
	void remove_one_removable();
	void decrementDegree(int n);
	void enableMoves(const Intlist &nodes);
	int getRemaingFromCoalescedGroup(int n);
	void coalesce_one();
	void possiblyUnfreeze(int n);
	bool canCoalesce_noAdjacentListfor1(int n1, int n2);
	bool canCoalesce_bothAdjacentLists(int n1, int n2);
	void combineNodes(int remain, int remove);
	void freezeOne();
	void freezeMoves(int node);
	void prespillOne();
	float getSpillBadness(int node);
	void assignColors();
	
	void printRegisters();
	void printColors();
	void printStatus();
	void checkColors();
public:
	PartialRegAllocator(const LivenessInfo *_liveness, int _colorcount,
		const std::string &funcname);
	~PartialRegAllocator();
	void precolor(int node, int color);
	void tryColoring();
	const Intlist &getSpilled();
	const std::vector<int> &getColors();
};

bool LivenessInfo::isLiveAfterNode(const FlowGraphNode *node, int var) const
{
	return isLiveAfterNode(node->index, var);
}

bool LivenessInfo::isLiveAfterNode(int node, int var) const
{
	const VarList &vars = live_after_node[node];
	for (int v: vars)
		if (v == var)
			return true;
	return false;
}

bool LivenessInfo::isAssignedAtNode(const FlowGraphNode *node, int var)
{
	for (int v: assigned_at_node[node->index])
		if (v == var)
			return true;
	return false;
}

void LivenessInfo::findLiveness(int virt_reg, const FlowGraphNode* node)
{
// 	if (! isLiveAtNode(node, virt_reg) && ! isAssignedAtNode(node, virt_reg)) {
// 		live_at_node[node->index].push_back(virt_reg);
// 		for (std::list<FlowGraphNode *>::const_iterator prev = node->previous.begin();
// 				prev != node->previous.end(); prev++)
// 			findLiveness(virt_reg, *prev);
// 	}
		for (FlowGraphNode* prev: node->previous) {
			if (! isLiveAfterNode(prev, virt_reg)) {
				live_after_node[prev->index].push_back(virt_reg);
				if (! isAssignedAtNode(prev, virt_reg))
					findLiveness(virt_reg, prev);
			}
		}
}

int LivenessInfo::getVirtualRegisterIndex(IR::VirtualRegister* vreg)
{
	auto pos = virt_indices_by_id.find(vreg->getIndex());
	if (pos == virt_indices_by_id.end())
		return -1;
	else
		return (*pos).second.index;
}

LivenessInfo::LivenessInfo(const FlowGraph& flowgraph) : DebugPrinter("liveness.log")
{
	this->flowgraph = &flowgraph;
	nodecount = flowgraph.nodeCount();
	int n_virtreg = 0;
	used_at_node.resize(flowgraph.nodeCount());
	assigned_at_node.resize(flowgraph.nodeCount());
	node_is_reg_reg_move.resize(flowgraph.nodeCount(), false);
	max_virt_reg_id = -1;
	
	const FlowGraph::NodeList &nodes = flowgraph.getNodes();	
	for (const FlowGraphNode &node: nodes) {
		debug("node %d, %d next, %d prev", node.index,
			  node.next.size(), node.previous.size());
		{
			const std::vector<IR::VirtualRegister *> &regs = node.usedRegisters();
			if (node.isRegToRegAssignment())
				assert(regs.size() == 1);
			node_is_reg_reg_move[node.index] = node.isRegToRegAssignment();
			used_at_node[node.index].resize(regs.size());
			for (int i = 0; i < regs.size(); i++) {
				if (virt_indices_by_id.find(regs[i]->getIndex()) == virt_indices_by_id.end()) {
					virt_indices_by_id.insert(std::make_pair(regs[i]->getIndex(),  VirtualRegInfo(n_virtreg, regs[i])));
					if (regs[i]->getIndex() > max_virt_reg_id)
						max_virt_reg_id = regs[i]->getIndex();
					n_virtreg++;
				}
				used_at_node[node.index][i] =
					virt_indices_by_id.at(regs[i]->getIndex()).index;
			}
		}
		{
			const std::vector<IR::VirtualRegister *> &regs = node.assignedRegisters();
			assigned_at_node[node.index].resize(regs.size());
			for (int i = 0; i < regs.size(); i++) {
				if (virt_indices_by_id.find(regs[i]->getIndex()) == virt_indices_by_id.end()) {
					virt_indices_by_id.insert(std::make_pair(regs[i]->getIndex(),  VirtualRegInfo(n_virtreg, regs[i])));
					if (regs[i]->getIndex() > max_virt_reg_id)
						max_virt_reg_id = regs[i]->getIndex();
					n_virtreg++;
				}
				assigned_at_node[node.index][i] =
					virt_indices_by_id.at(regs[i]->getIndex()).index;
			}
		}
	}
	
	live_after_node.resize(flowgraph.nodeCount());
	nodes_using.resize(n_virtreg);
	number_invocations.resize(n_virtreg, 0);
	virtuals.resize(n_virtreg);
	for (auto &reg: virt_indices_by_id)
		virtuals[reg.second.index] = reg.second.reg;
	
	for (const FlowGraphNode &node: nodes) {
		for (IR::VirtualRegister *reg: node.usedRegisters()) {
			int index = virt_indices_by_id.at(reg->getIndex()).index;
			nodes_using[index].push_back(&node);
			number_invocations[index]++;
		}
		for (IR::VirtualRegister *reg: node.assignedRegisters()) {
			int index = virt_indices_by_id.at(reg->getIndex()).index;
			number_invocations[index]++;
		}
	}
	
	for (int i = 0; i < n_virtreg; i++)
		for (const FlowGraphNode *node: nodes_using[i])
			findLiveness(i, node);
}

void PrintLivenessInfo(FILE *f, Asm::Instructions& code, IR::AbstractFrame *frame)
{
	FlowGraph graph(code, frame->getFramePointer());
	LivenessInfo liveness(graph);
	
	for (int i = 0; i < graph.nodeCount(); i++) {
		fprintf(f, "Line %d:", i);
		for (int v: liveness.live_after_node[i])
			fprintf(f, " %s", liveness.virtuals[v]->getName().c_str());
		fputc('\n', f);
	}
}

void NodeMoveManager::init(int nodecount, int movecount)
{
	
	
	node_status.resize(nodecount, S_UNPROCESSED);
	move_status.resize(movecount);
}

void ListRemove(Intlist& list, int value)
{
	for (auto x = list.begin(); x != list.end(); ++x)
		if (*x == value) {
			list.erase(x);
			return;
		}
	assert(false);
}

void ListRemoveReverse(Intlist& list, int value)
{
	for (auto x = list.end(); x != list.begin();) {
		--x;
		if (*x == value) {
			list.erase(x);
			return;
		}
	}
	assert(false);
}

void NodeMoveManager::setMoveStatus(int m, MoveStatus status)
{
	ListRemove(move_lists[(int)move_status[m]], m);
	move_status[m] = status;
	move_lists[(int)status].push_back(m);
}

void NodeMoveManager::setNodeStatus(int n, NodeStatus status)
{
	if (node_status[n] == S_SELECTED)
		ListRemoveReverse(node_lists[(int)node_status[n]], n);
	else
		ListRemove(node_lists[(int)node_status[n]], n);
	node_status[n] = status;
	node_lists[(int)status].push_back(n);
}

void PartialRegAllocator::printRegisters()
{
// 	debug("Registers:");
// 	for (int i = 0; i < nodecount; i++)
// 		debug("%d: %s", i, liveness->virtuals[i]->getName().c_str());
}

void PartialRegAllocator::printColors()
{
	debug("%d colors", colorcount);
}

const char *node_status_names[] = {
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

const char *move_status_names[] = {
	"unprocessed",
	"coalesced",
	"constrained",
	"frozen",
	"coalescable",
	"active",
};

static std::string IntToStr(int x)
{
	char buf[30];
	sprintf(buf, "%d", x);
	return std::string(buf);
}

void PartialRegAllocator::printStatus()
{
	for (NodeStatus ns = (NodeStatus)0; ns < MAX_NODESTATUS; ns = (NodeStatus)((int)ns+1)) {
		debug("%s nodes:", node_status_names[(int)ns]);
		const Intlist &nodes = status.getNodes(ns);
		for (int n: nodes) {
			std::string message = NodeName(n);
			if ((ns == S_COLORED) || (ns == S_PRECOLORED)) {
				message = message + ", color " + IntToStr(colors[n]);
			}
			if (ns != S_PRECOLORED)
				message = message + ", degree " + IntToStr(node_degrees[n]);
			message = message + " interferes with";
			for (int n2 = 0; n2 < nodecount; n2++)
				if ((n2 != n) && isAdjacent(n, n2))
					message = message + " " + NodeName(n2);
			debug("%s", message.c_str());
		}
	}
	for (MoveStatus ms = (MoveStatus)0; ms < MAX_MOVESTATUS; ms = (MoveStatus)((int)ms+1)) {
		debug("%s moves:", move_status_names[(int)ms]);
		const Intlist &moves = status.getMoves(ms);
		for (int m: moves) {
			debug("%d: %s -> %s", m,
				NodeName(liveness->used_at_node[m][0]),
				NodeName(liveness->assigned_at_node[m][0]));
		}
	}
}

PartialRegAllocator::PartialRegAllocator(const LivenessInfo *_liveness,
	int _colorcount, const std::string &funcname) :
	liveness(_liveness),
	DebugPrinter(("allocator_" + funcname + ".log").c_str())
{
	colorcount = _colorcount;
	nodecount = liveness->virtuals.size();
	table.resize(nodecount*nodecount, false);
	list.resize(nodecount);
	no_list.resize(nodecount, false);
	used_in_moves.resize(nodecount, false);
	node_degrees.resize(nodecount, 0);
	moves_per_node.resize(nodecount);
	coalesced_with.resize(nodecount, -1);
	colors.resize(nodecount, -1);
	status.init(nodecount, liveness->nodecount);
	
	printRegisters();
	printColors();
}

PartialRegAllocator::~PartialRegAllocator()
{
}

void PartialRegAllocator::setNoAdjacencyList(int n)
{
	list[n].clear();
	no_list[n] = true;
}

void PartialRegAllocator::connect(int n1, int n2)
{
	assert(n1 != n2);
	if (! table[n1*nodecount+n2] && ! table[n2*nodecount+n1]) {
		table[n1*nodecount+n2] = true;
		table[n2*nodecount+n1] = true;
		if (! no_list[n1]) {
			list[n1].push_back(n2);
			node_degrees[n1]++;
		}
		if (! no_list[n2]) {
			list[n2].push_back(n1);
			node_degrees[n2]++;
		}
	}
}

bool PartialRegAllocator::isAdjacent(int n1, int n2)
{
	return table[n1*nodecount+n2];
}

const std::list< int > &PartialRegAllocator::getAdjacent(int n)
{
	assert(! no_list[n]);
	return list[n];
}

void PartialRegAllocator::precolor(int node, int color)
{
	colors[node] = color;
	status.addNode(node, S_PRECOLORED);
	setNoAdjacencyList(node);
}

void PartialRegAllocator::buildGraph()
{
	for (int i = 0; i < liveness->nodecount; i++) {
		int assignment_source = -1;
		if (liveness->node_is_reg_reg_move[i]) {
			assert(liveness->used_at_node[i].size() == 1);
			assert(liveness->assigned_at_node[i].size() == 1);
			
			int r1 = liveness->used_at_node[i][0];
			int r2 = liveness->assigned_at_node[i][0];
			if (r1 != r2) {
				debug("move %s -> %s", NodeName(r1), NodeName(r2));
				assignment_source = r1;
				used_in_moves[r1] = true;
				used_in_moves[r2] = true;
				status.addMove(i, M_COALESCABLE);
				moves_per_node[r1].push_back(i);
				moves_per_node[r2].push_back(i);
			}
		}
		
		for (int assign_ind = 0; assign_ind < liveness->assigned_at_node[i].size();
				assign_ind++) {
			int assigned_here = liveness->assigned_at_node[i][assign_ind];
			//if (liveness->isLiveAfterNode(i, assigned_here))
				for (int livehere: liveness->live_after_node[i]) {
					if ((livehere != assigned_here) &&
							(livehere != assignment_source)) {
						debug("Line %d, Assigned node %s, collides with live node %s",
							  i, NodeName(assigned_here),
							  NodeName(livehere));
						connect(assigned_here, livehere);
					}
				}
		}
	}
	original_degrees.resize(nodecount);
	for (int i = 0; i < nodecount; i++)
		original_degrees[i] = node_degrees[i];
}


void PartialRegAllocator::classifyNodes()
{
	for (int n = 0; n < nodecount; n++)
		if (colors[n] < 0) {
			if (node_degrees[n] >= colorcount) {
				debug("Node %s is SPILLABLE", NodeName(n));
				status.addNode(n, S_SPILLABLE);
			} else if (used_in_moves[n]) {
				debug("Node %s is FREEZEABLE", NodeName(n));
				status.addNode(n, S_FREEZEABLE);
			} else
				status.addNode(n, S_REMOVABLE);
		}
	printStatus();
}

void PartialRegAllocator::getRemainingAdjacent(int n, Intlist& nodes)
{
	const Intlist &core_list = getAdjacent(n);
	nodes.clear();
	for (int node: core_list)
		if ((status.nodeStatus(node) != S_SELECTED) && (status.nodeStatus(node) != S_COALESCED))
			nodes.push_back(node);
}

bool PartialRegAllocator::hasRemainingMoves(int n)
{
	for (int move: moves_per_node[n])
		if ((status.moveStatus(move) == M_COALESCABLE) ||
			(status.moveStatus(move) == M_ACTIVE))
			return true;
	return false;
}

void PartialRegAllocator::getRemainingMoves(int n, Intlist& moves)
{
	moves.clear();
	for (int move: moves_per_node[n])
		if ((status.moveStatus(move) == M_COALESCABLE) ||
			(status.moveStatus(move) == M_ACTIVE))
			moves.push_back(move);
}

void PartialRegAllocator::remove_one_removable()
{
	if (status.getNodes(S_REMOVABLE).empty())
		return;
	int n = status.getNodes(S_REMOVABLE).front();
	debug("Removing node %s", NodeName(n));
	status.setNodeStatus(n, S_SELECTED);
	Intlist adjacent;
	getRemainingAdjacent(n, adjacent);
	for (int n2: adjacent)
		decrementDegree(n2);
}

void PartialRegAllocator::decrementDegree(int n)
{
	if (status.nodeStatus(n) == S_PRECOLORED)
		return;
	debug("decrementDegree: node %s is initially %d", 
		  NodeName(n), node_degrees[n]);
	node_degrees[n]--;
	if (node_degrees[n] == colorcount-1) {
		Intlist neighbours;
		getRemainingAdjacent(n, neighbours);
		neighbours.push_front(n);
		enableMoves(neighbours);
		assert(status.nodeStatus(n) == S_SPILLABLE);
		if (hasRemainingMoves(n)) {
			debug("DecrementDegree: setting node %s to freezeable",
				NodeName(n));
			status.setNodeStatus(n, S_FREEZEABLE);
		} else {
			debug("DecrementDegree: Can now remove node %s", NodeName(n));
			status.setNodeStatus(n, S_REMOVABLE);
		}
	}
}

void PartialRegAllocator::enableMoves(const Intlist& nodes)
{
	for (int node: nodes) {
		Intlist moves;
		getRemainingMoves(node, moves);
		for (int move: moves) {
			if (status.moveStatus(move) == M_ACTIVE)
				status.setMoveStatus(move, M_COALESCABLE);
		}
	}
}

int PartialRegAllocator::getRemaingFromCoalescedGroup(int n)
{
	if (status.nodeStatus(n) == S_COALESCED)
		return getRemaingFromCoalescedGroup(coalesced_with[n]);
	else
		return n;
}

void PartialRegAllocator::coalesce_one()
{
	if (status.getMoves(M_COALESCABLE).empty())
		return;
	int move = status.getMoves(M_COALESCABLE).front();
	int n1 = liveness->used_at_node[move][0];
	int n2 = liveness->assigned_at_node[move][0];
	debug("coalesce_one: merging %s and %s",
		NodeName(n1),
		NodeName(n2));
	
	n1 = getRemaingFromCoalescedGroup(n1);
	n2 = getRemaingFromCoalescedGroup(n2);
	
	if (status.nodeStatus(n2) == S_PRECOLORED) {
		int x = n1;
		n1 = n2;
		n2 = x;
	}
	
	if (n1 == n2) {
		debug("Already coalesced");
		status.setMoveStatus(move, M_COALESCED);
		possiblyUnfreeze(n1);
	} else if ((status.nodeStatus(n2) == S_PRECOLORED) ||
			isAdjacent(n1, n2)) {
		if (status.nodeStatus(n2) == S_PRECOLORED)
			debug("Cannot coalesce, two precolored nodes");
		else
			debug("Cannot coalesce, two interfering nodes");
		status.setMoveStatus(move, M_CONSTRAINED);
		possiblyUnfreeze(n1);
		possiblyUnfreeze(n2);
	} else if (
			(status.nodeStatus(n1) == S_PRECOLORED) &&
				canCoalesce_noAdjacentListfor1(n1, n2) ||
			(status.nodeStatus(n1) != S_PRECOLORED) &&
				canCoalesce_bothAdjacentLists(n1, n2)
		   ) {
		debug("Safe to coalesce, proceed");
		status.setMoveStatus(move, M_COALESCED);
		combineNodes(n1, n2);
		possiblyUnfreeze(n1);
	} else {
		debug("Afraid to coalesce, maybe later");
		status.setMoveStatus(move, M_ACTIVE);
	}
}

void PartialRegAllocator::possiblyUnfreeze(int n)
{
	if ((status.nodeStatus(n) != S_PRECOLORED) && ! hasRemainingMoves(n) &&
			(node_degrees[n] < colorcount)) {
		assert(status.nodeStatus(n) == S_FREEZEABLE);
		status.setNodeStatus(n, S_REMOVABLE);
	}
}

bool PartialRegAllocator::canCoalesce_noAdjacentListfor1(int n1, int n2)
{
	Intlist adjacent2;
	getRemainingAdjacent(n2, adjacent2);
	for (int neighbour2: adjacent2) {
		if (!(
			(node_degrees[neighbour2] < colorcount) ||
			(status.nodeStatus(neighbour2) == S_PRECOLORED) ||
			isAdjacent(neighbour2, n1)
		)) {
			debug("Afraid to coalesce %s and %s",
				NodeName(n1), NodeName(n2));
			debug("... because %s has neighbour %s of degree %d not precolored and not adjacent to %s",
					NodeName(n2), NodeName(neighbour2), node_degrees[neighbour2],
					NodeName(n1));
			return false;
		}
	}
	return true;
}

bool PartialRegAllocator::canCoalesce_bothAdjacentLists(int n1, int n2)
{
	Intlist adjacent1, adjacent2;
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

void PartialRegAllocator::combineNodes(int remain, int remove)
{
	assert((status.nodeStatus(remove) == S_FREEZEABLE) ||
		(status.nodeStatus(remove) == S_SPILLABLE));
	status.setNodeStatus(remove, S_COALESCED);
	coalesced_with[remove] = remain;
	Intlist removeds_moves;
	getRemainingMoves(remove, removeds_moves);
	for (int move: removeds_moves)
		moves_per_node[remain].push_back(move);
	
	Intlist adjacent;
	getRemainingAdjacent(remove, adjacent);
	for (int neighbour: adjacent) {
		if (! isAdjacent(neighbour, remain)) {
			debug("combineNodes: Connecting %s with %s, degrees initially %d and %d",
				NodeName(neighbour),
				NodeName(remain),
				node_degrees[neighbour], node_degrees[remain]
				);
			connect(neighbour, remain);
			//decrementDegree(*neighbour);
			if (status.nodeStatus(neighbour) != S_PRECOLORED)
				node_degrees[neighbour]--;
			debug("combineNodes: Connected, degrees are now %s = %d  %s = %d",
				NodeName(neighbour),
				node_degrees[neighbour], 
				NodeName(remain),
				node_degrees[remain]
				);
		}
	}
	if ((node_degrees[remain] >= colorcount) &&
			status.nodeStatus(remain) == S_FREEZEABLE) {
		debug("CombineNodes: node %s has now degree %d thus SPILLABLE",
			  NodeName(remain), node_degrees[remain]);
		status.setNodeStatus(remain, S_SPILLABLE);
	}
}

void PartialRegAllocator::freezeOne()
{
	if (status.getNodes(S_FREEZEABLE).empty())
		return;
	int node = status.getNodes(S_FREEZEABLE).front();
	status.setNodeStatus(node, S_REMOVABLE);
	freezeMoves(node);
}

void PartialRegAllocator::freezeMoves(int node)
{
	debug("freezeMoves for node %s", NodeName(node));
	Intlist moves;
	getRemainingMoves(node, moves);
	for (int move: moves) {
		int n1 = getRemaingFromCoalescedGroup(liveness->used_at_node[move][0]);
		int n2 = getRemaingFromCoalescedGroup(liveness->assigned_at_node[move][0]);
		int node2;
		if (n2 == getRemaingFromCoalescedGroup(node))
			node2 = n1;
		else
			node2 = n2;
		debug("Freezing move %s -> %s",
			NodeName(liveness->used_at_node[move][0]),
			NodeName(liveness->assigned_at_node[move][0]));
		assert(status.moveStatus(move) == M_ACTIVE);
		status.setMoveStatus(move, M_FROZEN);
		if (! hasRemainingMoves(node2) && (node_degrees[node2] < colorcount) &&
				(status.nodeStatus(node2) != S_PRECOLORED)) {
			assert(status.nodeStatus(node2) == S_FREEZEABLE);
			status.setNodeStatus(node2, S_REMOVABLE);
		}
	}
}

float PartialRegAllocator::getSpillBadness(int node)
{
	return (float)liveness->getInvocationCount(node) / original_degrees[node];
}

void PartialRegAllocator::prespillOne()
{
	int node_to_spill = -1;
	float best_badness = 1e6;
	
	for (int node: status.getNodes(S_SPILLABLE)) {
		float badness = getSpillBadness(node);
		debug("Spillable node %s, badness %g",
			  NodeName(node), badness);
		if ((node_to_spill < 0) || (badness < best_badness)) {
			node_to_spill = node;
			best_badness = badness;
		}
	}
	debug("Spilling node %s, (badness %g)",
			NodeName(node_to_spill),
			best_badness);
	status.setNodeStatus(node_to_spill, S_REMOVABLE);
	freezeMoves(node_to_spill);
}

void PartialRegAllocator::assignColors()
{
	while (! status.getNodes(S_SELECTED).empty()) {
		int n = status.getNodes(S_SELECTED).back();
		
		std::vector<int> colors_available;
		colors_available.resize(colorcount, true);
		
		Intlist &neighbours = list[n];
		for (int neighbour: neighbours) {
			int uncoalesced = getRemaingFromCoalescedGroup(neighbour);
			if ((status.nodeStatus(uncoalesced) == S_PRECOLORED) ||
					(status.nodeStatus(uncoalesced) == S_COLORED))
				colors_available[colors[uncoalesced]] = false;
		}
		
		int next_color = -1;
		for (int i = 0; i < colorcount; i++)
			if (colors_available[i]) {
				next_color = i;
				break;
			}
		if (next_color < 0)
			status.setNodeStatus(n, S_SPILLED);
		else {
			status.setNodeStatus(n, S_COLORED);
			colors[n] = next_color;
		}
	}
	for (int coalesced: status.getNodes(S_COALESCED))
		colors[coalesced] = colors[getRemaingFromCoalescedGroup(coalesced)];
}

void PartialRegAllocator::tryColoring()
{
	buildGraph();
	classifyNodes();
	
	do {
		if (! status.getNodes(S_REMOVABLE).empty())
			remove_one_removable();
		else if (! status.getMoves(M_COALESCABLE).empty())
			coalesce_one();
		else if (! status.getNodes(S_FREEZEABLE).empty()) {
			debug("================ Freezing phase");
			printStatus();
			freezeOne();
			debug("================ Freezing done");
		} else if (! status.getNodes(S_SPILLABLE).empty())
			prespillOne();
	} while (
		! status.getNodes(S_REMOVABLE).empty() ||
		! status.getMoves(M_COALESCABLE).empty() ||
		! status.getNodes(S_FREEZEABLE).empty() ||
		! status.getNodes(S_SPILLABLE).empty()
	);
	printStatus();
	assignColors();
	printStatus();
}

const std::vector< int > &PartialRegAllocator::getColors()
{
	return colors;
}

const Intlist& PartialRegAllocator::getSpilled()
{
	return status.getNodes(S_SPILLED);
}

void PartialRegAllocator::checkColors()
{
	for (int i = 0; i < liveness->nodecount; i++) {
		int assignment_source = -1;
		if (liveness->node_is_reg_reg_move[i]) {
			assert(liveness->used_at_node[i].size() == 1);
			assert(liveness->assigned_at_node[i].size() == 1);
			
			int r1 = liveness->used_at_node[i][0];
			int r2 = liveness->assigned_at_node[i][0];
			if (r1 != r2) { // TODO: eliminate useless moves somehow
				assignment_source = r1;
			}
		}
		for (int assign_ind = 0; assign_ind < liveness->assigned_at_node[i].size();
				assign_ind++) {
			int assigned_here = liveness->assigned_at_node[i][assign_ind];
			if (liveness->isLiveAfterNode(i, assigned_here))
				for (int livehere: liveness->live_after_node[i]) {
					if ((livehere != assigned_here) &&
							(livehere != assignment_source)) {
						if ((colors[livehere] >= 0) &&
								(colors[assignment_source] >= 0) &&
								(colors[livehere] == colors[assignment_source]))
							Error::fatalError("Colliding nodes colored the same");
					}
				}
		}
	}
}

void AssignRegisters(Asm::Instructions& code,
	Asm::Assembler &assembler,
	IR::AbstractFrame *frame,
	const std::vector<IR::VirtualRegister *> &machine_registers,
	IR::RegisterMap &id_to_machine_map)
{
	bool finished = false;
	FlowGraph *graph = NULL;
	LivenessInfo *liveness = NULL;
	PartialRegAllocator *allocator = NULL;
	const std::vector<int> *colors = NULL;
	const Intlist *spilled = NULL;
	
	do {
		delete graph;
		delete liveness;
		delete allocator;
		graph = new FlowGraph(code, frame->getFramePointer());
		liveness = new LivenessInfo(*graph);
		allocator = new PartialRegAllocator(liveness, machine_registers.size(),
			frame->getName());
		allocator->debug("%d colors", machine_registers.size());
		bool found_fp = false;
		for (int i = 0; i < machine_registers.size(); i++)
			allocator->debug("%d: %s", i, machine_registers[i]->getName().c_str());
		for (int i = 0; i < machine_registers.size(); i++) {
			int index = liveness->getVirtualRegisterIndex(machine_registers[i]);
			if (index >= 0) {
				allocator->precolor(index, i);
			}
		}
		allocator->tryColoring();
		spilled = & allocator->getSpilled();
		colors = & allocator->getColors();
		
		if (! spilled->empty()) {
			allocator->debug("================================ SPILLING AND RESTARTING");
		}
		
		for (int n: *spilled)
			assembler.spillRegister(frame, code, liveness->virtuals[n]);
	} while (! spilled->empty());
	
		id_to_machine_map.resize(liveness->getMaxVirtualRegisterId()+1, NULL);
		assert(colors->size() == liveness->virtuals.size());
		for (int i = 0; i < colors->size(); i++) {
			assert(colors->at(i) < (int)machine_registers.size());
			if (spilled->empty())
				assert(colors->at(i) >= 0);
			if (colors->at(i) >= 0)
				id_to_machine_map[liveness->virtuals[i]->getIndex()] =
					machine_registers[colors->at(i)];
		}

	delete graph;
	delete liveness;
	delete allocator;
}

}