#include "sope.h"
#include <stdlib.h>
#include <string.h>

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

bool checkInputs(int argc, char *argv[])
{
    if (argc != 6)
    {
        printf("Usage: user <accountID> <password> <op_delay_ms> <op_type> <arguments_list>\n");
        return false;
    }

    if (atoi(argv[1]) > MAX_BANK_ACCOUNTS || atoi(argv[1]) <= ADMIN_ACCOUNT_ID)
    {
        printf("Account ID must be between 1 and 4096\n");
        return false;
    }

    if (strlen(argv[2]) > MAX_PASSWORD_LEN || strlen(argv[2]) < MIN_PASSWORD_LEN)
    {
        printf("Password must have betweern 8 and 20 letters\n");
        return false;
    }

    if (atoi(argv[3]) > MAX_OP_DELAY_MS)
    {
        printf("Operation delay value invalid, must be <= 99999\n");
        return false;
    }

    if (atoi(argv[4]) < 0 || atoi(argv[4]) > 3)
    {
        printf("Operation type invalid, must be betwwen 0 and 3\n");
        return false;
    }

    if (atoi(argv[4]) == 1 || atoi(argv[4]) == 3)
    {
        if (strlen(argv[5]) != 0)
        {
            printf("Operations 1 & 3 don't receive arguments\n");
            return false;
        }
    }
    // Criar conta
    else if (atoi(argv[4]) == 0)
    {
        int n; //number of words
        char arr[10][20];

        n = getWords(argv[5], arr);

        if (n + 1 != 3)
        {
            printf("Must receive 3 arguments\n");
            return false;
        }

        // Get new_account_ID
        if (atoi(arr[0]) > MAX_BANK_ACCOUNTS || atoi(arr[0]) <= ADMIN_ACCOUNT_ID)
        {
            printf("Account ID must be between 1 and 4096\n");
            return false;
        }

        // Inital balance
        if (atoi(arr[1]) > MAX_BALANCE || atoi(arr[1]) < MIN_BALANCE)
        {
            printf("Initial balance must be between 1 and 1000000000\n");
            return false;
        }

        // Password
        if (strlen(arr[2]) > MAX_PASSWORD_LEN || strlen(arr[2]) < MIN_PASSWORD_LEN)
        {
            printf("Password must have betweern 8 and 20 letters\n");
            return false;
        }
    }
    else
    {
        int n; //number of words
        char arr[10][20];

        n = getWords(argv[5], arr);

        if (n + 1 != 2)
        {
            printf("Must receive 2 arguments\n");
            return false;
        }

        // Get Destination account_ID
        if (atoi(arr[0]) > MAX_BANK_ACCOUNTS || atoi(arr[0]) <= ADMIN_ACCOUNT_ID)
        {
            printf("Account ID must be between 1 and 4096\n");
            return false;
        }

        // Value
        if (atoi(arr[1]) > MAX_BALANCE || atoi(arr[1]) < MIN_BALANCE)
        {
            printf("Initial balance must be between 1 and 1000000000\n");
            return false;
        }
    }

    return true;
}

int main(int argc, char *argv[], char *envp[])
{
    if (!checkInputs(argc, argv))
        return 1;

    int accountID = atoi(argv[1]);
    char password[20];
    strcpy(password, argv[2]);
    int op_delay_ms = atoi(argv[3]);
    int op_type = atoi(argv[4]);

    // Criar conta
    if (op_type == 0)
    {
        char arr[10][20];
        getWords(argv[5], arr);

        int id_new_account = arr[0];
        int initial_balance = arr[1];
        char account_password[20];
        strcpy(account_password, arr[2]);
    }
    // Consulta de saldo
    else if (op_type == 1)
    {
    }
    // Transferencia
    else if (op_type == 2)
    {
        char arr[10][20];
        getWords(argv[5], arr);

        int id_desitnation_account = arr[0];
        int value = arr[1];
    }
    // Encerramento
    else if (op_type == 3)
    {
    }

    return 0;
}