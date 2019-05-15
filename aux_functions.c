#include "aux_functions.h"

int getWords(char *base, char target[10][20])
{
    int n = 0, i, j = 0;

    for (i = 0; 1; i++)
    {
        if (base[i] != ' ')
        {
            target[n][j++] = base[i];
        }
        else
        {
            target[n][j++] = '\0'; //insert NULL
            n++;
            j = 0;
        }
        if (base[i] == '\0')
            break;
    }
    return n;
}

char *getNewSaltNumber(int size)
{
    char *salt = malloc(size);
    char *availableChars = "0123456789abcdef";

    srand((unsigned int)time(NULL) + getpid() * getppid());

    for (size_t i = 0; i < size; i++)
    {
        salt[i] = availableChars[rand() % sizeof(availableChars)];
        srand(rand());
    }

    return salt;
}

char *getSha256sumOf(const char *word)
{
    FILE *temp;
    char command[180]; //echo -n "<word>" | sha256sum
    char *resultado = malloc(65);

    strcpy(command, "echo -n \"");
    strcat(command, word);
    strcat(command, "\"");
    strcat(command, "| sha256sum");

    temp = popen(command, "r");
    fgets(resultado, 65, temp);
    return resultado;
}

char *echoSha256sum(const char *password, const char *salt)
{
    FILE *temp;
    char command[180]; //echo -n "<password><salt>" | sha256sum
    char *resultado = malloc(HASH_LEN + 1);
    char concatStrings[128];

    strcpy(concatStrings, password);
    strcat(concatStrings, salt);

    strcpy(command, "echo -n ");
    strcat(command, "\"");
    strcat(command, concatStrings);
    strcat(command, "\"");
    strcat(command, "| sha256sum");

    temp = popen(command, "r");
    fgets(resultado, HASH_LEN + 1, temp);

    //printf("[ECHO] Salt: <%s>\n", salt);
    //printf("[ECHO] Hash: <%s>\n", resultado);

    return resultado;
}

const char *getUserFifoPath(int pid)
{

    char *USER_FIFO_PATH = malloc(USER_FIFO_PATH_LEN);
    strcpy(USER_FIFO_PATH, USER_FIFO_PATH_PREFIX);
    char pidChar[WIDTH_ID];
    sprintf(pidChar, "%d", pid);
    strcat(USER_FIFO_PATH, pidChar);

    return USER_FIFO_PATH;
}

void initFIFO(const char *path, mode_t mode)
{
    mkfifo(path, mode);
}

int initAndOpenFIFO(const char *path, mode_t mode, int flags)
{
    initFIFO(path, FIFO_READ_WRITE_ALL_PERM);
    return open(path, mode, flags);
}

void closeFD(int fd)
{
    close(fd);
}