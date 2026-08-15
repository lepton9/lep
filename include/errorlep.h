#ifndef ERRORLEP_H
#define ERRORLEP_H

#include "../include/lexer.h"
#include "../include/parser.h"
#include "../include/symtab.h"

#define ERR_OUT stderr


void error_lex(Lexer* l, const char* msg);
void error_parse(parser* p, const char *msg, const char* expected);
void error_semantic(const char* msg, cLoc loc);

void error_ret(const char* msg, const char* id);
void error_redef(const char* id, const TYPE type, const cLoc loc);
void error_fnotfound(const char* id, const cLoc loc);
void error_nodecl(const char* msg, const char* id, const cLoc loc);
void error_value(const char* id, const TYPE type, const cLoc loc);
void error_type(const char* msg, const cLoc loc, const TYPE actual, const TYPE expected);

#endif
