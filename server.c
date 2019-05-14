#include "sope.h"
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexAccounts = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t requestAdded = PTHREAD_COND_INITIALIZER;
int nThreadsWaitingForRequests = 0;
int serverLogFD;

void createAccount(const uint32_t id, const uint32_t initialBalance, const char * password);
void reply(pid_t pid, uint32_t accID, int answer);
int initAndOpenFIFO(char * path, mode_t mode, int flags);


//Request / reply related functions-----------------------------------------------------------------------------------------------------------------------
tlv_request_t requestQueue[MAX_REQUEST_QUEUE_LENGTH];
int nCurrentWaitingRequests = 0;
int firstQueueElement = 0;
int rear = -1;

tlv_request_t peekFirstQueueElement()
{
    pthread_mutex_lock(&mutex);
    return requestQueue[firstQueueElement];
    pthread_mutex_unlock(&mutex);
}

int getQueueSize()
{
    return nCurrentWaitingRequests;
}

bool isFull()
{
    return getQueueSize() == MAX_REQUEST_QUEUE_LENGTH;
}

void addRequestToQueue(tlv_request_t request)
{
    pthread_mutex_lock(&mutex);

    if(!isFull())
    {
        if(rear == MAX_REQUEST_QUEUE_LENGTH -1)
        {
            rear = -1;
        }

        requestQueue[++rear] = request;
        nCurrentWaitingRequests++;
    }

    pthread_mutex_unlock(&mutex);
    pthread_cond_signal(&requestAdded);
}

tlv_request_t getFirstQueueElement()
{
    pthread_mutex_lock(&mutex);

    tlv_request_t request = requestQueue[firstQueueElement++];

    if(firstQueueElement == MAX_REQUEST_QUEUE_LENGTH)
    {
        firstQueueElement = 0;
    }

    nCurrentWaitingRequests--;

    pthread_mutex_unlock(&mutex);

    return request;
}

void handleAccountCreation(tlv_request_t request)//TODO ID THREAD
{
    if(request.value.header.account_id == ADMIN_ACCOUNT_ID)
    {
        createAccount(request.value.create.account_id, request.value.create.balance, request.value.create.password);
        reply(request.value.header.pid, request.value.header.account_id, R_OK);
    }             
}

void handleGetBalance(tlv_request_t request)
{

}

void handleTransfer(tlv_request_t request)
{

}

void handleShutDown(tlv_request_t request)
{

}

void handleRequest(tlv_request_t request)
{
    switch (request.type)
    {
    case OP_CREATE_ACCOUNT:
        handleAccountCreation(request);
        break;
    
    case OP_BALANCE:
        handleGetBalance(request);
        break;

    case OP_TRANSFER:
        handleTransfer(request);
        break;

    case OP_SHUTDOWN:
        handleShutDown(request);
        break;
    
    default:
        break;
    }
}

void reply(pid_t pid, uint32_t accID, int answer)
{
    char * fifoName = USER_FIFO_PATH_PREFIX;
    char pidChar [6];
    sprintf(pidChar, "%d", pid);

    strcat(fifoName, pidChar);
    int fifoFD = initAndOpenFIFO(fifoName, O_WRONLY, O_CREAT);   //TODO?

    if(fifoFD == -1)
    {
        tlv_reply_t temp;
        temp.value.header.account_id = accID;
        temp.value.header.ret_code = RC_USR_DOWN;


        logReply(serverLogFD, pthread_self(), &temp);
    }

    tlv_reply_t temp;
    temp.value.header.account_id = accID;
    temp.value.header.ret_code = answer;

    logReply(fifoFD, accID, &temp);
}

//------------------------------------------------------------------------------------------------------------------------------------------------

// Account stuff ---------------------------------------------------------------------------------------------------------------------------------
bank_account_t contasBancarias[MAX_BANK_ACCOUNTS];//Estrutura que guarda as contas bancarias

