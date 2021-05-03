#include "redis_interface.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mul_common.h"

redisContext *context = NULL;

DB_RESULT redis_connect(void)
{
	if(context)redisFree(context);
	context = redisConnect(REDIS_SERVER_IP, REDIS_SERVER_PORT);
    if (context->err)
    {
        redisFree(context);
        printf("%d connect redis server failure:%s\n", __LINE__, context->errstr);
        return FAILURE;
    }

	return SUCCESS;
}

DB_RESULT redis_disconnect(void)
{
	if(context)redisFree(context);
	context = NULL;
	return SUCCESS;
}

DB_RESULT exeRedisIntCmd(char *cmd)
{
    redisReply *reply;

    /*检查入参*/
    if (NULL == cmd)
    {
        printf("NULL pointer");
        return FAILURE;
    }

    /*check redis connection*/
	if(!context)
	{
		printf("haven't connect to redis server");
		return FAILURE;
	}

    /*执行redis命令*/
    reply = (redisReply *)redisCommand(context, cmd);
    if (NULL == reply)
    {
        printf("%d execute command:%s failure\n", __LINE__, cmd);
        return FAILURE;
    }

    freeReplyObject(reply);
    //printf("%d execute command:%s success\n", __LINE__, cmd);
    return SUCCESS;
}

DB_RESULT redis_Get_Ctrl_Id(uint32_t ip, uint16_t *cid)
{
	char cmd[CMD_MAX_LENGHT] = {0};
	redisReply *reply;

	/*check redis connection*/
	if(!context)
	{
		printf("haven't connect to redis server");
		return FAILURE;
	}

    snprintf(cmd, CMD_MAX_LENGHT, "hget ctrl %u", ip);
    reply = (redisReply *)redisCommand(context, cmd);
    if (NULL == reply)
    {
        printf("%d execute command:%s failure\n", __LINE__, cmd);
        return FAILURE;
    }

	//printf("ctrl id:%s\n", reply->str);
	if(!reply->str)return FAILURE;
	*cid = atoi(reply->str);
    freeReplyObject(reply);

    return SUCCESS;
}

DB_RESULT redis_Set_Link_Delay(uint32_t sw1, uint32_t sw2, uint64_t delay)
{
	char cmd[CMD_MAX_LENGHT] = {0};
	uint64_t redis_key = (((uint64_t)sw1) << 32) + sw2;
	// c_log_debug("redis_key: %lx", redis_key);
	
	/*check redis connection*/
	if(!context)
	{
		printf("haven't connect to redis server");
		return FAILURE;
	}
	// set the link information
	snprintf(cmd, CMD_MAX_LENGHT, "hset link_delay %lu %lu", redis_key, delay);	
    return exeRedisIntCmd(cmd);
}

DB_RESULT redis_Get_Link_Delay(uint32_t sw1, uint32_t sw2, uint64_t *delay)
{
	char cmd[CMD_MAX_LENGHT] = {0};
	redisReply *reply;
	uint64_t redis_key = (((uint64_t)sw1) << 32) + sw2;
	// c_log_debug("redis_key: %lx", redis_key);

	/*check redis connection*/
	if(!context)
	{
		printf("haven't connect to redis server");
		return FAILURE;
	}

	snprintf(cmd, CMD_MAX_LENGHT, "hget link_delay %lu", redis_key);
	reply = (redisReply *)redisCommand(context, cmd);
    if (NULL == reply)
    {
        printf("%d execute command:%s failure\n", __LINE__, cmd);
        return FAILURE;
    }
	if(!reply->str)return FAILURE;
	*delay = atol(reply->str);
	freeReplyObject(reply);
    return SUCCESS;
}

DB_RESULT redis_Set_Link_Port(uint32_t sw1, uint32_t port1, uint32_t sw2, uint32_t port2)
{
	char cmd[CMD_MAX_LENGHT] = {0};
	uint64_t redis_key = (((uint64_t)sw1) << 32) + sw2;
	int ret = 0;
	// c_log_debug("redis_key: %lx", redis_key);
	
	/*check redis connection*/
	if(!context)
	{
		printf("haven't connect to redis server");
		return FAILURE;
	}
	// set the link information
    snprintf(cmd, CMD_MAX_LENGHT, "hset link_port_h %lu %u", redis_key, port1);
	ret &= exeRedisIntCmd(cmd);
	memset(cmd, 0, CMD_MAX_LENGHT);
	snprintf(cmd, CMD_MAX_LENGHT, "hset link_port_n %lu %u", redis_key, port2);
    return ret & exeRedisIntCmd(cmd);
}

