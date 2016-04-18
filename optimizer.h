#ifndef _OPTIMIZER_H
#define _OPTIMIZER_H

#include "assembler.h"

namespace Optimize {

class Private;

class Optimizer {
private:
	Private *impl;
public:
	Optimizer(const Asm::Assembler &_assembler, const IR::RegisterMap &reg_map);
	~Optimizer();
	
	void optimize(Asm::Instructions &code);
};

}

#endif
