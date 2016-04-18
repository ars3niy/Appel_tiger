#include "frame.h"

namespace IR {

void VarLocation::prespillRegister()
{
	if (reg != NULL)
		reg->prespill(owner_frame->createMemoryVariable(
			".store." + reg->getName(), size));
}

Expression VarLocation::createCode(AbstractFrame *calling_frame)
{
	if (isRegister()) {
		if (calling_frame->getId() == owner_frame->getId())
			return std::make_shared<RegisterExpression>(reg);
		else {
			if (! reg->isPrespilled()) {
				debug("Prespilling register %s: owner frame %s, accessed from %s",
					reg->getName().c_str(), owner_frame->getName().c_str(),
					calling_frame->getName().c_str());
				prespillRegister();
			}
			return reg->getPrespilledLocation()->createCode(calling_frame);
		}
	}
	
	std::list<AbstractFrame *> frame_stack;
	AbstractFrame *frame = calling_frame;
	while (frame != NULL) {
		frame_stack.push_front(frame);
		if (frame == owner_frame)
			break;
		frame = frame->getParent();
	}
	if (frame != owner_frame)
		Error::fatalError("Variable has been used outside its function without syntax error??");
	
	Expression result = nullptr;
	Expression *put_access_to_owner_frame = &result;
	
	for (AbstractFrame *frame: frame_stack) {
		VarLocation *var_location;
		if (frame->getId() == owner_frame->getId())
			var_location = this;
		else if (frame->getId() == calling_frame->getId()) {
			if (frame->getParentFpForUs() == NULL) {
				debug("Frame %s needs parent FP because it uses variable %s of parent frame %s",
					frame->getName().c_str(), this->name.c_str(),
					owner_frame->getName().c_str());
				frame->addParentFpParameter();
			}
			var_location = frame->getParentFpForUs();
		} else {
			if (frame->getParentFpForUs() == NULL) {
				debug("Frame %s needs parent FP because its child %s uses variable %s of its parent frame %s",
					frame->getName().c_str(), calling_frame->getName().c_str(),
					this->name.c_str(),	owner_frame->getName().c_str());
				frame->addParentFpParameter();
			}
			var_location = frame->getParentFpForChildren();
		}
				
		assert(var_location != NULL);
		assert(put_access_to_owner_frame != NULL);
	
		Expression access_to_owner_frame;
		Expression *put_access_to_next_owner_frame;
		if (var_location->isRegister() &&
				(frame->getId() == calling_frame->getId())) {
			access_to_owner_frame = var_location->createCode(frame);
			put_access_to_next_owner_frame = NULL;
		} else {
			if (var_location->isRegister()) {
				if (! var_location->getRegister()->isPrespilled()) {
					debug("Prespilling register %s: owner frame %s, accessed from %s",
						var_location->getRegister()->getName().c_str(),
						frame->getName().c_str(), calling_frame->getName().c_str());
					var_location->prespillRegister();
				}
				var_location = var_location->getRegister()->getPrespilledLocation();
			}
			assert(! var_location->isRegister());
			std::shared_ptr<BinaryOpExpression> address_expr = std::make_shared<BinaryOpExpression>(
				IR::OP_PLUS, nullptr, std::make_shared<IntegerExpression>(var_location->offset));
			access_to_owner_frame = std::make_shared<MemoryExpression>(address_expr);
			put_access_to_next_owner_frame = &address_expr->left;
		}
		
		*put_access_to_owner_frame = access_to_owner_frame;
		put_access_to_owner_frame = put_access_to_next_owner_frame;
	}
	if (put_access_to_owner_frame != NULL) {
		*put_access_to_owner_frame = std::make_shared<RegisterExpression>(
			calling_frame->getFramePointer());
	}
	assert(result != NULL);
	return result;
}

VarLocation* AbstractFrame::addVariable(const std::string &name,
	int size)
{
	VarLocation *impl = new VarLocation(this, size,
		framemanager->getIREnvironment()->addRegister(this->name + "::" + name), false);
	variables.push_back(impl);
	return impl;
}

VarLocation* AbstractFrame::addParameter(const std::string &name,
	int size)
{
	VarLocation *impl = createParameter(name, size);
	parameters.push_back(impl);
	return impl;
}

void AbstractFrame::addParentFpParameter()
{
	framemanager->notifyFrameWithParentFp(this);
	parent_fp_parameter = createParentFpParameter(".parent_fp");
	assert(parent_fp_parameter->getOwnerFrame() == this);
}

VarLocation *AbstractFrame::getParentFpForChildren() {
	if (parent_fp_parameter == NULL)
		return NULL;
	if (parent_fp_parameter->isRegister()) {
		if (! parent_fp_parameter->getRegister()->isPrespilled()) {
			debug("Prespilling frame %s parent FP register %s, accessed by child function",
				getName().c_str(), parent_fp_parameter->getRegister()->getName().c_str());
			parent_fp_parameter->prespillRegister();
		}
		return parent_fp_parameter->getRegister()->getPrespilledLocation();
	} else
		return parent_fp_parameter;
}

void AbstractFrame::prespillRegisters(Expression &exp,
	const std::map<int, VarLocation *> &spills)
{
	switch (exp->kind) {
	case IR_REGISTER: {
		auto spill = spills.find(ToRegisterExpression(exp)->reg->getIndex());
		if (spill != spills.end()) {
			assert (spill->second->getOwnerFrame()->getId() == getId());
			exp = spill->second->createCode(this);
		}
		break;
	}
	case IR_BINARYOP: {
		auto binop = ToBinaryOpExpression(exp);
		prespillRegisters(binop->left, spills);
		prespillRegisters(binop->right, spills);
		break;
	}
	case IR_MEMORY:
		prespillRegisters(ToMemoryExpression(exp)->address, spills);
		break;
	case IR_FUN_CALL: {
		auto call = ToCallExpression(exp);
		prespillRegisters(call->function, spills);
		for (Expression &arg: call->arguments)
			prespillRegisters(arg, spills);
		break;
	}
	case IR_STAT_EXP_SEQ: {
		auto seq = ToStatExpSequence(exp);
		prespillRegisters(seq->stat, spills);
		prespillRegisters(seq->exp, spills);
		break;
	}
	default: ;
	}
}

void AbstractFrame::prespillRegisters(Statement statm,
	const std::map<int, VarLocation *> &spills)
{
	switch (statm->kind) {
	case IR_MOVE: {
		auto move = ToMoveStatement(statm);
		prespillRegisters(move->from, spills);
		prespillRegisters(move->to, spills);
		break;
	}
	case IR_EXP_IGNORE_RESULT: {
		auto expstatm = ToExpressionStatement(statm);
		// Don't need to spill the register if its value is ignored
		if (expstatm->exp->kind != IR_REGISTER)
			prespillRegisters(expstatm->exp, spills);
		break;
	}
	case IR_JUMP:
		prespillRegisters(ToJumpStatement(statm)->dest, spills);
		break;
	case IR_COND_JUMP: {
		auto condjump = ToCondJumpStatement(statm);
		prespillRegisters(condjump->left, spills);
		prespillRegisters(condjump->right, spills);
		break;
	}
	case IR_STAT_SEQ:
		for (Statement element: ToStatementSequence(statm)->statements)
			prespillRegisters(element, spills);
		break;
	default: ;
	}
}

AbstractFrame::AbstractFrame(AbstractFrameManager *_framemanager,
	const std::string &_name, int _id, AbstractFrame *_parent) :
	DebugPrinter("translator.log"),
	framemanager(_framemanager), name(_name), id(_id), parent(_parent),
	calls_others(false), parent_fp_parameter(NULL)
{
	framepointer = framemanager->getIREnvironment()->addRegister("fp");
}

AbstractFrame::~AbstractFrame()
{
	for (VarLocation *var: variables)
		delete var;
	for (VarLocation *var: parameters)
		delete var;
	delete parent_fp_parameter;
}

}
