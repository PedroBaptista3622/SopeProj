#pragma once

#include "sope.h"
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

// Funcao auxiliar pra separar words de uma string
// adaptada de https://www.includehelp.com/code-snippets/c-program-to-split-string-into-the-words-separated-by-spaces.aspx
int getWords(char *base, char target[10][20]);

// Salt related functions

char *getNewSaltNumber(int size);

char *getSha256sumOf(const char *word);

char *echoSha256sum(const char *password, const char *salt);

// FIFO related functions

const char * getUserFifoPath(int pid);

void initFIFO(const char *path, mode_t mode);

int initAndOpenFIFO(const char *path, mode_t mode, int flags);

void closeFD(int fd);

void removeFileFromPath(char * path);
