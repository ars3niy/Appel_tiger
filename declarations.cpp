#include "declarations.h"

namespace Semantic {

Type *Type::resolve()
{
	Type *result = this;
	while (result->basetype == TYPE_NAMEREFERENCE) {
		if (((ForwardReferenceType *)result)->meaning == NULL)
			Error::fatalError(std::string("Found stray unresolved type ") +
				((ForwardReferenceType *)result)->name,
				((ForwardReferenceType *)result)->reference_position->linenumber);
		result = ((ForwardReferenceType *)result)->meaning;
	}
	return result;
}

}
