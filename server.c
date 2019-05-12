#include "sope.h"
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// Account stuff ---------------------------------------------------------------------------------------------------------------------------------

bank_account_t contasBancarias[MAX_BANK_ACCOUNTS];//Estrutura que guarda as contas bancarias

void addBankAccount(bank_account_t acc)
{
    contasBancarias[acc.account_id] = acc;
}

int checkArgs(int argc, char *argv[])
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

char * echoSha256sum (const char * password, const char * salt)
{
    FILE * temp;
    char command[180]; //echo -n "<password><salt>" | sha256sum
    char * resultado = malloc(65);
    char concatStrings [128];

    strcpy(concatStrings, password);
    strcat(concatStrings, salt);

    strcpy(command, "echo -n ");
    strcat(command, "\"");
    strcat(command, concatStrings);
    strcat(command, "\"");
    strcat(command, "| sha256sum");

    temp = popen(command, "r");
    fgets(resultado, 65, temp);
    return resultado;
}

char * getSha256sumOf (const char * word)
{
    FILE * temp;
    char command[180]; //echo -n "<word>" | sha256sum
    char * resultado = malloc(65);

    strcpy(command, "echo -n \"");
    strcat(command, word);
    strcat(command, "\"");
    strcat(command, "| sha256sum");

    temp = popen(command, "r");
    fgets(resultado, 65, temp);
    return resultado;
}

char * getNewSaltNumber(int size)
{
    char * salt = malloc(size);
    char * availableChars = "0123456789abcdef";

    srand((unsigned int) time(NULL) + getpid()*getppid());

    for(size_t i = 0; i < size; i++)
    {
        salt[i] = availableChars[rand() % sizeof(availableChars)];
        srand(rand());
    }

    return salt;
}

//TODO: VERIFICAR DADOS CLIENTE?
void createAccount(const uint32_t id, const uint32_t initialBalance, const char * passwordAdmin) //Assume que os valores estao dentro das normas
{
    bank_account_t conta;
    conta.account_id = id;
    conta.balance = initialBalance;

    //Salty part *-*
    char saltyNumber[SALT_LEN + 1];
    strcpy(saltyNumber, getNewSaltNumber(SALT_LEN));

    strcpy(conta.salt, saltyNumber);

    //Password part
    char passHash[HASH_LEN + 1];
    strcpy(passHash, getSha256sumOf(passwordAdmin));

    strcpy(conta.hash, passHash);

    //Adding account
    addBankAccount(conta);

    //Debug only
    //printf("ID = <%d>\n", conta.account_id);
    //printf("Bal = <%d>\n", conta.balance);
    //printf("Hash = <%s>\n", conta.hash);
    //printf("Salt = <%s>\n", conta.salt);
}

//------------------------------------------------------------------------------------------------------------------------------------------------

//Bank Offices stuff------------------------------------------------------------------------------------------------------------------------------

//TODO: CHANGE FUNCTION (and args) OF THREADS
void * threadFunc(void * arg)
{
    return NULL;
}

void initializeBankOffices(pthread_t listBankOffices[], const size_t nBankOffices)
{
    /* 
    *
    * Espaco vazio, para lembrar que aqui e necessario adicionar codigo para log da criacao do bankOffice
    * 
    * 
    */

    for(size_t i = 0; i < nBankOffices; i++)
    {
        pthread_t temp;
        pthread_create(&temp, NULL, threadFunc, NULL); //TODO: CHANGE FUNCTION (and args) OF THREADS
        listBankOffices[i] = temp;
    }

}

void waitForAllThreads(pthread_t listTids[], const size_t nBankOffices)
{
    for(size_t i = 0; i < nBankOffices; i++)
    {
        pthread_join(listTids[i], NULL);
    }

    printf("[DEBUG ONLY] All threads closed!\n");
}

//------------------------------------------------------------------------------------------------------------------------------------------------

//FIFO related functions--------------------------------------------------------------------------------------------------------------------------

void initFIFO(char * path, mode_t mode)
{
    mkfifo(path, mode);
}

int initAndOpenFIFO(char * path, mode_t mode, int flags)
{
    initFIFO(path, mode);
    return open(path, flags);
}

void closeFD(int fd)
{
    close(fd);
}

//------------------------------------------------------------------------------------------------------------------------------------------------
int main (int argc, char *argv[], char *envp[])
{

    if(checkArgs(argc, argv)) //This func returns 1 if there's any problem with the arguments
        return 1;             //If that happens, the program closes.

    printf("Server running!\n");

    char password[MAX_PASSWORD_LEN];
    memcpy(password, &argv[2][0], sizeof(argv[2]) - 2); //Retira aspas; TODO: FUNCAO?
    password[MAX_PASSWORD_LEN] = '\0';

    createAccount(ADMIN_ACCOUNT_ID, 0, password); //Cria conta do admin


    size_t nBankOffices = atoi(argv[1]);
    pthread_t activeBankOfficesList[nBankOffices];//AKA activeThreadsList; Thread id 1 => activeBankOfficesList[0];

    initializeBankOffices(activeBankOfficesList, nBankOffices); //activeThreads created, running and tids loaded to activeBankOfficesList
    waitForAllThreads(activeBankOfficesList, nBankOffices);


    //commented in order for the program not be stuck waiting for the user to write


    //int fifoFD;
    //fifoFD = initAndOpenFIFO(SERVER_FIFO_PATH, FIFO_READ_WRITE_ALL_PERM, O_RDONLY); //Allows the user to insert commands; 
    



    //closeFD(fifoFD); //Should only be executed once the admin enters de command to end server.
    printf("Server's dead.\n");
    return 0;
}