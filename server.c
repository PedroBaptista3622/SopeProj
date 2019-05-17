#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "server_functions.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;  //Used for request queue
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER; //Used for activeBankOfficesListGlobal

pthread_mutex_t mutexWaitingThreads = PTHREAD_MUTEX_INITIALIZER; //Used for controlling the nThreadsWaitingForRequests writes

pthread_mutex_t mutexAccounts = PTHREAD_MUTEX_INITIALIZER; //Used for the accounts array
pthread_cond_t requestAdded = PTHREAD_COND_INITIALIZER;

int nThreadsWaitingForRequests = 0;
int serverLogFD;

bank_account_t contasBancarias[MAX_BANK_ACCOUNTS]; //Estrutura que guarda as contas bancarias
bool contaExistente[MAX_BANK_ACCOUNTS] = {false};

bool shuttingDown = false;

pthread_t activeBankOfficesListGlobal[MAX_BANK_OFFICES];

void createAccount(const uint32_t id, const uint32_t initialBalance, const char *password, uint32_t op_delay_ms);
void reply(pid_t pid, int answer, tlv_reply_t reply);

int getMyID(pthread_t myTid)
{
    for (size_t i = 0; i < MAX_BANK_OFFICES; i++)
    {
        if (activeBankOfficesListGlobal[i] == myTid)
            return i;
    }

    return 0;
}

//Request / reply related functions-----------------------------------------------------------------------------------------------------------------------
tlv_request_t requestQueue[MAX_REQUEST_QUEUE_LENGTH];
int nCurrentWaitingRequests = 0;
int firstQueueElement = 0;
int rear = -1;

tlv_request_t peekFirstQueueElement()
{
    return requestQueue[firstQueueElement];
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
    if (!isFull())
    {
        if (rear == MAX_REQUEST_QUEUE_LENGTH - 1)
        {
            rear = -1;
        }

        requestQueue[++rear] = request;
        nCurrentWaitingRequests++;
    }

    pthread_cond_signal(&requestAdded);
}

tlv_request_t getFirstQueueElement()
{
    tlv_request_t request = requestQueue[firstQueueElement++];

    if (firstQueueElement == MAX_REQUEST_QUEUE_LENGTH)
    {
        firstQueueElement = 0;
    }

    nCurrentWaitingRequests--;

    return request;
}

tlv_reply_t getReplyFromRequest(tlv_request_t request)
{
    tlv_reply_t temp;
    temp.value.header.account_id = request.value.header.account_id;
    temp.type = request.type;
    return temp;
}

void handleAccountCreation(tlv_request_t request)
{
    if (request.value.header.account_id == ADMIN_ACCOUNT_ID)
    {

        if (contaExistente[request.value.create.account_id] == true)
        {
            reply(request.value.header.pid, RC_ID_IN_USE, getReplyFromRequest(request));
        }
        else
        {
            createAccount(request.value.create.account_id, request.value.create.balance, request.value.create.password, request.value.header.op_delay_ms);
            reply(request.value.header.pid, RC_OK, getReplyFromRequest(request));
        }
    }
    else
    {
        reply(request.value.header.pid, RC_OP_NALLOW, getReplyFromRequest(request));
    }
}

void handleGetBalance(tlv_request_t request, int myID)
{
    if (request.value.header.account_id == ADMIN_ACCOUNT_ID)
        reply(request.value.header.pid, RC_OP_NALLOW, getReplyFromRequest(request));
    else
    {
        tlv_reply_t temp = getReplyFromRequest(request);

        pthread_mutex_lock(&mutexAccounts);
        usleep(request.value.header.op_delay_ms * 1000);
        logSyncDelay(serverLogFD, myID, request.value.header.account_id, request.value.header.op_delay_ms);
        
        temp.value.balance.balance = contasBancarias[request.value.header.account_id].balance;

        pthread_mutex_unlock(&mutexAccounts);

        reply(request.value.header.pid, RC_OK, temp);
    }
}

