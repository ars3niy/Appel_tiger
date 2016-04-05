#ifndef _ERRORMSG_H
#define _ERRORMSG_H

#include <string>

namespace Error {

void global_error(const std::string &message);

void setFileName(const std::string &filename);
void newline();
void error(const std::string &message, int _linenumber = -1);
void warning(const std::string &message, int _linenumber = -1);
void fatalError(const std::string &message, int _linenumber = -1);
int getLineNumber();

int getErrorCount();

}

#endif
