#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "server_functions.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; //Used for request queue
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER; //Used for waking "sleeping" threads, when a request is added
pthread_mutex_t mutexAccounts = PTHREAD_MUTEX_INITIALIZER; //Used for the accounts array
pthread_cond_t requestAdded = PTHREAD_COND_INITIALIZER;
int nThreadsWaitingForRequests = 0;
int serverLogFD;

void createAccount(const uint32_t id, const uint32_t initialBalance, const char *password);
void reply(pid_t pid, int answer, tlv_reply_t reply);

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

    if (!isFull())
    {
        if (rear == MAX_REQUEST_QUEUE_LENGTH - 1)
        {
            rear = -1;
        }

        requestQueue[++rear] = request;
        nCurrentWaitingRequests++;
    }

    printf("Request added\n");

    pthread_mutex_unlock(&mutex);
    pthread_cond_signal(&requestAdded);
}

tlv_request_t getFirstQueueElement()
{
    printf("B4 lock\n");
    pthread_mutex_lock(&mutex);
    printf("MutexLocked!\n");

    tlv_request_t request = requestQueue[firstQueueElement++];

    if (firstQueueElement == MAX_REQUEST_QUEUE_LENGTH)
    {
        firstQueueElement = 0;
    }

    nCurrentWaitingRequests--;

    pthread_mutex_unlock(&mutex);

    return request;
}

tlv_reply_t getReplyFromRequest(tlv_request_t request)
{
    printf("Generating reply\n");
    tlv_reply_t temp;
    temp.value.header.account_id = request.value.header.account_id;
    temp.type = request.type;
    //temp.length = sizeof(temp.value.header);
    printf("End\n");
    return temp;
}

void handleAccountCreation(tlv_request_t request) //TODO ID THREAD
{
    printf("Acc creation handler called\n");

    if (request.value.header.account_id == ADMIN_ACCOUNT_ID)
    {
        createAccount(request.value.create.account_id, request.value.create.balance, request.value.create.password);
        reply(request.value.header.pid, RC_OK, getReplyFromRequest(request));
    }
    else
    {
        reply(request.value.header.pid, RC_OP_NALLOW, getReplyFromRequest(request));
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
        printf("Handle acc creation called\n");
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
        printf("Operation undefined!\n");
        break;
    }
}

void reply(pid_t pid, int answer, tlv_reply_t reply)
{
    const char *fifoName = getUserFifoPath((int)pid);

    printf("[DEBUG ONLY] fifoNameCreated\n");

    int fifoFD = initAndOpenFIFO(fifoName, O_WRONLY, O_CREAT); //TODO?

    printf("[DEBUG ONLY] fifoInit\n");

    if (fifoFD == -1)
    {
        reply.value.header.ret_code = RC_USR_DOWN;
        reply.length = sizeof(reply.value.header);
    }
    else
    {
        reply.value.header.ret_code = answer;
        reply.length = sizeof(reply.value.header);
        
        // TODO
        // Foi preciso adicionar + 8, se nao nao dava
        // É preciso dps dar uma olhadela nisto
        write(fifoFD, &reply, reply.length + 8);
        close(fifoFD);
    }

    logReply(serverLogFD, pthread_self(), &reply);
    printf("LogDONE\n");
}

//------------------------------------------------------------------------------------------------------------------------------------------------

// Account stuff ---------------------------------------------------------------------------------------------------------------------------------
bank_account_t contasBancarias[MAX_BANK_ACCOUNTS]; //Estrutura que guarda as contas bancarias

void addBankAccount(bank_account_t acc)
{
    pthread_mutex_lock(&mutexAccounts);
    contasBancarias[acc.account_id] = acc;
    pthread_mutex_unlock(&mutexAccounts);
}

