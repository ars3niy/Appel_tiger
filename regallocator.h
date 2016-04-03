#ifndef REGALLOCATOR_H
#define REGALLOCATOR_H

#include "assembler.h"
#include "flowgraph.h"
#include "translate_utils.h"
#include <stdint.h>

namespace Optimize {

void PrintLivenessInfo(FILE *f, Asm::Instructions &code,
	IR::AbstractFrame *frame);
void AssignRegisters(Asm::Instructions& code,
	Asm::Assembler &assembler,
	IR::AbstractFrame *frame,
	const std::vector<IR::VirtualRegister *> &machine_registers,
	IR::RegisterMap &id_to_machine_map);

}

#endif // REGALLOCATOR_H
