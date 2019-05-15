#include "server_functions.h"

int checkArgs(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage: ./server <nBankOffices> \"<passwordAdmin>\"\n");
        return 1;
    }

    int nBankOffices = atoi(argv[1]);
    if (nBankOffices <= 0)
    {
        printf("Invalid nBanckOffices\n");
        return 1;
    }

    if (nBankOffices > MAX_BANK_OFFICES)
    {
        printf("Too many bank offices!\n");
        return 1;
    }

    return 0;
}
