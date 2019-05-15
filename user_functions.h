#pragma once

#include "sope.h"

// Funcao auxiliar pra separar words de uma string
// adaptada de https://www.includehelp.com/code-snippets/c-program-to-split-string-into-the-words-separated-by-spaces.aspx
int getWords(char *base, char target[10][20]);

bool checkOperation(int argc, char *argv[]);

bool checkInputs(int argc, char *argv[]);

tlv_reply_t createReply (char *argv[], ret_code_t ret_code);

tlv_request_t createRequest(char *argv[]);

const char * getUserFifoPath(int pid);