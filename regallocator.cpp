#include "regallocator.h"
#include "flowgraph.h"
#include <vector>
#include <stdio.h>

namespace Optimize {

class LivenessInfo {
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
public:
	std::vector<IR::VirtualRegister *> virtuals;
	
	int nodecount;
	typedef std::list<int> VarList;
	typedef std::vector<int> VarArray;
	std::vector<VarList> live_at_node;
	std::vector<VarArray > used_at_node;
	std::vector<VarArray > assigned_at_node;
	std::vector<bool> node_is_reg_reg_move;
	
	bool isLiveAtNode(const FlowGraphNode *node, int var);
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

class PartialRegAllocator {
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
	
	void post_precolor();
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
public:
	PartialRegAllocator(const LivenessInfo *_liveness, int _colorcount);
	~PartialRegAllocator();
	void precolor(int node, int color);
	void tryColoring();
	const Intlist &getSpilled();
	const std::vector<int> &getColors();
};

bool LivenessInfo::isLiveAtNode(const FlowGraphNode *node, int var)
{
	VarList &vars = live_at_node[node->index];
	for (VarList::iterator v = vars.begin(); v != vars.end(); v++)
		if (*v == var)
			return true;
	return false;
}

bool LivenessInfo::isAssignedAtNode(const FlowGraphNode *node, int var)
{
	std::vector<int> &vars = assigned_at_node[node->index];
	for (int i = 0; i < vars.size(); i++)
		if (vars[i] == var)
			return true;
	return false;
}

void LivenessInfo::findLiveness(int virt_reg, const FlowGraphNode* node)
{
	if (! isLiveAtNode(node, virt_reg) && ! isAssignedAtNode(node, virt_reg)) {
		live_at_node[node->index].push_back(virt_reg);
		for (std::list<FlowGraphNode *>::const_iterator prev = node->previous.begin();
				prev != node->previous.end(); prev++)
			findLiveness(virt_reg, *prev);
	}
}

int LivenessInfo::getVirtualRegisterIndex(IR::VirtualRegister* vreg)
{
	std::map<int, VirtualRegInfo>::iterator pos = virt_indices_by_id.find(vreg->getIndex());
	if (pos == virt_indices_by_id.end())
		return -1;
	else
		return (*pos).second.index;
}

LivenessInfo::LivenessInfo(const FlowGraph& flowgraph)
{
	nodecount = flowgraph.nodeCount();
	int n_virtreg = 0;
	used_at_node.resize(flowgraph.nodeCount());
	assigned_at_node.resize(flowgraph.nodeCount());
	node_is_reg_reg_move.resize(flowgraph.nodeCount(), false);
	max_virt_reg_id = -1;
	
	const FlowGraph::NodeList &nodes = flowgraph.getNodes();	
	for (FlowGraph::NodeList::const_iterator node = nodes.begin();
			node != nodes.end(); node++) {
		{
		const std::vector<IR::VirtualRegister *> &regs = (*node).usedRegisters();
		node_is_reg_reg_move[(*node).index] = (*node).isRegToRegAssignment();
		used_at_node[(*node).index].resize(regs.size());
		for (int i = 0; i < regs.size(); i++) {
			if (virt_indices_by_id.find(regs[i]->getIndex()) == virt_indices_by_id.end()) {
				virt_indices_by_id.insert(std::make_pair(regs[i]->getIndex(),  VirtualRegInfo(n_virtreg, regs[i])));
				if (regs[i]->getIndex() > max_virt_reg_id)
					max_virt_reg_id = regs[i]->getIndex();
				n_virtreg++;
			}
			used_at_node[(*node).index][i] =
				virt_indices_by_id.at(regs[i]->getIndex()).index;
		}
		}
		{
		const std::vector<IR::VirtualRegister *> &regs = (*node).assignedRegisters();
		assigned_at_node[(*node).index].resize(regs.size());
		for (int i = 0; i < regs.size(); i++) {
			if (virt_indices_by_id.find(regs[i]->getIndex()) == virt_indices_by_id.end()) {
				virt_indices_by_id.insert(std::make_pair(regs[i]->getIndex(),  VirtualRegInfo(n_virtreg, regs[i])));
				if (regs[i]->getIndex() > max_virt_reg_id)
					max_virt_reg_id = regs[i]->getIndex();
				n_virtreg++;
			}
			assigned_at_node[(*node).index][i] =
				virt_indices_by_id.at(regs[i]->getIndex()).index;
		}
		}
	}
	
	live_at_node.resize(flowgraph.nodeCount());
	nodes_using.resize(n_virtreg);
	number_invocations.resize(n_virtreg, 0);
	virtuals.resize(n_virtreg);
	for (std::map<int, VirtualRegInfo>::iterator reg = virt_indices_by_id.begin();
			reg != virt_indices_by_id.end(); reg++)
		virtuals[(*reg).second.index] = (*reg).second.reg;
	
	for (FlowGraph::NodeList::const_iterator node = nodes.begin();
			node != nodes.end(); node++) {
		{
		const std::vector<IR::VirtualRegister *> &regs = (*node).usedRegisters();
		for (int i = 0; i < regs.size(); i++) {
			int index = virt_indices_by_id.at(regs[i]->getIndex()).index;
			nodes_using[index].push_back(& *node);
			number_invocations[index]++;
		}
		}
		{
		const std::vector<IR::VirtualRegister *> &regs = (*node).assignedRegisters();
		for (int i = 0; i < regs.size(); i++) {
			int index = virt_indices_by_id.at(regs[i]->getIndex()).index;
			number_invocations[index]++;
		}
		}
	}
	
	for (int i = 0; i < n_virtreg; i++)
		for (NodeList::iterator node = nodes_using[i].begin();
				node != nodes_using[i].end(); node++)
			findLiveness(i, *node);
}

void PrintLivenessInfo(FILE *f, Asm::Instructions& code)
{
	FlowGraph graph(code);
	LivenessInfo liveness(graph);
	
	for (int i = 0; i < graph.nodeCount(); i++) {
		fprintf(f, "Line %d:", i);
		for (LivenessInfo::VarList::iterator v = liveness.live_at_node[i].begin();
				v != liveness.live_at_node[i].end(); v++)
			fprintf(f, " %s", liveness.virtuals[*v]->getName().c_str());
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
	for (Intlist::iterator x = list.begin(); x != list.end(); ++x)
		if (*x == value) {
			list.erase(x);
			return;
		}
	assert(false);
}

void ListRemoveReverse(Intlist& list, int value)
{
	for (Intlist::iterator x = list.end(); x != list.begin();) {
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

PartialRegAllocator::PartialRegAllocator(const LivenessInfo *_liveness,
	int _colorcount) :
	liveness(_liveness)
{
	colorcount = _colorcount;
	nodecount = liveness->virtuals.size();
	table.resize(nodecount*nodecount, false);
	list.resize(nodecount);
	no_list.resize(nodecount, false);
	used_in_moves.resize(nodecount, true);
	node_degrees.resize(nodecount, 0);
	moves_per_node.resize(nodecount);
	coalesced_with.resize(nodecount, -1);
	colors.resize(nodecount, -1);
	status.init(nodecount, liveness->nodecount);
	
	for (int i = 0; i < liveness->nodecount; i++) {
		int assignment_source = -1;
		if (liveness->node_is_reg_reg_move[i]) {
			assert(liveness->used_at_node[i].size() == 1);
			assert(liveness->assigned_at_node[i].size() == 1);
			
			int r1 = liveness->used_at_node[i][0];
			int r2 = liveness->assigned_at_node[i][0];
			if (r1 != r2) { // TODO: eliminate useless moves somehow
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
		
			for (LivenessInfo::VarList::const_iterator livehere = liveness->live_at_node[i].begin();
					livehere != liveness->live_at_node[i].end(); livehere++) {
				if ((*livehere != assigned_here) && (*livehere != assignment_source)) {
					connect(assigned_here, *livehere);
				}
			}
		}
	}
	original_degrees.resize(nodecount);
	for (int i = 0; i < nodecount; i++)
		original_degrees[i] = node_degrees[i];
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

void PartialRegAllocator::post_precolor()
{
	for (int n = 0; n < nodecount; n++)
		if (colors[n] < 0) {
			if (node_degrees[n] >= colorcount)
				status.addNode(n, S_SPILLABLE);
			else if (used_in_moves[n])
				status.addNode(n, S_FREEZEABLE);
			else
				status.addNode(n, S_REMOVABLE);
		}
}

void PartialRegAllocator::getRemainingAdjacent(int n, Intlist& nodes)
{
	const Intlist &core_list = getAdjacent(n);
	nodes.clear();
	for (Intlist::const_iterator node = core_list.begin(); node != core_list.end(); node++)
		if ((status.nodeStatus(*node) != S_SELECTED) && (status.nodeStatus(*node) != S_COALESCED))
			nodes.push_back(*node);
}

bool PartialRegAllocator::hasRemainingMoves(int n)
{
	for (Intlist::iterator move = moves_per_node[n].begin();
			move != moves_per_node[n].end(); move++)
		if ((status.moveStatus(*move) == M_COALESCABLE) ||
			(status.moveStatus(*move) == M_ACTIVE))
			return true;
	return false;
}

void PartialRegAllocator::getRemainingMoves(int n, Intlist& moves)
{
	moves.clear();
	for (Intlist::iterator move = moves_per_node[n].begin();
			move != moves_per_node[n].end(); move++)
		if ((status.moveStatus(*move) == M_COALESCABLE) ||
			(status.moveStatus(*move) == M_ACTIVE))
			moves.push_back(*move);
}

void PartialRegAllocator::remove_one_removable()
{
	if (status.getNodes(S_REMOVABLE).empty())
		return;
	int n = status.getNodes(S_REMOVABLE).front();
	status.setNodeStatus(n, S_SELECTED);
	Intlist adjacent;
	getRemainingAdjacent(n, adjacent);
	for (Intlist::iterator n2 = adjacent.begin(); n2 != adjacent.end(); n2++)
		decrementDegree(*n2);
}

void PartialRegAllocator::decrementDegree(int n)
{
	node_degrees[n]--;
	if (node_degrees[n] == colorcount-1) {
		Intlist neighbours;
		getRemainingAdjacent(n, neighbours);
		neighbours.push_front(n);
		enableMoves(neighbours);
		assert(status.nodeStatus(n) == S_SPILLABLE);
		if (hasRemainingMoves(n))
			status.setNodeStatus(n, S_FREEZEABLE);
		else
			status.setNodeStatus(n, S_REMOVABLE);
	}
}

void PartialRegAllocator::enableMoves(const Intlist& nodes)
{
	for (Intlist::const_iterator node = nodes.begin(); node != nodes.end(); node++) {
		Intlist moves;
		getRemainingMoves(*node, moves);
		for (Intlist::iterator move = moves.begin();
				move != moves.end(); move++) {
			int m = *move;
			if (status.moveStatus(m) == M_ACTIVE)
				status.setMoveStatus(m, M_COALESCABLE);
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
	
	n1 = getRemaingFromCoalescedGroup(n1);
	n2 = getRemaingFromCoalescedGroup(n2);
	
	if (status.nodeStatus(n2) == S_PRECOLORED) {
		int x = n1;
		n1 = n2;
		n2 = x;
	}
	
	if (n1 == n2) {
		status.setMoveStatus(move, M_COALESCED);
		possiblyUnfreeze(n1);
	} else if ((status.nodeStatus(n1) == S_PRECOLORED) &&
			(status.nodeStatus(n2) == S_PRECOLORED) ||
			isAdjacent(n1, n2)) {
		status.setMoveStatus(move, M_CONSTRAINED);
		possiblyUnfreeze(n1);
		possiblyUnfreeze(n2);
	} else if (
			(status.nodeStatus(n1) == S_PRECOLORED) &&
				canCoalesce_noAdjacentListfor1(n1, n2) ||
			(status.nodeStatus(n1) != S_PRECOLORED) &&
				canCoalesce_bothAdjacentLists(n1, n2)
		   ) {
		status.setMoveStatus(move, M_COALESCED);
		combineNodes(n1, n2);
		possiblyUnfreeze(n1);
	} else {
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
	for (Intlist::iterator neighbour2 = adjacent2.begin();
			neighbour2 != adjacent2.end(); neighbour2++) {
		if (!(
			(node_degrees[*neighbour2] < colorcount) ||
			(status.nodeStatus(*neighbour2) == S_PRECOLORED) ||
			isAdjacent(*neighbour2, n1)
		))
			return false;
	}
	return true;
}

bool PartialRegAllocator::canCoalesce_bothAdjacentLists(int n1, int n2)
{
	Intlist adjacent1, adjacent2;
	getRemainingAdjacent(n1, adjacent1);
	getRemainingAdjacent(n2, adjacent2);
	int n_bigdegree = 0;
	for (Intlist::iterator neighbour = adjacent1.begin();
			neighbour != adjacent1.end(); neighbour++)
		if (node_degrees[*neighbour] >= colorcount)
			n_bigdegree++;
	for (Intlist::iterator neighbour = adjacent2.begin();
			neighbour != adjacent2.end(); neighbour++)
		if (node_degrees[*neighbour] >= colorcount)
			n_bigdegree++;
}

void PartialRegAllocator::combineNodes(int remain, int remove)
{
	assert((status.nodeStatus(remove) == S_FREEZEABLE) ||
		(status.nodeStatus(remove) == S_SPILLABLE));
	status.setNodeStatus(remove, S_COALESCED);
	coalesced_with[remove] = remain;
	Intlist removeds_moves;
	getRemainingMoves(remove, removeds_moves);
	for (Intlist::iterator move = removeds_moves.begin();
			move != removeds_moves.end(); move++)
		moves_per_node[remain].push_back(*move);
	
	Intlist adjacent;
	getRemainingAdjacent(remove, adjacent);
	for (Intlist::iterator neighbour = adjacent.begin(); neighbour != adjacent.end();
			neighbour++) {
		connect(*neighbour, remain);
		decrementDegree(*neighbour);
	}
	if ((node_degrees[remain] >= colorcount) &&
			status.nodeStatus(remain) == S_FREEZEABLE)
		status.setNodeStatus(remain, S_SPILLABLE);
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
	Intlist moves;
	getRemainingMoves(node, moves);
	for (Intlist::iterator move = moves.begin(); move != moves.end(); move++) {
		int n1 = getRemaingFromCoalescedGroup(liveness->used_at_node[*move][0]);
		int n2 = getRemaingFromCoalescedGroup(liveness->assigned_at_node[*move][0]);
		int node2;
		if (n2 == getRemaingFromCoalescedGroup(node))
			node2 = n1;
		else
			node2 = n2;
		assert(status.moveStatus(*move) == M_ACTIVE);
		status.setMoveStatus(*move, M_FROZEN);
		if (! hasRemainingMoves(node2) && (node_degrees[node2] < colorcount)) {
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
	
	for (Intlist::const_iterator node = status.getNodes(S_SPILLABLE).begin();
			node != status.getNodes(S_SPILLABLE).end(); node++) {
		float badness = getSpillBadness(*node);
		if ((node_to_spill < 0) || (badness < best_badness)) {
			node_to_spill = *node;
			best_badness = badness;
		}
	}
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
		for (Intlist::iterator neighbour = neighbours.begin();
				neighbour != neighbours.end(); neighbour++) {
			int uncoalesced = getRemaingFromCoalescedGroup(*neighbour);
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
	for (Intlist::const_iterator coalesced = status.getNodes(S_COALESCED).begin();
			coalesced != status.getNodes(S_COALESCED).end(); coalesced++)
		colors[*coalesced] = colors[getRemaingFromCoalescedGroup(*coalesced)];
}

void PartialRegAllocator::tryColoring()
{
	post_precolor();
	
	do {
		if (! status.getNodes(S_REMOVABLE).empty())
			remove_one_removable();
		else if (! status.getMoves(M_COALESCABLE).empty())
			coalesce_one();
		else if (! status.getNodes(S_FREEZEABLE).empty())
			freezeOne();
		else if (! status.getNodes(S_SPILLABLE).empty())
			prespillOne();
	} while (
		! status.getNodes(S_REMOVABLE).empty() ||
		! status.getMoves(M_COALESCABLE).empty() ||
		! status.getNodes(S_FREEZEABLE).empty() ||
		! status.getNodes(S_SPILLABLE).empty()
	);
	assignColors();
}

const std::vector< int > &PartialRegAllocator::getColors()
{
	return colors;
}

const Intlist& PartialRegAllocator::getSpilled()
{
	return status.getNodes(S_SPILLED);
}

void AssignRegisters(Asm::Instructions& code,
	const std::vector<IR::VirtualRegister *> &machine_registers,
	IR::RegisterMap &id_to_machine_map,
	std::list<IR::VirtualRegister *> &unassigned)
{
	FlowGraph graph(code);
	LivenessInfo liveness(graph);
	PartialRegAllocator allocator(&liveness, machine_registers.size());
	for (int i = 0; i < machine_registers.size(); i++) {
		int index = liveness.getVirtualRegisterIndex(machine_registers[i]);
		if (index >= 0)
			allocator.precolor(index, i);
	}
	allocator.tryColoring();
	const Intlist &spilled = allocator.getSpilled();
	const std::vector<int> &colors = allocator.getColors();
	
	id_to_machine_map.resize(liveness.getMaxVirtualRegisterId()+1, NULL);
	assert(colors.size() == liveness.virtuals.size());
	for (int i = 0; i < colors.size(); i++) {
		assert(colors[i] < machine_registers.size());
		if (spilled.empty())
			assert(colors[i] >= 0);
		if (colors[i] >= 0)
			id_to_machine_map[liveness.virtuals[i]->getIndex()] =
				machine_registers[colors[i]];
	}
	
	unassigned.clear();
	for (Intlist::const_iterator ind = spilled.begin(); ind != spilled.end(); ind++)
		unassigned.push_back(liveness.virtuals[*ind]);
}

}