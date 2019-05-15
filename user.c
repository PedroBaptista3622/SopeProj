#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

#include "user_functions.h"

int main(int argc, char *argv[])
{
    // Check if inputs are valid
    if (!checkInputs(argc, argv))
        return 1;

    // Open user logFile
    // TODO DAR APPEND DOS REGISTOS??
    FILE *logFile = fopen(USER_LOGFILE, "w");
    int logFile_fd = fileno(logFile);

    // Initialize tlv request and reply
    tlv_request_t request;
    tlv_reply_t reply;

    // Open server FIFO
    int serverFIFO_fd;
    if ((serverFIFO_fd = open(SERVER_FIFO_PATH, O_WRONLY)) == -1)
    {
        reply = createReply(argv, RC_SRV_DOWN);
        logReply(logFile_fd, getpid(), &reply);
        return 1;
    }

    // Send rqeuest to server
    request = createRequest(argv);
    write(serverFIFO_fd, &request, request.length);
    logRequest(logFile_fd, getpid(), &request);

    // User FIFO path
    const char *USER_FIFO_PATH = getUserFifoPath((int)getpid());

    // Time variables
    time_t start, end;
    time(&start);
    double elapsed_secs;

    int userFIFO_fd;
    bool fifoOpened = false;
    bool reply_received = false;


    // Get reply from server
    while (elapsed_secs < FIFO_TIMEOUT_SECS)
    {
        if (fifoOpened || ((userFIFO_fd = open(USER_FIFO_PATH, O_RDONLY)) != -1))
        {
            fifoOpened = true;

            if (read(userFIFO_fd, &reply, sizeof(reply)) > 0)
            {
                reply_received = true;
                break;
            }

            time(&end);
            elapsed_secs = difftime(end, start);
        }

        time(&end);
        elapsed_secs = difftime(end, start);
        sleep(1);
    }

    // If reply wasnt received in 30 secs
    if (!reply_received)
    {
        reply = createReply(argv, RC_SRV_TIMEOUT);
        logReply(logFile_fd, getpid(), &reply);
        return 1;
    }

    // Everything worked smoothly, write to log
    logReply(logFile_fd, getpid(), &reply);

    printf("No problems!\n");

    return 0;
}