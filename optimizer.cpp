#include "optimizer.h"
#include "debugprint.h"
#include <set>
#include <map>

namespace Optimize {

class Private: public DebugPrinter {
private:
	const Asm::Assembler &assembler;
	const IR::RegisterMap &reg_map;
	bool cleanup(Asm::Instructions &code);
	bool optimizeJxxJmp(Asm::Instructions &code, Asm::Instructions::iterator &p_inst);
public:
	Private(const Asm::Assembler &_assembler, const IR::RegisterMap &_reg_map) :
		DebugPrinter("optimize.log"),
		assembler(_assembler), reg_map(_reg_map) {}
	void optimize(Asm::Instructions &code);
};

bool Private::cleanup(Asm::Instructions &code)
{
	bool did_something = false;
	std::set<int> used_labels;
	std::list<Asm::Instructions::iterator> labels;
	
	for (Asm::Instructions::iterator p_inst = code.begin();
			p_inst != code.end(); ) {
		Asm::Instruction &inst = *p_inst;
		if (inst.is_reg_to_reg_assign) {
			assert(inst.inputs.size() == 1);
			assert(inst.outputs.size() == 1);
			if (IR::MapRegister(&reg_map, inst.inputs[0])->getIndex() ==
					IR::MapRegister(&reg_map, inst.outputs[0])->getIndex()) {
				auto todelete = p_inst;
				++p_inst;
				code.erase(todelete);
				did_something = true;
				continue;
			}
		}
		if (inst.label != NULL)
			labels.push_back(p_inst);
		for (IR::Label *label: inst.extra_destinations)
			used_labels.insert(label->getIndex());
		++p_inst;
	}
	for (Asm::Instructions::iterator p_inst: labels)
		if (used_labels.count((*p_inst).label->getIndex()) == 0) {
			debug("Removing unused label %s", p_inst->label->getName().c_str());
			did_something = true;
			code.erase(p_inst);
		}
	return did_something;
}

bool Private::optimizeJxxJmp(Asm::Instructions &code,
	Asm::Instructions::iterator &p_inst)
{
	assert (! p_inst->extra_destinations.empty());
	Asm::Instructions::iterator after_jmp = p_inst;
	++after_jmp;
	Asm::Instructions::iterator second_jump = after_jmp;
	if (second_jump->extra_destinations.size() != 1)
		return false;
	++after_jmp;
	if ((after_jmp != code.end()) && (after_jmp->label != NULL) &&
			(after_jmp->label->getIndex() == p_inst->extra_destinations.front()->getIndex()))
		// Pattern:
		//     jump-if-something label
		//     jmp somewhere
		//     label:
		// Replacement:
		//     jump-if-not-something somewhere
		//     label:
		if (assembler.canReverseCondJump(*p_inst)) {
			debug("Removing jump to %s by adjusting preceding conditional jump to %s",
				second_jump->extra_destinations.front()->getName().c_str(),
				p_inst->extra_destinations.front()->getName().c_str());
			p_inst->extra_destinations.front() = second_jump->extra_destinations.front();
			assembler.reverseCondJump(*p_inst, second_jump->extra_destinations.front());
			code.erase(second_jump);
			p_inst++;
			return true;
		}
	return false;
}

void Private::optimize(Asm::Instructions &code)
{
	cleanup(code);
	while (true) {
		bool did_something = false;
		for (Asm::Instructions::iterator p_inst = code.begin();
				p_inst != code.end(); p_inst++) {
			if (((*p_inst).extra_destinations.size() == 1) && (*p_inst).jumps_to_next) {
				auto nextcmd = p_inst;
				++nextcmd;
				if ((nextcmd != code.end()) && ! (*nextcmd).jumps_to_next)
					if (optimizeJxxJmp(code, p_inst))
						did_something = true;
			}
		}
		if (! did_something)
			return;
		if (! cleanup(code))
			return;
	}
}

Optimizer::Optimizer(const Asm::Assembler &_assembler, const IR::RegisterMap &reg_map)
{
	impl = new Private(_assembler, reg_map);
}

Optimizer::~Optimizer()
{
	delete impl;
}

void Optimizer::optimize(Asm::Instructions& code)
{
	impl->optimize(code);
}

}
