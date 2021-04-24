#include "redis_interface.h"
#include "config.h"

int unpack_redis_reply(redisReply * reply)
{
    int ret = 1;
	
	switch(reply->type)
		{
			case REDIS_REPLY_STATUS://5 
				c_log_debug("redis status: %s\n", reply->str);
				break;
			case REDIS_REPLY_ERROR://6
				c_log_debug("redis error: %s\n", reply->str);
				ret = 0;
				break;
			case REDIS_REPLY_INTEGER: //3 integer(long long)
				c_log_debug("redis return integer: %lld\n", reply->integer);
				break;
			case REDIS_REPLY_NIL://4
				c_log_debug("redis no data return!\n");
				break;
			case REDIS_REPLY_STRING://1
				c_log_debug("redis return string: %s\n", reply->str);
				break;
			case REDIS_REPLY_ARRAY://11
				c_log_debug("redis return array!\n");
				for(int i = 0; i < reply->elements; i++)
				{
					unpack_redis_reply(reply->element[i]);
				}
				break;
			default:
				c_log_debug("reply type error, %d!\n", reply->type);
				break;
		}
	return ret;
}