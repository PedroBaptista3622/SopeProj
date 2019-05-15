#pragma once

#include "sope.h"
#include "aux_functions.h"

bool checkOperation(int argc, char *argv[]);

bool checkInputs(int argc, char *argv[]);

tlv_reply_t createReply (char *argv[], ret_code_t ret_code);

tlv_request_t createRequest(char *argv[]);