DB_RESULT redis_Get_Link_Port(uint32_t sw1, uint32_t *port1, uint32_t sw2, uint32_t *port2)
{
	char cmd[CMD_MAX_LENGHT] = {0};
	redisReply *reply;
	uint64_t redis_key = (((uint64_t)sw1) << 32) + sw2;
	// c_log_debug("redis_key: %lx", redis_key);

	/*check redis connection*/
	if(!context)
	{
		printf("haven't connect to redis server");
		return FAILURE;
	}
	//get the information list
    snprintf(cmd, CMD_MAX_LENGHT, "hget link_port_h %lu", redis_key);
	reply = (redisReply *)redisCommand(context, cmd);
    if (NULL == reply)
    {
        printf("%d execute command:%s failure\n", __LINE__, cmd);
        return FAILURE;
    }
	//convert string to integer
	if(!reply->str)return FAILURE;
	*port1 = atoi(reply->str);
	memset(cmd, 0, CMD_MAX_LENGHT);
	freeReplyObject(reply);

	snprintf(cmd, CMD_MAX_LENGHT, "hget link_port_n %lu", redis_key);
	reply = (redisReply *)redisCommand(context, cmd);
    if (NULL == reply)
    {
        printf("%d execute command:%s failure\n", __LINE__, cmd);
        return FAILURE;
    }
	if(!reply->str)return FAILURE;
	*port2 = atoi(reply->str);
	memset(cmd, 0, CMD_MAX_LENGHT);
	freeReplyObject(reply);

    return SUCCESS;
}

DB_RESULT redis_Set_Pc_Sw_Port(uint32_t ip, uint32_t sw, uint32_t port)
{
	char cmd[CMD_MAX_LENGHT] = {0};
	int ret = 0;

    /*check redis connection*/
	if(!context)
	{
		printf("haven't connect to redis server");
		return FAILURE;
	}

	snprintf(cmd, CMD_MAX_LENGHT, "hset pc_sw %u %u", ip, sw);
	ret &= exeRedisIntCmd(cmd);
	memset(cmd, 0, CMD_MAX_LENGHT);
	snprintf(cmd, CMD_MAX_LENGHT, "hset pc_port %u %u", ip, port);
    return ret&exeRedisIntCmd(cmd);;
}

DB_RESULT redis_Get_Pc_Sw_Port(uint32_t ip, uint32_t *sw, uint32_t *port)
{
	char cmd[CMD_MAX_LENGHT] = {0};
	redisReply *reply;

	/*check redis connection*/
	if(!context)
	{
		printf("haven't connect to redis server");
		return FAILURE;
	}

	snprintf(cmd, CMD_MAX_LENGHT, "hget pc_sw %u", ip);
	reply = (redisReply *)redisCommand(context, cmd);
    if (NULL == reply)
    {
        printf("%d execute command:%s failure\n", __LINE__, cmd);
        return FAILURE;
    }
	if(!reply->str)return FAILURE;
	*sw = atoi(reply->str);
	memset(cmd, 0, CMD_MAX_LENGHT);
	freeReplyObject(reply);

	snprintf(cmd, CMD_MAX_LENGHT, "hget pc_port %u", ip);
	reply = (redisReply *)redisCommand(context, cmd);
    if (NULL == reply)
    {
        printf("%d execute command:%s failure\n", __LINE__, cmd);
        return FAILURE;
    }
	if(!reply->str)return FAILURE;
	*port = atoi(reply->str);
	freeReplyObject(reply);
	return SUCCESS;
}

DB_RESULT redis_Get_Pc_MAC(uint32_t ip, uint8_t *mac)
{
	char cmd[CMD_MAX_LENGHT] = {0};
	redisReply *reply;
	uint64_t maci = 0;
	int i;

	/*check redis connection*/
	if(!context)
	{
		printf("haven't connect to redis server");
		return FAILURE;
	}

	snprintf(cmd, CMD_MAX_LENGHT, "hget pc_mac %u", ip);
	reply = (redisReply *)redisCommand(context, cmd);
    if (NULL == reply)
    {
        printf("%d execute command:%s failure\n", __LINE__, cmd);
        return FAILURE;
    }
	if(!reply->str)return FAILURE;
	maci = atoi(reply->str);
	freeReplyObject(reply);

	for(i=5; i>=0; i--)
	{
		mac[i] = 0x00000000000000ff&maci;
		maci = (maci>>8) & 0x00ffffffffffffff;
	}

	return SUCCESS;
}

DB_RESULT redis_Set_Pc_MAC(uint32_t ip, uint8_t *mac)
{
	char cmd[CMD_MAX_LENGHT] = {0};
	uint64_t maci = 0;
	int i;

	for(i = 0; i<5; i++)
	{
		maci += mac[i];
		maci = (maci << 8) & 0xffffffffffffff00;
	}
	maci += mac[5];

	snprintf(cmd, CMD_MAX_LENGHT, "hset pc_mac %u %lu", ip, maci);

    return exeRedisIntCmd(cmd);
}

DB_RESULT redis_Set_Sw_Delay(uint32_t sw, uint64_t delay)
{
	char cmd[CMD_MAX_LENGHT] = {0};

	// set the PC information
    snprintf(cmd, CMD_MAX_LENGHT, "hset sw %u %lu", sw, delay);

    return exeRedisIntCmd(cmd);
}