void handleTransfer(tlv_request_t request, int myID)
{
    // Testar Condicoes
    if (request.value.header.account_id == ADMIN_ACCOUNT_ID)
        reply(request.value.header.pid, RC_OP_NALLOW, getReplyFromRequest(request));
    else if (contaExistente[request.value.transfer.account_id] == false)
        reply(request.value.header.pid, RC_ID_NOT_FOUND, getReplyFromRequest(request));
    else if (request.value.header.account_id == request.value.transfer.account_id)
        reply(request.value.header.pid, RC_SAME_ID, getReplyFromRequest(request));
    else if (contasBancarias[request.value.header.account_id].balance - request.value.transfer.amount < MIN_BALANCE)
        reply(request.value.header.pid, RC_NO_FUNDS, getReplyFromRequest(request));
    else if (contasBancarias[request.value.transfer.account_id].balance + request.value.transfer.amount > MAX_BALANCE)
        reply(request.value.header.pid, RC_NO_FUNDS, getReplyFromRequest(request));
    else
    {
        pthread_mutex_lock(&mutexAccounts);
        usleep(request.value.header.op_delay_ms * 1000);
        logSyncDelay(serverLogFD, myID, request.value.header.account_id, request.value.header.op_delay_ms);
        
        // Fazer transferencia
        contasBancarias[request.value.header.account_id].balance -= request.value.transfer.amount;
        contasBancarias[request.value.transfer.account_id].balance += request.value.transfer.amount;
        
        pthread_mutex_unlock(&mutexAccounts);

        reply(request.value.header.pid, RC_OK, getReplyFromRequest(request));
    }
}

void handleRequest(tlv_request_t request, int myID)
{
    logRequest(serverLogFD, myID, &request);

    switch (request.type)
    {
    case OP_CREATE_ACCOUNT:
        handleAccountCreation(request);
        break;
    case OP_BALANCE:
        handleGetBalance(request, myID);
        break;
    case OP_TRANSFER:
        handleTransfer(request, myID);
        break;
    default:
        break;
    }
}

void reply(pid_t pid, int answer, tlv_reply_t reply)
{
    const char *fifoName = getUserFifoPath((int)pid);

    int fifoFD = initAndOpenFIFO(fifoName, O_WRONLY, O_CREAT);

    if (fifoFD == -1)
    {
        reply.value.header.ret_code = RC_USR_DOWN;
        reply.length = sizeof(reply.value.header);
    }
    else
    {
        reply.value.header.ret_code = answer;
        reply.length = sizeof(reply.value.header);

        switch (reply.type)
        {
        case OP_TRANSFER:
            reply.value.transfer.balance = contasBancarias[reply.value.header.account_id].balance;
            reply.length += reply.value.transfer.balance;
            break;

        default:
            break;
        }

        write(fifoFD, &reply, sizeof(reply));
        close(fifoFD);
    }

    pthread_mutex_lock(&mutex2);
    int myID = getMyID(pthread_self());
    pthread_mutex_unlock(&mutex2);

    logReply(serverLogFD, myID, &reply);
}

//------------------------------------------------------------------------------------------------------------------------------------------------

// Account stuff ---------------------------------------------------------------------------------------------------------------------------------
void addBankAccount(bank_account_t acc)
{
    contasBancarias[acc.account_id] = acc;
    contaExistente[acc.account_id] = true;
}

void createAccount(const uint32_t id, const uint32_t initialBalance, const char *password, uint32_t op_delay_ms) //Assume que os valores estao dentro das normas
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

    pthread_mutex_lock(&mutex2);
    int myID = getMyID(pthread_self());
    pthread_mutex_unlock(&mutex2);

    //Adding account
    pthread_mutex_lock(&mutexAccounts);
    usleep(op_delay_ms * 1000);
    logSyncDelay(serverLogFD, myID, id, op_delay_ms);

    addBankAccount(conta);

    pthread_mutex_unlock(&mutexAccounts);

    logAccountCreation(serverLogFD, myID, &conta);
}

bool authenticate(const uint32_t accID, const char *password)
{
    return (strcmp(echoSha256sum(password, contasBancarias[accID].salt), contasBancarias[accID].hash) == 0);
}

//------------------------------------------------------------------------------------------------------------------------------------------------

//Bank Offices stuff------------------------------------------------------------------------------------------------------------------------------

void *threadFunc(void *arg)
{
    pthread_mutex_lock(&mutex);
    int nElementsQueue = getQueueSize();
    pthread_mutex_unlock(&mutex);

    pthread_mutex_lock(&mutex2);
    int myID = getMyID(pthread_self());
    pthread_mutex_unlock(&mutex2);

    while (!shuttingDown || (nElementsQueue != 0))
    {
        pthread_mutex_lock(&mutex);

        if (getQueueSize() == 0)
        {
            pthread_mutex_lock(&mutexWaitingThreads);
            nThreadsWaitingForRequests++;
            pthread_mutex_unlock(&mutexWaitingThreads);

            pthread_cond_wait(&requestAdded, &mutex);

            pthread_mutex_lock(&mutexWaitingThreads);
            nThreadsWaitingForRequests--;
            pthread_mutex_unlock(&mutexWaitingThreads);
        }

        if (getQueueSize() > 0)
            handleRequest(getFirstQueueElement(), myID);

        nElementsQueue = getQueueSize();
        pthread_mutex_unlock(&mutex);
    }

    logBankOfficeClose(serverLogFD, myID, pthread_self());

    return NULL;
}

