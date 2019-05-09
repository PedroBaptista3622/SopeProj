#include "sope.h"
#include <stdlib.h>

int checkArgs(const int argc, const char *argv[])
{
    if(argc != 3)
    {
        printf("Usage: ./server <nBankOffices> \"<passwordAdmin>\"\n");
        return 1;
    }

    int nBankOffices = atoi(argv[1]);
    if(nBankOffices <= 0)
    {
        printf("Invalid nBanckOffices\n");
        return 1;
    }

    if(nBankOffices > MAX_BANK_OFFICES)
    {
        printf("Too many bank offices!\n");
        return 1;
    }

    return 0;
}


int main (int argc, char *argv[], char *envp[])
{
    if(checkArgs(argc, argv)) //This func returns 1 if there's any problem with the arguments
        return 1;             //If that happens, the program closes.

    printf("Server running!\n");

    printf("Server's dead.\n");
    return 0;
}