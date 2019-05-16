#include "user_functions.h"

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
        if (atoi(arr[0]) >= MAX_BANK_ACCOUNTS || atoi(arr[0]) < ADMIN_ACCOUNT_ID + 1)
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
        int n; //number of words
        char arr[10][20];

        n = getWords(argv[5], arr);

        if (n + 1 != 2)
        {
            printf("Must receive 2 arguments\n");
            return false;
        }

        // Get Destination account_ID
        if (atoi(arr[0]) >= MAX_BANK_ACCOUNTS || atoi(arr[0]) < ADMIN_ACCOUNT_ID + 1)
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

tlv_reply_t createReply(char *argv[], ret_code_t ret_code)
{
    tlv_reply_t reply;

    reply.type = (op_type_t)atoi(argv[4]);
    reply.value.header.account_id = atoi(argv[1]);
    reply.value.header.ret_code = ret_code;
    reply.length = sizeof(reply.value.header);

    return reply;
}

tlv_request_t createRequest(char *argv[])
{
    tlv_request_t request;

    // Create request header
    req_header_t request_header;
    request_header.pid = getpid();
    request_header.account_id = atoi(argv[1]);
    strcpy(request_header.password, argv[2]);
    request_header.op_delay_ms = atoi(argv[3]);

    request.type = (op_type_t)atoi(argv[4]);
    request.value.header = request_header;
    printf("0.1 Size of value tranfer: %d\n", request.length);
    request.length = sizeof(request.value.header);
    printf("0.2 Size of value tranfer: %d\n", request.length);
    request.length += sizeof(request.type);
    printf("0.3 Size of value tranfer: %d\n", request.length);

    switch (request.type)
    {
    case OP_CREATE_ACCOUNT:
    {
        char arr[5][20];
        getWords(argv[5], arr);

        request.value.create.account_id = atoi(arr[0]);
        request.value.create.balance = atoi(arr[1]);
        strcpy(request.value.create.password, arr[2]);

        request.length += sizeof(request.value.create);

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

        printf("1 Size of value tranfer: %d\n", request.length);
        request.value.transfer.account_id = atoi(arr[0]);
        request.value.transfer.amount = atoi(arr[1]);

        request.length += sizeof(request.value.transfer);
        printf("2 Size of value tranfer: %d\n", request.length);

        break;
    }
    case OP_SHUTDOWN:
    {
        break;
    }
    default:
        break;
    }

    return request;
}