DB_RESULT redis_Get_Sw_Delay(uint32_t sw, uint64_t *delay)
{
	char cmd[CMD_MAX_LENGHT] = {0};
	redisReply *reply;

	/*check redis connection*/
	if(!context)
	{
		printf("haven't connect to redis server");
		return FAILURE;
	}

    snprintf(cmd, CMD_MAX_LENGHT, "hget sw %u", sw);
    reply = (redisReply *)redisCommand(context, cmd);
    if (NULL == reply)
    {
        printf("%d execute command:%s failure\n", __LINE__, cmd);
        return FAILURE;
    }

	//printf("ctrl id:%s\n", reply->str);
	if(!reply->str)return FAILURE;
    *delay = atol(reply->str);
    freeReplyObject(reply);
	return SUCCESS;
}

DB_RESULT redis_Set_Route_Path(uint32_t nw_src, uint32_t nw_dst, uint32_t *path, uint8_t len)
{
	char cmd[CMD_MAX_LENGHT] = {0};
	redisReply *reply;
	uint64_t redis_key = (((uint64_t)nw_src) << sizeof(uint32_t)) + nw_dst;
	int i;

	/*check redis connection*/
	if(!context)
	{
		printf("haven't connect to redis server");
		return FAILURE;
	}

	if(redis_Is_Route_Path(nw_src, nw_dst))
	{
		snprintf(cmd, CMD_MAX_LENGHT, "del %lu", redis_key);
    	reply = (redisReply *)redisCommand(context, cmd);
		if (NULL == reply)
		{
			printf("%d execute command:%s failure\n", __LINE__, cmd);
			return FAILURE;
		}
		freeReplyObject(reply);
		memset(cmd, 0, CMD_MAX_LENGHT);
	}

	for(i = 0; i<len; i++)
	{
		snprintf(cmd, CMD_MAX_LENGHT, "rpush %lu %u", redis_key, path[i]);
		if(!exeRedisIntCmd(cmd))return FAILURE;
		memset(cmd, 0, CMD_MAX_LENGHT);
	}

	return SUCCESS;
}

DB_RESULT redis_Get_Route_Path(uint32_t nw_src, uint32_t nw_dst, uint32_t **path, uint32_t *len)
{
	char cmd[CMD_MAX_LENGHT] = {0};
	redisReply *reply;
	uint64_t redis_key = (((uint64_t)nw_src) << sizeof(uint32_t)) + nw_dst;
	int i;

	/*check redis connection*/
	if(!context)
	{
		printf("haven't connect to redis server");
		return FAILURE;
	}

	if(!redis_Is_Route_Path(nw_src, nw_dst))return FAILURE;
	
	snprintf(cmd, CMD_MAX_LENGHT, "llen %lu", redis_key);
	reply = (redisReply *)redisCommand(context, cmd);
	if (NULL == reply)
	{
		printf("%d execute command:%s failure\n", __LINE__, cmd);
		return FAILURE;
	}
	if(!reply->integer)return FAILURE;
	*len = reply->integer;
	freeReplyObject(reply);
	memset(cmd, 0, CMD_MAX_LENGHT);

	for(i = 0; i<*len; i++)
	{
		snprintf(cmd, CMD_MAX_LENGHT, "lindex %lu %d", redis_key, i);
		reply = (redisReply *)redisCommand(context, cmd);
		if (NULL == reply)
		{
			printf("%d execute command:%s failure\n", __LINE__, cmd);
			return FAILURE;
		}
		if(!reply->str)return FAILURE;
		*path[i] = atoi(reply->str);
		freeReplyObject(reply);
		memset(cmd, 0, CMD_MAX_LENGHT);
	}

	return SUCCESS;
}

DB_RESULT redis_Is_Route_Path(uint32_t nw_src, uint32_t nw_dst)
{
	char cmd[CMD_MAX_LENGHT] = {0};
	redisReply *reply;
	uint64_t redis_key = (((uint64_t)nw_src) << sizeof(uint32_t)) + nw_dst;

	/*check redis connection*/
	if(!context)
	{
		printf("haven't connect to redis server");
		return FAILURE;
	}

	snprintf(cmd, CMD_MAX_LENGHT, "exists %lu", redis_key);
    reply = (redisReply *)redisCommand(context, cmd);
    if (NULL == reply)
    {
        printf("%d execute command:%s failure\n", __LINE__, cmd);
        return FAILURE;
    }
	if(reply->integer == 1L)
	{
		freeReplyObject(reply);
		return SUCCESS;
	}
	freeReplyObject(reply);
	return FAILURE;
}

// int main(int argc,char *argv[])
// {
// 	uint32_t sw1 = 1, sw2 = 2;
// 	uint64_t sw1_delay = 1,  sw2_delay = 2;
// 	uint32_t port1 = 0x1, port2 = 0x2;
//     uint64_t link_delay = 3;
// 	uint32_t pc_ip = 4;
//     uint32_t pc_sw_port = 4;
// 	uint8_t mac1[6]={'\0'};
// 	uint8_t mac2[6]={'\0'};
// 	mac1[5] = 1;

// 	redis_connect();
// 	redis_Set_Pc_MAC(331, mac1);
// 	redis_Get_Pc_MAC(331, mac2);
// 	redis_disconnect();
// 	printf("Close\n");
//     return 0;
// }