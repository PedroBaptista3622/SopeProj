#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include "sope.h"

// Funcao auxiliar pra separar words de uma string
// adaptada de https://www.includehelp.com/code-snippets/c-program-to-split-string-into-the-words-separated-by-spaces.aspx
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

bool checkOperation(int argc, char *argv[])
{
    // Check balance e End account
    if (atoi(argv[4]) == 1 || atoi(argv[4]) == 3)
    {
        if (strlen(argv[5]) != 0)
        {
            printf("Operations 1 & 3 don't receive arguments\n");
            return false;
        }

        if (atoi(argv[4]) == 3)
            if (atoi(argv[1]) != 0)
            {
                printf("Only root can end accounts\n");
                return false;
            }

        if (atoi(argv[4]) == 1)
            if (atoi(argv[1]) == 0)
            {
                printf("Root cant check balance\n");
                return false;
            }
    }
    // Criar conta
    else if (atoi(argv[4]) == 0)
    {
        if (atoi(argv[1]) != 0)
        {
            printf("Only root can create accounts\n");
            return false;
        }

        int n; //number of words
        char arr[10][20];

        n = getWords(argv[5], arr);

        if (n + 1 != 3)
        {
            printf("Must receive 3 arguments\n");
            return false;
        }

        // Get new_account_ID
        if (atoi(arr[0]) >= MAX_BANK_ACCOUNTS || atoi(arr[0]) < 1)
        {
            printf("Account ID must be between 1 and 4095\n");
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
    // Transferencia
    else
    {
        if (atoi(argv[1]) == 0)
        {
            printf("Root cant make transfers\n");
            return false;
        }

        int n; //number of words
        char arr[10][20];

        n = getWords(argv[5], arr);

        if (n + 1 != 2)
        {
            printf("Must receive 2 arguments\n");
            return false;
        }

        // Get Destination account_ID
        if (atoi(arr[0]) >= MAX_BANK_ACCOUNTS || atoi(arr[0]) < 1)
        {
            printf("Account ID must be between 1 and 4095\n");
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

bool checkInputs(int argc, char *argv[])
{
    if (argc != 6)
    {
        printf("Usage: user <accountID> <password> <op_delay_ms> <op_type> <arguments_list>\n");
        return false;
    }

    if (atoi(argv[1]) >= MAX_BANK_ACCOUNTS || atoi(argv[1]) < ADMIN_ACCOUNT_ID)
    {
        printf("Account ID must be between 0 and 4095\n");
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

    return checkOperation(argc, argv);
}

int main(int argc, char *argv[], char *envp[])
{
    // Check if inputs are valid
    if (!checkInputs(argc, argv))
        return 1;

    // Open user logFile
    // TODO DAR APPEND DOS REGISTOS??
    FILE *logFile = fopen(USER_LOGFILE, "w");
    int logFile_fd = fileno(logFile);

    // Initialize tlv request and reply
    tlv_request_t tlv_request;

    tlv_reply_t tlv_reply;

    // Create tlv request header
    req_header_t req_header;
    req_header.pid = getpid();
    req_header.account_id = atoi(argv[1]);
    strcpy(req_header.password, argv[2]);
    req_header.op_delay_ms = atoi(argv[3]);

    tlv_request.type = (op_type_t)atoi(argv[4]);
    tlv_request.value.header = req_header;
    tlv_request.length = sizeof(tlv_request.value.header);

    switch (tlv_request.type)
    {
    case OP_CREATE_ACCOUNT:
    {
        char arr[5][20];
        getWords(argv[5], arr);

        tlv_request.value.create.account_id = atoi(arr[0]);
        tlv_request.value.create.balance = atoi(arr[1]);
        strcpy(tlv_request.value.create.password, arr[2]);

        tlv_request.length += sizeof(tlv_request.value.create);

        break;
    }
    case OP_BALANCE:
    {
        break;
    }
    case OP_TRANSFER:
    {
        char arr[5][20];
        getWords(argv[5], arr);

        tlv_request.value.transfer.account_id = atoi(arr[0]);
        tlv_request.value.transfer.amount = atoi(arr[1]);

        tlv_request.length += sizeof(tlv_request.value.transfer);

        break;
    }
    case OP_SHUTDOWN:
    {
        break;
    }
    default:
        break;
    }

    // Create user FIFO
    char USER_FIFO_PATH[USER_FIFO_PATH_LEN];
    sprintf(USER_FIFO_PATH, "%s%d", USER_FIFO_PATH_PREFIX, (int)getpid());
    //mkfifo(USER_FIFO_PATH, 0750);

    // Open server FIFO
    int serverFIFO_fd;
    if ((serverFIFO_fd = open(SERVER_FIFO_PATH, O_WRONLY)) == -1)
    {
        // Create the tlv reply
        tlv_reply.type = (op_type_t)atoi(argv[4]);
        tlv_reply.value.header.account_id = atoi(argv[1]);
        tlv_reply.value.header.ret_code = RC_SRV_DOWN;
        tlv_reply.length = sizeof(tlv_reply.value.header);
        logReply(logFile_fd, getpid(), &tlv_reply);
        return 1;
    }

    // Send info to server
    write(serverFIFO_fd, &tlv_request, tlv_request.length);
    logRequest(logFile_fd, getpid(), &tlv_request);

    // Open user FIFO
    int userFIFO_fd;
    if ((userFIFO_fd = open(USER_FIFO_PATH, O_RDONLY)) == -1)
    {
        // Create the tlv reply
        tlv_reply.type = (op_type_t)atoi(argv[4]);
        tlv_reply.value.header.account_id = atoi(argv[1]);
        tlv_reply.value.header.ret_code = RC_USR_DOWN;
        tlv_reply.length = sizeof(tlv_reply.value.header);
        logReply(logFile_fd, getpid(), &tlv_reply);
        return 1;
    }

    // Get reply from server
    clock_t t_start, t_end;
    t_start = clock();
    double time_taken;
    bool reply_received = false;

    do
    {
        if (read(userFIFO_fd, &tlv_reply, tlv_reply.length))
        {
            reply_received = true;
            break;
        }
        t_end = clock() - t_start;
        time_taken = ((double)t_end) / CLOCKS_PER_SEC;
        sleep(1);
    } while (time_taken < FIFO_TIMEOUT_SECS);

    // Close FIFOs
    close(serverFIFO_fd);
    close(userFIFO_fd);

    // If reply wasnt received in 30 secs
    if (!reply_received)
    {
        // Create the tlv reply
        tlv_reply.type = (op_type_t)atoi(argv[4]);
        tlv_reply.value.header.account_id = atoi(argv[1]);
        tlv_reply.value.header.ret_code = RC_SRV_TIMEOUT;
        tlv_reply.length = sizeof(tlv_reply.value.header);
        logReply(logFile_fd, getpid(), &tlv_reply);
        return 1;
    }

    // Everything worked smoothly, write to log
    logReply(logFile_fd, getpid(), &tlv_reply);

    return 0;
}