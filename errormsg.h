#ifndef _ERRORMSG_H
#define _ERRORMSG_H

#include <string>

namespace Error {

struct InputPos {
	int linenumber;
};

extern InputPos DEFAULT_POS;

void global_error(const std::string &message);

void setFileName(const std::string &filename);
void newline();
void error(const std::string &message, const InputPos &position = DEFAULT_POS);
void warning(const std::string &message, const InputPos &position = DEFAULT_POS);
void fatalError(const std::string &message, const InputPos &position = DEFAULT_POS);
const InputPos &getPosition();

int getErrorCount();

}

#endif