//TODO: VERIFICAR DADOS CLIENTE?
void createAccount(const uint32_t id, const uint32_t initialBalance, const char *password) //Assume que os valores estao dentro das normas
{
    bank_account_t conta;
    conta.account_id = id;
    conta.balance = initialBalance;

    //Salty part *-*
    char saltyNumber[SALT_LEN + 1];
    strcpy(saltyNumber, getNewSaltNumber(SALT_LEN));

    strcpy(conta.salt, saltyNumber);

    //Password part
    strcpy(conta.hash, echoSha256sum(password, saltyNumber));

    //Adding account
    addBankAccount(conta);
    logAccountCreation(serverLogFD, pthread_self(), &conta);

    //Debug only
    //printf("ID = <%d>\n", conta.account_id);
    //printf("Bal = <%d>\n", conta.balance);
    //printf("Hash = <%s>\n", conta.hash);
    //printf("Salt = <%s>\n", conta.salt);
}

bool authenticate(const uint32_t accID, const char *password)
{
    return (strcmp(echoSha256sum(password, contasBancarias[accID].salt), contasBancarias[accID].hash) == 0);
}

//------------------------------------------------------------------------------------------------------------------------------------------------

//Bank Offices stuff------------------------------------------------------------------------------------------------------------------------------

//TODO: CHANGE FUNCTION (and args) OF THREADS
void *threadFunc(void *arg)
{
    while (1)
    {
        printf("[Thread n %ld]", pthread_self());

        if (getQueueSize() == 0)
        {
            printf("Queue Vazia!\n");
            nThreadsWaitingForRequests++;
            pthread_cond_wait(&requestAdded, &mutex2);
        }

        printf("Queue n esta mais vazia, handler chamado\n");
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
    printf("[Debug Only] Init banckOffices\n");

    for (size_t i = 1; i <= nBankOffices; i++)
    {
        pthread_t temp;
        pthread_create(&temp, NULL, threadFunc, NULL); //TODO: CHANGE FUNCTION (and args) OF THREADS
        logBankOfficeOpen(serverLogFD, i, temp);
        listBankOffices[i] = temp;
    }
}

void waitForAllThreads(pthread_t listTids[], const size_t nBankOffices)
{
    for (size_t i = 0; i < nBankOffices; i++)
    {
        pthread_join(listTids[i], NULL);
    }

    //printf("[DEBUG ONLY] All threads closed!\n");
}

//Utility functions-------------------------------------------------------------------------------------------------------------------------------

void initServer(char *argv[])
{
    FILE *temp = fopen(SERVER_LOGFILE, "w");
    serverLogFD = fileno(temp); //Cria o ficheiro de log do servidor

    createAccount(ADMIN_ACCOUNT_ID, 0, argv[2]); //Cria conta do admin
}

//------------------------------------------------------------------------------------------------------------------------------------------------
int main(int argc, char *argv[], char *envp[])
{

    if (checkArgs(argc, argv)) //This func returns 1 if there's any problem with the arguments
        return 1;              //If that happens, the program closes.

    printf("Server running!\n");

    initServer(argv);

    size_t nBankOffices = atoi(argv[1]);
    pthread_t activeBankOfficesList[nBankOffices + 1]; //AKA activeThreadsList; Thread id 1 => activeBankOfficesList[1];
    activeBankOfficesList[0] = pthread_self();         //activeBankOfficesList[0] => main thread's tid

    initializeBankOffices(activeBankOfficesList, nBankOffices); //activeThreads created, running and tids loaded to activeBankOfficesList

    //printf("[Debug Only] Opening Fifo\n");

    int fifoFD;
    fifoFD = initAndOpenFIFO(SERVER_FIFO_PATH, FIFO_READ_WRITE_ALL_PERM, O_RDONLY); //Allows the user to insert commands;

    //printf("[Debug Only] Fifo Opened\n");

    while (true)
    {
        //printf("[Debug Only] Reading requests\n");

        tlv_request_t temp;
        read(fifoFD, &temp, sizeof(temp)); //ERROR EBADF

        //printf("[Debug Only] Request read\n");

        if (!authenticate(temp.value.header.account_id, temp.value.header.password))
        {
            printf("[Debug Only] Aut failed\n");
            reply(temp.value.header.pid, RC_LOGIN_FAIL, getReplyFromRequest(temp));
        }
        else
        {
            printf("[Debug Only] Adding request\n");
            addRequestToQueue(temp);
        }
    }

    waitForAllThreads(activeBankOfficesList, nBankOffices);
    closeFD(fifoFD); //Should only be executed once the admin enters de command to end server.
    printf("Server's dead.\n");
    return 0;
}