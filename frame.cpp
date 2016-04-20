#include "frame.h"

namespace IR {

VarLocation::VarLocation(AbstractFrame *_frame, int _size, int _offset,
		const std::string &_name, bool _predefined) :
	DebugPrinter("translator.log"),
	id(_frame->getNewVarId()),
	owner_frame(_frame),
	reg(NULL),
	offset(_offset),
	size(_size),
	name(_name),
	predefined(_predefined),
	read_only(false)
{}

VarLocation::VarLocation(AbstractFrame *_frame, int _size, IR::VirtualRegister *_reg,
		bool _predefined) :
	DebugPrinter("translator.log"),
	id(_frame->getNewVarId()),
	owner_frame(_frame),
	reg(_reg),
	offset(-1),
	size(_size),
	name(_reg->getName()),
	predefined(_predefined),
	read_only(false)
{}

void VarLocation::prespillRegister()
{
	if (reg != NULL)
		reg->prespill(owner_frame->addMemoryVariable(
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

int AbstractFrame::getNewVarId()
{
	return framemanager->getNewVarId();
}

VarLocation *AbstractFrame::addMemoryVariable(const std::string &name, int size)
{
	VarLocation *result = createMemoryVariable(name, size);
	variables.push_back(result);
	return result;
}

VarLocation* AbstractFrame::addVariable(const std::string &name,
	int size)
{
	VirtualRegister *reg;
	if (name == "")
		reg = framemanager->getIREnvironment()->addRegister();
	else
		reg = framemanager->getIREnvironment()->addRegister(this->name + "::" + name);
	VarLocation *impl = new VarLocation(this, size, reg, false);
	variables.push_back(impl);
	nvariable++;
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

AbstractFrame::AbstractFrame(AbstractFrameManager *_framemanager,
	const std::string &_name, int _id, AbstractFrame *_parent) :
	DebugPrinter("translator.log"),
	framemanager(_framemanager), name(_name), id(_id), parent(_parent),
	calls_others(false), parent_fp_parameter(NULL)
{
	if (_parent != NULL)
		_parent->has_children = true;
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

AbstractFrame *AbstractFrameManager::newFrame(AbstractFrame *parent, const std::string &name)
{
	AbstractFrame *result = createFrame(parent, name);
	frames.push_back(result);
	return result;
}

AbstractFrameManager::~AbstractFrameManager()
{
	for (AbstractFrame *frame: frames)
		delete frame;
}

}
