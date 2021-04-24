#ifndef __MUL_REDIS_INTERFACE_H__
#define __MUL_REDIS_INTERFACE_H__

#include "hiredis.h"

#define buf_len 1024

int unpack_redis_reply(redisReply * reply);

#endif