void addBankAccount(bank_account_t acc)
{
    pthread_mutex_lock(&mutexAccounts);
    contasBancarias[acc.account_id] = acc;
    pthread_mutex_unlock(&mutexAccounts);
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
void createAccount(const uint32_t id, const uint32_t initialBalance, const char * password) //Assume que os valores estao dentro das normas
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
    strcpy(passHash, getSha256sumOf(password));

    strcpy(conta.hash, passHash);

    //Adding account
    addBankAccount(conta);
    logAccountCreation(serverLogFD, pthread_self(), &conta);

    //Debug only
    //printf("ID = <%d>\n", conta.account_id);
    //printf("Bal = <%d>\n", conta.balance);
    //printf("Hash = <%s>\n", conta.hash);
    //printf("Salt = <%s>\n", conta.salt);
}

bool authenticate(const uint32_t accID, const char password[])
{
    char * strConcat = (char *) password;
    strcat(strConcat, contasBancarias[accID].salt);
    
    return getSha256sumOf(strConcat) == contasBancarias[accID].hash;
}


//------------------------------------------------------------------------------------------------------------------------------------------------

//Bank Offices stuff------------------------------------------------------------------------------------------------------------------------------

//TODO: CHANGE FUNCTION (and args) OF THREADS
void * threadFunc(void * arg)
{
    while(1)
    {
        if(getQueueSize() == 0)
        {
            nThreadsWaitingForRequests++;
            pthread_cond_wait(&requestAdded, &mutex);
        }

        handleRequest(getFirstQueueElement());

    }

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

    for(size_t i = 1; i <= nBankOffices; i++)
    {
        pthread_t temp;
        pthread_create(&temp, NULL, threadFunc, NULL); //TODO: CHANGE FUNCTION (and args) OF THREADS
        logBankOfficeOpen(serverLogFD, i, temp);
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



//Utility functions-------------------------------------------------------------------------------------------------------------------------------

void initServer(char *argv[])
{
    serverLogFD = open(SERVER_LOGFILE, O_WRONLY | O_CREAT | O_TRUNC); //Cria o ficheiro de log do servidor

    char password[MAX_PASSWORD_LEN];
    memcpy(password, &argv[2][0], sizeof(argv[2]) - 2); //Retira aspas
    password[MAX_PASSWORD_LEN] = '\0';

    createAccount(ADMIN_ACCOUNT_ID, 0, password); //Cria conta do admin
}

//------------------------------------------------------------------------------------------------------------------------------------------------
int main (int argc, char *argv[], char *envp[])
{

    if(checkArgs(argc, argv)) //This func returns 1 if there's any problem with the arguments
        return 1;             //If that happens, the program closes.

    printf("Server running!\n");

    initServer(argv);

    size_t nBankOffices = atoi(argv[1]);
    pthread_t activeBankOfficesList[nBankOffices + 1];//AKA activeThreadsList; Thread id 1 => activeBankOfficesList[1];
    activeBankOfficesList[0] = pthread_self();//activeBankOfficesList[0] => main thread's tid

    initializeBankOffices(activeBankOfficesList, nBankOffices); //activeThreads created, running and tids loaded to activeBankOfficesList
    waitForAllThreads(activeBankOfficesList, nBankOffices);



    int fifoFD;
    fifoFD = initAndOpenFIFO(SERVER_FIFO_PATH, FIFO_READ_WRITE_ALL_PERM, O_RDONLY); //Allows the user to insert commands;
    
    while(true)
    {
        tlv_request_t temp;
        read(fifoFD, &temp, sizeof(temp)); //ERROR EBADF

        if(!authenticate(temp.value.header.account_id, temp.value.header.password))
        {
            reply(temp.value.header.pid, temp.value.header.account_id, RC_LOGIN_FAIL);
            //TODO CLOSE FIFO?
        }
        else
        {
            addRequestToQueue(temp);
        }
        
    }

    closeFD(fifoFD); //Should only be executed once the admin enters de command to end server.
    printf("Server's dead.\n");
    return 0;
}