void initializeBankOffices(pthread_t listBankOffices[], const size_t nBankOffices)
{
    for (size_t i = 1; i <= nBankOffices; i++)
    {
        pthread_t temp;
        pthread_create(&temp, NULL, threadFunc, NULL);
        logBankOfficeOpen(serverLogFD, i, temp);
        listBankOffices[i] = temp;
    }
}

void waitForAllThreads(pthread_t listTids[], size_t nBankOffices)
{
    while (nThreadsWaitingForRequests != nBankOffices) //Waits for the threads to finish their work
    {
        sleep(1);
    }

    pthread_cond_broadcast(&requestAdded);

    for (size_t i = 1; i <= nBankOffices; i++)
    {
        printf("Waiting for %ld\n", listTids[i]);
        pthread_join(listTids[i], NULL);
    }
}

//Utility functions-------------------------------------------------------------------------------------------------------------------------------

void initServer(char *argv[])
{
    FILE *temp = fopen(SERVER_LOGFILE, "w");
    serverLogFD = fileno(temp); //Cria o ficheiro de log do servidor

    createAccount(ADMIN_ACCOUNT_ID, 0, argv[2], 0); //Cria conta do admin
}

//------------------------------------------------------------------------------------------------------------------------------------------------
int main(int argc, char *argv[], char *envp[])
{

    if (checkArgs(argc, argv)) //This func returns 1 if there's any problem with the arguments
        return 1;              //If that happens, the program closes.

    printf("[Server] Starting!\n");

    initServer(argv);

    size_t nBankOffices = atoi(argv[1]);
    pthread_t activeBankOfficesList[nBankOffices + 1]; //AKA activeThreadsList; Thread id 1 => activeBankOfficesList[1];
    activeBankOfficesList[0] = pthread_self();         //activeBankOfficesList[0] => main thread's tid

    initializeBankOffices(activeBankOfficesList, nBankOffices); //activeThreads created, running and tids loaded to activeBankOfficesList

    for (size_t i = 0; i <= nBankOffices; i++)
        activeBankOfficesListGlobal[i] = activeBankOfficesList[i];

    int fifoFD;
    fifoFD = initAndOpenFIFO(SERVER_FIFO_PATH, FIFO_READ_WRITE_ALL_PERM, O_RDONLY); //Allows the user to insert commands;

    while (!shuttingDown)
    {
        tlv_request_t temp;
        read(fifoFD, &temp, sizeof(temp));
        logRequest(serverLogFD, 0, &temp);

        if (!authenticate(temp.value.header.account_id, temp.value.header.password))
        {
            reply(temp.value.header.pid, RC_LOGIN_FAIL, getReplyFromRequest(temp));
        }
        else
        {
            if (temp.type == OP_SHUTDOWN) //Neste sitio, o user ja se encontra autenticado
            {
                if (temp.value.header.account_id == ADMIN_ACCOUNT_ID)
                {
                    shuttingDown = true;
                    usleep(temp.value.header.op_delay_ms * 1000);
                    logDelay(serverLogFD, 0, temp.value.header.op_delay_ms);
                    fchmod(fifoFD, S_IRGRP | S_IROTH | S_IRUSR);
                    printf("ShuttingDown enabled!\n");

                    tlv_reply_t tempRep = getReplyFromRequest(temp);
                    tempRep.value.shutdown.active_offices = nBankOffices - nThreadsWaitingForRequests;
                    reply(temp.value.header.pid, RC_OK, tempRep);
                }
                else
                {
                    printf("Try to shutdown without permitions\n");
                    tlv_reply_t tempRep = getReplyFromRequest(temp);
                    tempRep.value.shutdown.active_offices = 0;
                    reply(temp.value.header.pid, RC_OP_NALLOW, tempRep);
                }
            }
            else
            {
                pthread_mutex_lock(&mutex);
                addRequestToQueue(temp);
                pthread_mutex_unlock(&mutex);
            }
        }
    }

    waitForAllThreads(activeBankOfficesList, nBankOffices);
    closeFD(fifoFD);                      //Should only be executed once the admin enters de command to end server.
    removeFileFromPath(SERVER_FIFO_PATH); //Deletes fifo after execution
    printf("[Server] End.\n");
    return 0;
}