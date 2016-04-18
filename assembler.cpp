#include "assembler.h"
#include "errormsg.h"
#include "error.h"
#include <stdarg.h>
#include <sys/time.h>

namespace Asm {

std::string IntToStr(int x)
{
	char buf[30];
	sprintf(buf, "%d", x);
	return std::string(buf);
}

Instruction::Instruction(const std::string &_notation,
	bool _reg_to_reg_assign)
		: notation(_notation), label(NULL),
		is_reg_to_reg_assign(_reg_to_reg_assign)
{
	jumps_to_next = true;
}

Instruction::Instruction(const std::string &_notation,
		const std::vector<IR::VirtualRegister *> &_inputs,
		const std::vector<IR::VirtualRegister *> &_outputs,
		bool _reg_to_reg_assign,
		const std::vector<IR::Label *> &_destinations,
		bool also_jump_to_next) :
	notation(_notation), inputs(_inputs), outputs(_outputs),
	//destinations(_destinations),
	label(NULL),
	is_reg_to_reg_assign(_reg_to_reg_assign)
{
	//if (destinations.size() == 0)
	//	destinations.push_back(NULL);
	if (_destinations.size() == 0)
		jumps_to_next = true;
	else {
		extra_destinations = _destinations;
		jumps_to_next = also_jump_to_next;
	}
}

Assembler::Assembler(IR::IREnvironment *ir_env)
	: DebugPrinter("assembler.log"), IRenvironment(ir_env)
{
	finding_time = 0;
	expr_templates.resize((int)IR::IR_EXPR_MAX);
	for (int i = 0; i < IR::IR_EXPR_MAX; i++) {
		if (i == (int)IR::IR_BINARYOP)
			expr_templates[i] = new TemplateOpKindKindMap(IR::BINOP_MAX);
		else
			expr_templates[i] = new TemplateList;
	}
	
	statm_templates.resize((int)IR::IR_STAT_MAX);
	for (int i = 0; i < IR::IR_STAT_MAX; i++) {
		if (i == (int)IR::IR_COND_JUMP)
			statm_templates[i] = new TemplateOpKindKindMap(IR::COMPOP_MAX);
		else
			statm_templates[i] = new TemplateList;
	}
}

Assembler::~Assembler()
{
	for (TemplateStorage *storage: expr_templates)
		delete storage;
	for (TemplateStorage *storage: statm_templates)
		delete storage;
}

std::list<std::shared_ptr<Template> > *Assembler::getTemplatesList(IR::Code code)
{
	switch (code->kind) {
		case IR::CODE_EXPRESSION:
			return getTemplatesList(std::static_pointer_cast<IR::ExpressionCode>(code)->exp);
		case IR::CODE_STATEMENT:
			return getTemplatesList(std::static_pointer_cast<IR::StatementCode>(code)->statm);
		default:
			Error::fatalError("Wrong template code kind");
			return NULL;
	}
}

std::list<std::shared_ptr<Template> > *Assembler::getTemplatesList(IR::Expression expr)
{
	if (expr->kind == IR::IR_BINARYOP) {
		std::shared_ptr<IR::BinaryOpExpression> binop = IR::ToBinaryOpExpression(expr);
		return ((TemplateOpKindKindMap *)expr_templates[(int)expr->kind])->
			getList((int)binop->operation/*, (int)binop->left->kind, (int)binop->right->kind*/);
	} else
		return & ((TemplateList *)expr_templates[(int)expr->kind])->list;
}

std::list<std::shared_ptr<Template> > *Assembler::getTemplatesList(IR::Statement statm)
{
	if ((statm->kind == IR::IR_COND_JUMP) /*|| (statm->kind = IR::IR_MOVE)*/) {
		int op;//, kind1, kind2;
// 		if (statm->kind == IR::IR_COND_JUMP) {
			std::shared_ptr<IR::CondJumpStatement> cj = IR::ToCondJumpStatement(statm);
			op = (int)cj->comparison;
			//kind1 = (int)cj->left->kind;
			//kind2 = (int)cj->right->kind;
// 		} else {
// 			IR::MoveStatement *m = IR::ToMoveStatement(statm);
// 			op = 0;
// 			kind1 = (int)m->to->kind;
// 			kind2 = (int)m->from->kind;
// 		}
		return ((TemplateOpKindKindMap *)statm_templates[(int)statm->kind])->
			getList(op/*, kind1, kind2*/);
	} else
		return & ((TemplateList *)statm_templates[(int)statm->kind])->list;
}

void Assembler::addTemplate(const std::vector<std::string> &commands, IR::Expression expr)
{
	addTemplate(std::make_shared<Template>(commands, expr));
}

void Assembler::addTemplate(const std::vector<std::string> &commands, IR::Statement statm)
{
	addTemplate(std::make_shared<Template>(commands, statm));
}

void Assembler::addTemplate(const std::vector<std::string> &_commands,
	const std::vector<std::vector<IR::VirtualRegister *> > &extra_inputs,
	const std::vector<std::vector<IR::VirtualRegister *> > &extra_outputs,
	const std::vector<bool> is_reg_to_reg_assign, IR::Expression expr,
	TemplateAdjuster adjuster_fcn, ExpressionMatchChecker match_fcn)
{
	auto templ = std::make_shared<Template>(expr, adjuster_fcn, match_fcn);
	for (unsigned i = 0; i < _commands.size(); i++)
		templ->instructions.push_back(Instruction(_commands.at(i),
			extra_inputs.at(i), extra_outputs.at(i), is_reg_to_reg_assign.at(i)));
	addTemplate(templ);
}

void Assembler::addTemplate(const std::vector<std::string> &_commands,
	const std::vector<bool> &is_jump_to_extra_dest, 
	const std::vector<bool> &jumps_to_next, IR::Statement statm)
{
	auto templ = std::make_shared<Template>(statm);
	std::vector<IR::Label *> no_extra_dest = {};
	std::vector<IR::Label *> has_extra_dest = {NULL};
	for (unsigned i = 0; i < _commands.size(); i++)
		templ->instructions.push_back(Instruction(_commands.at(i), {}, {}, false,
			is_jump_to_extra_dest.at(i) ? has_extra_dest : no_extra_dest,
			jumps_to_next.at(i)));
	addTemplate(templ);
}

void Assembler::addTemplate(const std::string &command, IR::Expression expr)
{
	addTemplate(std::make_shared<Template>(command, false, expr));
}

void Assembler::addTemplate(const std::string &command, IR::Statement statm)
{
	addTemplate(std::make_shared<Template>(command, false, statm));
}

void Assembler::addTemplate(const std::string &command, 
	bool is_reg_to_reg_assign, IR::Expression expr)
{
	addTemplate(std::make_shared<Template>(command, is_reg_to_reg_assign, expr));
}

void Assembler::addTemplate(const std::string &command, 
	bool is_reg_to_reg_assign, IR::Statement statm)
{
	addTemplate(std::make_shared<Template>(command, is_reg_to_reg_assign, statm));
}

void Assembler::addTemplate(std::shared_ptr<Template> templ)
{
#ifdef DEBUG
	if (templ->match_fcn != NULL)
		debug("Template with match function 0x%x", templ->match_fcn);
	debug("Assembler Template:");
	PrintCode(debug_output, templ->code);
	for (Instruction &inst: templ->instructions) {
		std::string s = "    " + inst.notation;
		if (! inst.inputs.empty()) {
			s += " <-";
			for (IR::VirtualRegister *reg: inst.inputs)
				s += " " + reg->getName();
		}
		if (! inst.outputs.empty()) {
			s += " ->";
			for (IR::VirtualRegister *reg: inst.outputs)
				s += " " + reg->getName();
		}
		if (inst.is_reg_to_reg_assign)
			s += " register assignment";
		if (! inst.extra_destinations.empty())
			s += " jumps";
		if (! inst.jumps_to_next)
			s += " no fall-through";
		debug("%s", s.c_str());
	}
#endif
	std::list<std::shared_ptr<Template> > *list = getTemplatesList(templ->code);
	debug("Adding template, already have %d\n", list->size());
	list->push_back(templ);
}

std::string Assembler::Operand::implement(int first_index) const
{
	std::string result;
	unsigned i = 0;
	unsigned ind = 0;
	while (i < format_string.size()) {
		unsigned i2 = i;
		while ((i2 < format_string.size()) && (format_string[i2] != '%'))
			i2++;
		result += format_string.substr(i, i2-i);
		if (i2 < format_string.size()) {
			assert (ind < elements.size());
			char buf[16];
			snprintf(buf, sizeof(buf), "%d", first_index + elements[ind]);
			result += "&";
			result += buf;
			ind++;
			i2++;
		}
		i = i2;
	}
	assert (ind == elements.size());
	return result;
}

bool Assembler::MatchMoveDestination(IR::Expression expression, IR::Expression templ,
	int &nodecount, std::list<TemplateChildInfo> *children)
{
	if (templ->kind != expression->kind)
		return false;
	return MatchExpression(expression, templ, nodecount, children);
}

bool Assembler::MatchExpression(IR::Expression expression, IR::Expression templ,
	int &nodecount, std::list<TemplateChildInfo> *children)
{
	if ((templ->kind == IR::IR_REGISTER) && (expression->kind != IR::IR_REGISTER)) {
		// Matched register template against non-register expression
		// Non-register expression gets computed separately and
		// "glued" to the template
		if (children != NULL) {
			children->push_back(TemplateChildInfo(NULL, expression));
			IR::VirtualRegister *value_storage = IRenvironment->addRegister();
			children->back().value_storage = value_storage;
		}
		return true;
	}
	
	if (templ->kind != expression->kind)
		return false;
	
	nodecount++;
	assert(templ->kind != IR::IR_STAT_EXP_SEQ);
	
	switch (templ->kind) {
		case IR::IR_INTEGER:
			if (children != NULL)
				children->push_back(TemplateChildInfo(NULL, expression));
			return (IR::ToIntegerExpression(templ)->value == 0) ||
				(IR::ToIntegerExpression(templ)->value ==
				IR::ToIntegerExpression(expression)->value);
		case IR::IR_LABELADDR:
			if (children != NULL)
				children->push_back(TemplateChildInfo(NULL, expression));
			return true;
		case IR::IR_REGISTER:
			if (children != NULL)
				children->push_back(TemplateChildInfo(NULL, expression));
			return true;
		case IR::IR_FUN_CALL: {
			std::shared_ptr<IR::CallExpression> call_expr = IR::ToCallExpression(expression);
			std::shared_ptr<IR::CallExpression> call_inst = nullptr;
// 			if (children != NULL) {
// 				for (IR::Expression arg: call_expr->arguments) {
// 					children->push_back(TemplateChildInfo(NULL, arg));
// 					if (call_inst != NULL) {
// 						IR::VirtualRegister *arg_reg = IRenvironment->addRegister();
// 						children->back().value_storage = arg_reg;
// 						call_inst->addArgument(std::make_shared<IR::RegisterExpression>(arg_reg));
// 					}
// 				}
// 			}
			return MatchExpression(
				call_expr->function, IR::ToCallExpression(templ)->function,
				nodecount, children);
		}
		case IR::IR_BINARYOP: {
			std::shared_ptr<IR::BinaryOpExpression> binop_expr =
				IR::ToBinaryOpExpression(expression);
			std::shared_ptr<IR::BinaryOpExpression> binop_templ =
				IR::ToBinaryOpExpression(templ);
			
			if (binop_expr->operation != binop_templ->operation)
				return false;
			return MatchExpression(binop_expr->left, binop_templ->left,
					nodecount, children) && 
				MatchExpression(binop_expr->right, binop_templ->right,
					nodecount, children);
		}
		case IR::IR_MEMORY: {
			return MatchExpression(
				IR::ToMemoryExpression(expression)->address,
				IR::ToMemoryExpression(templ)->address,
				nodecount, children);
		}
		default:
			Error::fatalError("Strange expression template kind");
	}
	return false;
}

bool Assembler::MatchStatement(IR::Statement statement, IR::Statement templ,
	int &nodecount, std::list<TemplateChildInfo> *children)
{
	// getTemplatesList takes care of this
	assert(statement->kind == templ->kind);
	
	nodecount++;
	
	switch (statement->kind) {
		case IR::IR_LABEL:
// 			if (template_instantiation != NULL)
// 				*template_instantiation = std::make_shared<IR::LabelPlacementStatement>(
// 					IR::ToLabelPlacementStatement(statement)->label);
			if (children != NULL)
				children->push_back(TemplateChildInfo(NULL,
					std::make_shared<IR::LabelAddressExpression>(
						IR::ToLabelPlacementStatement(statement)->label)
				));
			return true;
		case IR::IR_MOVE: {
			std::shared_ptr<IR::MoveStatement> move_statm = IR::ToMoveStatement(statement);
			std::shared_ptr<IR::MoveStatement> move_templ = IR::ToMoveStatement(templ);
// 			IR::Expression *to_inst = NULL, *from_inst = NULL;
// 			if (template_instantiation != NULL) {
// 				std::shared_ptr<IR::MoveStatement>move_inst =
// 					std::make_shared<IR::MoveStatement>(nullptr, nullptr);
// 				*template_instantiation = move_inst;
// 				to_inst = &move_inst->to;
// 				from_inst = &move_inst->from;
// 			}
			return MatchMoveDestination(move_statm->to, move_templ->to, 
					nodecount, children) &&
				MatchExpression(move_statm->from, move_templ->from, 
					nodecount, children);
		}
		case IR::IR_JUMP: {
// 			IR::Expression *dest_inst = NULL;
// 			if (template_instantiation != NULL) {
// 				std::shared_ptr<IR::JumpStatement> jump_inst =
// 					std::make_shared<IR::JumpStatement>(nullptr);
// 				*template_instantiation = jump_inst;
// 				dest_inst = &jump_inst->dest;
// 			}
			return MatchExpression(
				IR::ToJumpStatement(statement)->dest,
				IR::ToJumpStatement(templ)->dest,
				nodecount, children);
		}
		case IR::IR_COND_JUMP: {
			std::shared_ptr<IR::CondJumpStatement> cj_statm = IR::ToCondJumpStatement(statement);
			std::shared_ptr<IR::CondJumpStatement> cj_templ = IR::ToCondJumpStatement(templ);
			
			// getTemplatesList takes care of this
			assert(cj_statm->comparison == cj_templ->comparison);

// 			IR::Expression *left_inst = NULL, *right_inst = NULL;
// 			if (template_instantiation != NULL) {
// 				std::shared_ptr<IR::CondJumpStatement> cj_inst =
// 					std::make_shared<IR::CondJumpStatement>(
// 						cj_statm->comparison, nullptr, nullptr, cj_statm->true_dest,
// 						cj_statm->false_dest);
// 				*template_instantiation = cj_inst;
// 				left_inst = &cj_inst->left;
// 				right_inst = &cj_inst->right;
// 			}
			bool result = MatchExpression(cj_statm->left, cj_templ->left, 
					nodecount, children) &&
				MatchExpression(cj_statm->right, cj_templ->right, 
					nodecount, children);
			if (children != NULL) {
				children->push_back(TemplateChildInfo(NULL,
					std::make_shared<IR::LabelAddressExpression>(cj_statm->true_dest)));
				children->push_back(TemplateChildInfo(NULL,
					std::make_shared<IR::LabelAddressExpression>(cj_statm->false_dest)));
			}
			return result;
		}
		default:
			Error::fatalError("Strange statement template kind");
	}
	return false;
}

static int dt(timeval t1, timeval t2)
{
	return 1000000*(t2.tv_sec-t1.tv_sec) + (t2.tv_usec-t1.tv_usec);
}

std::shared_ptr<Template> Assembler::FindExpressionTemplate(IR::Expression expression)
{
	std::list<std::shared_ptr<Template> > *templates = getTemplatesList(expression);
	int best_nodecount = -1;
	std::shared_ptr<Template> templ = nullptr;
	struct timeval t1;
	gettimeofday(&t1, NULL);
	for (auto cur_templ: *templates) {
		assert(cur_templ->code->kind == IR::CODE_EXPRESSION);
		IR::Expression templ_exp = std::static_pointer_cast<IR::ExpressionCode>(cur_templ->code)->exp;
		int nodecount = 0;
		if (MatchExpression(expression, templ_exp, nodecount) &&
			((cur_templ->match_fcn == NULL) || (debug("Match fcn: 0x%x", cur_templ->match_fcn), cur_templ->match_fcn(expression)))) {
			if (nodecount > best_nodecount) {
				best_nodecount = nodecount;
				templ = cur_templ;
			}
		}
	}
	struct timeval t2;
	gettimeofday(&t2, NULL);
	finding_time += dt(t1, t2);
	return templ;
}

std::shared_ptr<Template> Assembler::FindStatementTemplate(IR::Statement statement)
{
	std::list<std::shared_ptr<Template> > *templates = getTemplatesList(statement);
	int best_nodecount = -1;
	std::shared_ptr<Template> templ = nullptr;

	struct timeval t1;
	gettimeofday(&t1, NULL);
	for (auto cur_templ: *templates) {
		assert(cur_templ->code->kind == IR::CODE_STATEMENT);
		IR::Statement templ_statm = std::static_pointer_cast<IR::StatementCode>(cur_templ->code)->statm;
		int nodecount = 0;
		if (MatchStatement(statement, templ_statm, nodecount)) {
			if (nodecount > best_nodecount) {
				best_nodecount = nodecount;
				templ = cur_templ;
			}
		}
	}
	struct timeval t2;
	gettimeofday(&t2, NULL);
	finding_time += dt(t1, t2);
	return templ;
}

struct TemplateInput {
	IR::ExpressionKind kind;
	union {
		int ivalue;
		IR::Label *label;
		IR::VirtualRegister *reg;
	};
	int input_index, output_index;
};

void appendRegisters(std::vector<IR::VirtualRegister *> &to,
	const std::vector<IR::VirtualRegister *> &from,
	const std::vector<int> &existing_indices)
{
	assert(existing_indices.size() == from.size());
	unsigned newsize = to.size();
	to.resize(newsize + from.size());
	for (unsigned i = 0; i < from.size(); i++)
		if (existing_indices[i] < 0)
			to[newsize++] = from[i];
	to.resize(newsize);
}

void Template::implement(IR::IREnvironment *ir_env, Instructions &result,
		const std::list<TemplateChildInfo> &elements,
		IR::VirtualRegister *value_storage, const std::vector<IR::Label *> &extra_dest)
{
	Instructions adjusted_instructions;
	Instructions *template_instructions = &this->instructions;
 	if (adjuster_fcn != NULL) {
 		if (adjuster_fcn(instructions, ir_env, adjusted_instructions, elements))
			template_instructions = &adjusted_instructions;
 	}
	std::vector<TemplateInput> input_pool(elements.size());
	int ind = 0;
	for (const TemplateChildInfo &element: elements) {
		if (element.value_storage != NULL) {
			input_pool[ind].kind = IR::IR_REGISTER;
			input_pool[ind].reg = element.value_storage;
		} else {
			input_pool[ind].kind = element.expression->kind;
			switch (element.expression->kind) {
			case IR::IR_INTEGER:
				input_pool[ind].ivalue =
					IR::ToIntegerExpression(element.expression)->value;
				break;
			case IR::IR_LABELADDR:
				input_pool[ind].label =
					IR::ToLabelAddressExpression(element.expression)->label;
				break;
			case IR::IR_REGISTER:
				input_pool[ind].reg =
					IR::ToRegisterExpression(element.expression)->reg;
				break;
			default:
				Error::fatalError("Wrong leaf at the template");
			}
		}
		ind++;
	}
	for (Instruction &inst: *template_instructions) {
		for (TemplateInput &i: input_pool) {
			i.input_index = -1;
			i.output_index = -1;
		}
		
		std::string final_notation = "";
		const char *s = inst.notation.c_str();
		unsigned i = 0;
		unsigned len = inst.notation.size();
		std::vector<IR::VirtualRegister *> inputs, outputs;
		int value_storage_output_index = -1;
		std::vector<int> xinput_indices, xoutput_indices;
		xinput_indices.resize(inst.inputs.size(), -1);
		xoutput_indices.resize(inst.outputs.size(), -1);
		
		while (i < len) {
			unsigned argstart = i;
			while ((argstart < len) && (s[argstart] != '&'))
				argstart++;
			if (argstart+1 < len) {
				unsigned skip_to = argstart+1;
				std::string value = "";
				if (s[argstart+1] == 'o') {
					// Register where we storer the result goes here.
					// If there is none, we'll ignore the instruction (we cannot
					// form it anyway). Don't ditch the entire template though
					// because it could have other side effects than producing 
					// a result.
					if (value_storage_output_index < 0) {
						value_storage_output_index = outputs.size();
						outputs.push_back(value_storage);
					}
					//final_notation += inst.notation.substr(i, argstart-i);
					value = Instruction::Output(value_storage_output_index);
					skip_to = argstart+2;
				} else if ((s[argstart+1] == 'I') && (argstart+2 < len) &&
				            isdigit(s[argstart+2])) {
					int numstart = argstart+2;
					skip_to = numstart;
					while ((skip_to < len) && isdigit(s[skip_to]))
						skip_to++;
					unsigned xinput_index = atoi(inst.notation.substr(numstart,
						skip_to-numstart).c_str());
					assert(xinput_index < inst.inputs.size());
					if (xinput_indices[xinput_index] < 0) {
						xinput_indices[xinput_index] = inputs.size();
						inputs.push_back(inst.inputs[xinput_index]);
					}
					value = Instruction::Input(xinput_indices[xinput_index]);
				} else if ((s[argstart+1] == 'O') && (argstart+2 < len) &&
				            isdigit(s[argstart+2])) {
					int numstart = argstart+2;
					skip_to = numstart;
					while ((skip_to < len) && isdigit(s[skip_to]))
						skip_to++;
					unsigned xoutput_index = atoi(inst.notation.substr(numstart,
						skip_to-numstart).c_str());
					assert(xoutput_index < inst.outputs.size());
					if (xoutput_indices[xoutput_index] < 0) {
						xoutput_indices[xoutput_index] = outputs.size();
						outputs.push_back(inst.outputs[xoutput_index]);
					}
					value = Instruction::Output(xoutput_indices[xoutput_index]);
				} else if (((s[argstart+1] == 'w') && (argstart+2 < len) &&
				            isdigit(s[argstart+2])) || isdigit(s[argstart+1])) {
					bool is_assigned = false;
					int numstart = argstart+1;
					if (s[numstart] == 'w') {
						is_assigned = true;
						numstart++;
					}
					skip_to = numstart;
					while ((skip_to < len) && isdigit(s[skip_to]))
						skip_to++;
					unsigned elem_index = atoi(inst.notation.substr(numstart,
						skip_to-numstart).c_str());
					if (elem_index >= input_pool.size())
						Error::fatalError("Template refers to more inputs than it got");

					switch (input_pool[elem_index].kind) {
					case IR::IR_INTEGER: {
						int v = input_pool[elem_index].ivalue;
						if ((argstart > 0) && (s[argstart-1] == '-')) {
							argstart--;
							v = -v;
						}
						value = IntToStr(v);
						break;
					}
					case IR::IR_LABELADDR:
						value = input_pool[elem_index].label->getName();
						break;
					case IR::IR_REGISTER:
						if (! is_assigned) {
							if (input_pool[elem_index].input_index < 0) {
								input_pool[elem_index].input_index = inputs.size();
								inputs.push_back(input_pool[elem_index].reg);
							}
							value = Instruction::Input(input_pool[elem_index].input_index);
						} else {
							if (input_pool[elem_index].output_index < 0) {
								input_pool[elem_index].output_index = outputs.size();
								outputs.push_back(input_pool[elem_index].reg);
							}
							value = Instruction::Output(input_pool[elem_index].output_index);
						}
						break;
					default:
						value = "?wtf";
					}
					
				} else
					Error::fatalError("Malformed template: " + inst.notation);
				final_notation += inst.notation.substr(i, argstart-i);
				final_notation += value;
				i = skip_to;
			} else {
				final_notation += inst.notation.substr(i, len-i);
				i = len;
			}
		}
		
		if ((value_storage_output_index >= 0) && (value_storage == NULL))
			// Skip the instruction because it generates value that we
			// have nowhere to place
			continue;
		appendRegisters(outputs, inst.outputs, xoutput_indices);
		appendRegisters(inputs, inst.inputs, xinput_indices);
#ifdef DEBUG
		std::string suffix;
		if (inputs.size() > 0) {
			suffix += " Uses:";
			for (IR::VirtualRegister *reg: inputs)
				suffix += " " + reg->getName();
		}
		if (outputs.size() > 0) {
			suffix += " Assigns:";
			for (IR::VirtualRegister *reg: outputs)
				suffix += " " + reg->getName();
		}
		debug("Implement: %s -> %s", inst.notation.c_str(), (final_notation + suffix).c_str());
#endif
		if (! inst.extra_destinations.empty())
			result.push_back(Instruction(final_notation, inputs, outputs,
				inst.is_reg_to_reg_assign, extra_dest, inst.jumps_to_next));
		else
			result.push_back(Instruction(final_notation, inputs, outputs,
				inst.is_reg_to_reg_assign, {}, inst.jumps_to_next));
	}
}


void Assembler::translateExpression(IR::Expression expression,
	IR::AbstractFrame *frame, IR::VirtualRegister* value_storage, 
	Instructions& result)
{
	if (expression->kind == IR::IR_STAT_EXP_SEQ) {
		std::shared_ptr<IR::StatExpSequence> statexp = IR::ToStatExpSequence(expression);
		translateStatement(statexp->stat, frame, result);
		translateExpression(statexp->exp, frame, value_storage, result);
	} else {
		std::shared_ptr<Template> templ = FindExpressionTemplate(expression);
		if (! templ)
			Error::fatalError("Failed to find expression template");
		
		std::list<TemplateChildInfo> children;
		assert(templ->code->kind == IR::CODE_EXPRESSION);
		int nodecount = 0;
		MatchExpression(expression,
			std::static_pointer_cast<IR::ExpressionCode>(templ->code)->exp,
			nodecount, &children);
		for (TemplateChildInfo &child: children)
			if (child.value_storage != NULL)
				translateExpression(child.expression, frame, child.value_storage,
					result);
		
		std::list<IR::Expression> immediate_arguments;
		if (expression->kind == IR::IR_FUN_CALL) {
			for (IR::Expression arg: IR::ToCallExpression(expression)->arguments) {
				IR::VirtualRegister *arg_reg = IRenvironment->addRegister();
				translateExpression(arg, frame, arg_reg, result);
				immediate_arguments.push_back(
					std::make_shared<IR::RegisterExpression>(arg_reg));
			}
			placeCallArguments(immediate_arguments, frame,
				IR::ToCallExpression(expression)->callee_parentfp, result);
		}
		templ->implement(this->IRenvironment, result, children, value_storage);
		if (expression->kind == IR::IR_FUN_CALL)
			removeCallArguments(immediate_arguments, result);
		//translateExpressionTemplate(template_instantiation, frame, value_storage,
		//	children, result);
	}
}

void Assembler::translateStatement(IR::Statement statement, 
	IR::AbstractFrame *frame, Instructions& result)
{
	if (statement->kind == IR::IR_STAT_SEQ) {
		std::shared_ptr<IR::StatementSequence> seq = IR::ToStatementSequence(statement);
		for (IR::Statement statm: seq->statements)
			 translateStatement(statm, frame, result);
	} else if (statement->kind == IR::IR_EXP_IGNORE_RESULT) {
		translateExpression(IR::ToExpressionStatement(statement)->exp, 
			frame, NULL, result);
	} else {
		std::shared_ptr<Template> templ = FindStatementTemplate(statement);
		if (! templ)
			Error::fatalError("Failed to find statement template");
		
		std::list<TemplateChildInfo> children;
		assert(templ->code->kind == IR::CODE_STATEMENT);
		int nodecount = 0;
		MatchStatement(statement,
			std::static_pointer_cast<IR::StatementCode>(templ->code)->statm,
			nodecount, &children);
		for (TemplateChildInfo &child: children)
			if (child.value_storage != NULL)
				translateExpression(child.expression, frame, child.value_storage,
					result);
				
		std::vector<IR::Label *> extra_dest;
		if (statement->kind == IR::IR_JUMP) {
			std::list<IR::Label *> labellist =
				IR::ToJumpStatement(statement)->possible_results;
			extra_dest.resize(labellist.size());
			int i = 0;
			for (IR::Label *label: labellist)
				extra_dest[i++] = label;
		} else if (statement->kind == IR::IR_COND_JUMP) {
			extra_dest.push_back(IR::ToCondJumpStatement(statement)->true_dest);
		}
			
		templ->implement(this->IRenvironment, result, children, NULL, extra_dest);
		//translateStatementTemplate(template_instantiation, children, result);
		if (statement->kind == IR::IR_LABEL)
			result.back().label = IR::ToLabelPlacementStatement(statement)->label;
	}
}

void Assembler::outputCode(FILE* output, const std::list<Instructions>& code,
	const IR::RegisterMap *register_map)
{
	std::string header;
	getCodeSectionHeader(header);
	fwrite(header.c_str(), header.size(), 1, output);
	if ((header.size() > 0) && (header[header.size()-1] != '\n'))
		fputc('\n', output);
	
	for (const Instructions &chunk: code) {
		int line = -1;
		for (const Instruction &inst: chunk) {
			line++;
			const std::string &s = inst.notation;
			
			int len = 0;
			if ((s.size() > 0) && (s[s.size()-1] != ':')) {
				fputs("    ", output);
				len += 4;
			}
		
			unsigned i = 0;
			while (i < s.size()) {
				if (s[i] != '&') {
					fputc(s[i], output);
					len++;
					i++;
				} else {
					i++;
					if (i+1 < s.size()) {
						int arg_index = s[i+1] - '0';
						const std::vector<IR::VirtualRegister *> *registers = NULL;
						
						if (s[i] == 'i')
							registers = &inst.inputs;
						else if (s[i] == 'o')
							registers = &inst.outputs;
						else
							Error::fatalError("Misformed instruction");
						
						if ((unsigned)arg_index >= (*registers).size())
							Error::fatalError("Misformed instruction");
						else {
							IR::VirtualRegister *reg =
								MapRegister(register_map, (*registers)[arg_index]);
							fprintf(output, "%s", reg->getName().c_str());
							len += reg->getName().size();
						}
						i += 2;
					}
				}
			}
			for (int i = 0; i < 35-len; i++)
				fputc(' ', output);
			fprintf(output, " # %2d ", line);
			if (inst.is_reg_to_reg_assign)
				fprintf(output, "MOVE ");
			bool use_reg = false;
			if (inst.inputs.size() > 0) {
				use_reg = true;
				fprintf(output, "<- ");
				for (unsigned i = 0; i < inst.inputs.size(); i++)
					fprintf(output, "%s ", inst.inputs[i]->getName().c_str());
			}
			if (inst.outputs.size() > 0) {
				use_reg = true;
				fprintf(output, "-> ");
				for (unsigned  i = 0; i < inst.outputs.size(); i++)
					fprintf(output, "%s ", inst.outputs[i]->getName().c_str());
			}
			if (! use_reg)
				fprintf(output, "no register use");
			fputc('\n', output);
		}
	}
}

void Assembler::outputBlobs(FILE* output, const std::list< IR::Blob >& blobs)
{
	Instructions content;
	for (const IR::Blob &blob: blobs)
		translateBlob(blob, content);
	
	std::string header;
	getBlobSectionHeader(header);
	fwrite(header.c_str(), header.size(), 1, output);
	if ((header.size() > 0) && (header[header.size()-1] != '\n'))
		fputc('\n', output);
	
	for (Instruction &inst: content)
		fprintf(output, "%s\n", inst.notation.c_str());
}

void Assembler::translateFunctionBody(IR::Code code, IR::Label *fcn_label,
	IR::AbstractFrame *frame, Instructions &result)
{
	if (code == NULL)
		// External function, ignore
		return;
	switch (code->kind) {
		case IR::CODE_EXPRESSION: {
			IR::VirtualRegister *result_storage = IRenvironment->addRegister();
			std::vector<IR::VirtualRegister *> prologue_regs;
			functionPrologue(fcn_label, frame, result, prologue_regs);
			translateExpression(std::static_pointer_cast<IR::ExpressionCode>(code)->exp,
				frame, result_storage, result);
			functionEpilogue(frame, result_storage, prologue_regs, result);
			break;
		}
		case IR::CODE_STATEMENT: {
			std::vector<IR::VirtualRegister *> prologue_regs;
			functionPrologue(fcn_label, frame, result, prologue_regs);
			translateStatement(std::static_pointer_cast<IR::StatementCode>(code)->statm,
				frame, result);
			functionEpilogue(frame, NULL, prologue_regs, result);
			break;
		}
		default:
			Error::fatalError("Assembler::translateFunctionBody strange body code");
	}
}


void Assembler::translateProgram(IR::Statement program, IR::AbstractFrame *frame,
	Instructions& result)
{
	translateStatement(program, frame, result);
}

void Assembler::implementProgramFrameSize(IR::AbstractFrame *frame,
	Instructions &result)
{
	programPrologue(frame, result);
	programEpilogue(frame, result);
	implementFramePointer(frame, result);
}

void Assembler::implementFunctionFrameSize(IR::Label *fcn_label,
	IR::AbstractFrame *frame, Instructions &result)
{
	framePrologue(fcn_label, frame, result);
	frameEpilogue(frame, result);
		implementFramePointer(frame, result);
}

}
