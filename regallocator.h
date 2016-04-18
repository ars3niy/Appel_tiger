#ifndef REGALLOCATOR_H
#define REGALLOCATOR_H

#include "assembler.h"
#include "flowgraph.h"
#include <stdint.h>
#include <string.h>

namespace Optimize {

class Timer {
public:
	int inittime;
	int destruct;
	int flowtime;
	int livenesstime;
	int selfinittime;
	int assigntime;
	int buildtime;
	int removetime;
	int coalescetime;
	int freezetime;
	int prespilltime;
	int spilltime;
	
	Timer() {memset(this, 0, sizeof(*this));}
};

void PrintLivenessInfo(FILE *f, Asm::Instructions &code,
	IR::AbstractFrame *frame);
void AssignRegisters(Asm::Instructions& code,
	Asm::Assembler &assembler,
	IR::AbstractFrame *frame,
	const std::vector<IR::VirtualRegister *> &machine_registers,
	IR::RegisterMap &id_to_machine_map,
	Timer &timer);

}

#endif // REGALLOCATOR_H
