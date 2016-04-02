#ifndef REGALLOCATOR_H
#define REGALLOCATOR_H

#include "assembler.h"
#include "flowgraph.h"
#include <stdint.h>

namespace Optimize {

void PrintLivenessInfo(FILE *f, Asm::Instructions &code);
void AssignRegisters(Asm::Instructions& code,
	const std::vector<IR::VirtualRegister *> &machine_registers,
	IR::RegisterMap &id_to_machine_map,
	std::list<IR::VirtualRegister *> &unassigned);

}

#endif // REGALLOCATOR_H
