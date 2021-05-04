#include "tp_route.h"
#include "heap.h"
#include "ARP.h"
#include "redis_interface.h"

extern tp_sw * tp_graph;
extern tp_swdpid_glabolkey * key_table;
extern uint32_t controller_area;
extern redisContext *context;

int rt_load_glabol_topo(void)
{
    redisReply * reply, **reply_tmp;
	char cmd[CMD_MAX_LENGHT] = {0};
    uint32_t cursor, sw_key_tmp1, sw_key_tmp2, sw_port1, sw_port2;
    uint64_t redis_key_tmp, delay_tmp;
    tp_sw * sw_tmp;
    int i;

    if(!context)return 0;
    c_log_debug("loading sw send command!");
    snprintf(cmd, CMD_MAX_LENGHT, "hscan sw 0 match * count 100");
	reply = (redisReply *)redisCommand(context, cmd);
	if (NULL == reply)
	{
		printf("%d execute command:%s failure\n", __LINE__, cmd);
		return FAILURE;
	}
	c_log_debug("loading sw send command reply");
	if(reply->elements != 0)
	{
		cursor = atoi(reply->element[0]->str);
		while(cursor != 0)
		{
            c_log_debug("need to get the less sw infor");
			reply_tmp = reply->element[1]->element;	
			for(i = 0; i<reply->element[1]->elements; i++)
			{
				sw_key_tmp1 = atoi(reply_tmp[i++]->str);
                sw_tmp = tp_add_sw(sw_key_tmp1);
				sw_tmp->delay = atol(reply_tmp[i]->str);
			}
			freeReplyObject(reply);
			memset(cmd, 0, CMD_MAX_LENGHT);
			snprintf(cmd, CMD_MAX_LENGHT, "hscan sw %u match * count 100", cursor);
			reply = (redisReply *)redisCommand(context, cmd);
			if (NULL == reply)
			{
				printf("%d execute command:%s failure\n", __LINE__, cmd);
				return FAILURE;
			}
			cursor = atoi(reply->element[0]->str);
		}
        c_log_debug("sw add to topo");
		reply_tmp = reply->element[1]->element;	
		for(i = 0; i<reply->element[1]->elements; i++)
		{
			sw_key_tmp1 = atoi(reply_tmp[i++]->str);
            if(!tp_find_sw(sw_key_tmp1))
            {
                sw_tmp = tp_add_sw(sw_key_tmp1);
                sw_tmp->delay = atol(reply_tmp[i]->str);
            }
		}
	}
    freeReplyObject(reply);
    memset(cmd, 0, CMD_MAX_LENGHT);
    c_log_debug("loaded sw");
    c_log_debug("loading link");
    snprintf(cmd, CMD_MAX_LENGHT, "hscan link_delay 0 match * count 100");
	reply = (redisReply *)redisCommand(context, cmd);
	if (NULL == reply)
	{
		printf("%d execute command:%s failure\n", __LINE__, cmd);
		return FAILURE;
	}
	
	if(reply->elements != 0)
	{
		cursor = atoi(reply->element[0]->str);
		while(cursor != 0)
		{
			reply_tmp = reply->element[1]->element;	
			for(i = 0; i<reply->element[1]->elements; i++)
			{
				redis_key_tmp = atol(reply_tmp[i++]->str);
                sw_key_tmp1 = (uint32_t)(((0xffffffff00000000&redis_key_tmp)>>32)&0x00000000ffffffff);
                sw_key_tmp2 = (uint32_t)(0x00000000ffffffff&redis_key_tmp);
                c_log_debug("redis_key_tmp %lx add a link between %x and %x", redis_key_tmp, sw_key_tmp1, sw_key_tmp2);
				delay_tmp = atol(reply_tmp[i]->str);
                redis_Get_Link_Port(sw_key_tmp1, &sw_port1, sw_key_tmp2, &sw_port2);
                tp_add_link(sw_key_tmp1, sw_port1, sw_key_tmp2, sw_port2);
                TP_SET_LINK(sw_key_tmp1, sw_key_tmp2, delay, delay_tmp);
			}
			freeReplyObject(reply);
			memset(cmd, 0, CMD_MAX_LENGHT);
			snprintf(cmd, CMD_MAX_LENGHT, "hscan link_delay %u match * count 100", cursor);
			reply = (redisReply *)redisCommand(context, cmd);
			if (NULL == reply)
			{
				printf("%d execute command:%s failure\n", __LINE__, cmd);
				return FAILURE;
			}
			cursor = atoi(reply->element[0]->str);
		}
		reply_tmp = reply->element[1]->element;	
		for(i = 0; i<reply->element[1]->elements; i++)
		{
			redis_key_tmp = atol(reply_tmp[i++]->str);
            sw_key_tmp1 = (uint32_t)(((0xffffffff00000000&redis_key_tmp)>>32)&0x00000000ffffffff);
            sw_key_tmp2 = (uint32_t)(0x00000000ffffffff&redis_key_tmp);
            c_log_debug("redis_key_tmp %lx add a link between %x and %x", redis_key_tmp, sw_key_tmp1, sw_key_tmp2);
            delay_tmp = atol(reply_tmp[i]->str);
            redis_Get_Link_Port(sw_key_tmp1, &sw_port1, sw_key_tmp2, &sw_port2);
            tp_add_link(sw_key_tmp1, sw_port1, sw_key_tmp2, sw_port2);
            TP_SET_LINK(sw_key_tmp1, sw_key_tmp2, delay, delay_tmp);
		}
	}
    freeReplyObject(reply);
    c_log_debug("loaded link");

    return SUCCESS;
}

rt_node * rt_find_node(uint32_t key, rt_node ** rt_set)
{
    rt_node *s = NULL;
    HASH_FIND(hh, *rt_set, &key, sizeof(uint32_t), s);
    return s;
}

rt_node * rt_add_node(uint32_t key, uint32_t prev_key, rt_node ** rt_set)
{
    rt_node *s = NULL;

    // c_log_debug("rt_find_node");
    if(rt_find_node(key, rt_set))return NULL;

    // c_log_debug("start add node to rt_set");
    s = malloc(sizeof(rt_node));
    memset(s, 0, sizeof(rt_node));
    s->key = key;
    s->prev_key = prev_key;
    HASH_ADD(hh, *rt_set, key, sizeof(uint32_t), s);

    return s;
}

int rt_del_node(uint32_t key, rt_node ** rt_set)
{
    rt_node *s = NULL;

    s = rt_find_node(key, rt_set);
    if(!s)return 0;
    HASH_DEL(*rt_set, s);
    free(s);

    return 1;
}

void rt_distory(rt_node ** rt_set)
{
    rt_node * s, * tmp;

    HASH_ITER(hh, *rt_set, s, tmp) 
    {
        HASH_DEL(*rt_set, s);
        free(s);
    }
}


rt_node* rt_ip_get_path(uint32_t sw_start, uint32_t sw_end)
{
    tp_sw * src_node, * dst_node, * start_node;
    tp_link * adj_node;
    heap rt_minheap;
    rt_node * rt_visited_set = NULL, *path = NULL, *path_tmp;
    uint64_t * delay_get;
    uint32_t * sw_visiting_key;
    uint64_t delay_set = 0;

    c_log_debug("loading topo from redis");
    rt_load_glabol_topo();
    c_log_debug("loaded topo from redis");
    src_node = tp_find_sw(sw_start);
    dst_node = tp_find_sw(sw_end);
    start_node = src_node;
    c_log_debug("src_node %x, dst_node %x!", src_node->key, dst_node->key);
    if(!src_node || !dst_node) return 0;

    c_log_debug("heap_create!");
    heap_create(&rt_minheap, 0, NULL);
    c_log_debug("rt_set add src_node %x!", src_node->key);
    rt_add_node(src_node->key, 0, &rt_visited_set);
    c_log_debug("heap insert src_node %x!", src_node->key);
    heap_insert(&rt_minheap, (void*)&(src_node->key), (void*)&delay_set);

    while(heap_size(&rt_minheap))
    {
        heap_delmin(&rt_minheap, (void*)&sw_visiting_key, (void*)&delay_get);
        c_log_debug("get the min_delay node %x %lu from heap", *sw_visiting_key, *delay_get);
        start_node = tp_find_sw(*sw_visiting_key);
        adj_node = start_node->list_link;//找到一个点开始遍历
        c_log_debug("get the sw links_head");
        if(adj_node == NULL) continue;
        c_log_debug("start traverse the sw links");
        while(adj_node)//遍历邻接点
        {
            if(adj_node->key == dst_node->key)
            {
                //找到了，下发流表
                c_log_debug("find the dst and than issue the flow table");
                c_log_debug("add the adj sw %x to rt_visited_set", adj_node->key);
                rt_add_node(adj_node->key, start_node->key, &rt_visited_set);
                c_log_debug("issue the flow table");
                //rt_set_ip_flow_path(nw_src, nw_dst, &rt_visited_set);
                while(sw_start != sw_end)
                {
                    path_tmp = rt_find_node(sw_end, &rt_visited_set);
                    rt_add_node(sw_end, path_tmp->prev_key, &path);
                    sw_end = path_tmp->prev_key;
                }
                rt_add_node(sw_start, 0, &path);
                c_log_debug("free the malloc and return 1");
                rt_distory(&rt_visited_set);
                heap_destroy(&rt_minheap);
                return path;
            }
            if(!rt_find_node(adj_node->key, &rt_visited_set))//没有被遍历过
            {
                c_log_debug("calculate the sum of delay");
                delay_set = *delay_get + adj_node->delay;
                c_log_debug("add the adj sw %x to heap", adj_node->key);
                heap_insert(&rt_minheap, (void*)&adj_node->key, (void*)&delay_set);//添加要遍历的点
                c_log_debug("add the adj sw %x to rt_visited_set", adj_node->key);
                rt_add_node(adj_node->key, start_node->key, &rt_visited_set);//添加已经遍历的点
            }
            adj_node = adj_node->next;
        }
    }

    c_log_debug("free the malloc and return 0");
    rt_distory(&rt_visited_set);
    heap_destroy(&rt_minheap);

    return path;
}

int rt_ip(uint32_t nw_src, uint32_t nw_dst, uint32_t sw_key)
{
    rt_node * rt_path = NULL, * rt_tmp;
    uint32_t * path = NULL, path_len = 0;
    uint32_t sw1_key = 0, sw1_port = 0, sw2_key = 0, sw2_port = 0;
    int i;
    struct in_addr addr1;

    redis_Get_Pc_Sw_Port(nw_src, &sw1_key, &sw1_port);
    addr1.s_addr = nw_src;
    c_log_debug("get_src %s sw %x and port %x", inet_ntoa(addr1), sw1_key, sw1_port);
    redis_Get_Pc_Sw_Port(nw_dst, &sw2_key, &sw2_port);
    addr1.s_addr = nw_dst;
    c_log_debug("get_dst %s sw %x and port %x", inet_ntoa(addr1), sw2_key, sw2_port);


    if(!sw1_key || !sw1_port || !sw2_key || !sw2_port)return 0;

    c_log_debug("if have a route before");
    if(redis_Is_Route_Path(nw_src, nw_dst))
    {
        c_log_debug("have a route before!");
        if(rt_set_ip_flow_path_from_redis(nw_src, nw_dst))return 1;
        c_log_debug("fail to set flow from redis, need to re-cul");
        rt_path = rt_ip_get_path(sw_key, sw2_key);
    }else
    {
        c_log_debug("dont have a path, first cul");
        rt_path = rt_ip_get_path(sw1_key, sw2_key);
    }
    if(rt_path)
    {
        c_log_debug("have a route, seting");
        path_len = HASH_COUNT(rt_path);
        c_log_debug("path len %u path:", path_len);
        path = malloc(sizeof(uint32_t)*path_len);
        for(i=path_len-1; i>=0; i--)
        {
            HASH_FIND(hh, rt_path, &sw2_key, sizeof(uint32_t), rt_tmp);
            c_log_debug("rt_tmp->key:%x", rt_tmp->key);
            path[i] = rt_tmp->key;
            sw2_key = rt_tmp->prev_key;
        }
        c_log_debug("path write to redis");
        redis_Set_Route_Path(nw_src, nw_dst, path, path_len);
        c_log_debug("send to the sw");
        for(i = 0; i < path_len; i++)
        {
            if((path[i]&0xffff0000) == controller_area)
            {
                c_log_debug("seting sw %x, i=%d", path[i], i);
                if(i == path_len -1)
                {
                    redis_Get_Pc_Sw_Port(nw_dst, &sw1_key, &sw1_port);
                    c_log_debug("end issuing sw %x outport: %u", path[i], sw1_port);
                    rt_ip_issue_flow(tp_find_sw(sw1_key)->sw_dpid, nw_src, nw_dst, sw1_port);
                    free(path);
                    return 1;
                }
                redis_Get_Link_Port(path[i], &sw1_port, path[i+1], &sw2_port);
                c_log_debug("issuing sw %x outport: %u", path[i], sw1_port);
                rt_ip_issue_flow(tp_find_sw(path[i])->sw_dpid, nw_src, nw_dst, sw1_port);
                if((path[i + 1]&0xffff0000) != controller_area)
                {
                    c_log_debug("end issuing sw %x ", path[i]);
                    free(path);
                    return 1;
                }
            }
        }
        free(path);
        return 1;
    }
    
    return 0;
}

int rt_set_ip_flow_path_from_redis(uint32_t src_ip, uint32_t dst_ip)
{
    uint32_t * path, path_len, outport1, outport2, tmp;
    int i;
    c_log_debug("get path from redis");
    redis_Get_Route_Path(src_ip, dst_ip, &path, &path_len);
    c_log_debug("look for the sw that I controll");
    for (i = 0; i < path_len; i++)
    {
        if((path[i]&0x11110000) == controller_area)
        {
            if(i == path_len -1)
            {
                redis_Get_Pc_Sw_Port(dst_ip, &tmp, &outport1);
                rt_ip_issue_flow(tp_find_sw(tmp)->sw_dpid, src_ip, dst_ip, outport1);
                free(path);
                return 1;
            }
            redis_Get_Link_Port(path[i], &outport1, path[i+1], &outport2);
            rt_ip_issue_flow(tp_find_sw(path[i])->sw_dpid, src_ip, dst_ip, outport1);
            if((path[i + 1]&0x11110000) != controller_area){free(path);return 1;}
        }
    }
    free(path);
    return 0;
}

int rt_ip_issue_flow(uint64_t sw_dpid, uint32_t src_ip, uint32_t dst_ip, uint32_t outport)
{
    struct flow fl;
    struct flow mask;
    mul_act_mdata_t mdata;

    memset(&fl, 0, sizeof(fl));
    memset(&mdata, 0, sizeof(mdata));
    of_mask_set_dc_all(&mask);

    fl.ip.nw_dst = dst_ip;
    fl.ip.nw_src = src_ip;
    fl.dl_type = htons(ETH_TYPE_IP);
    of_mask_set_dl_type(&mask);
    of_mask_set_nw_dst(&mask, 32);
    of_mask_set_nw_src(&mask, 32);

    mul_app_act_alloc(&mdata);
    mul_app_act_set_ctors(&mdata, sw_dpid);
    mul_app_action_output(&mdata, outport); 

    if(!mul_app_send_flow_add(MY_CONTROLLER_APP_NAME, NULL, sw_dpid, &fl, &mask,
                         (uint32_t)-1, mdata.act_base, mul_app_act_len(&mdata),
                         0, 0, C_FL_PRIO_EXM, C_FL_ENT_NOCACHE))
    {
        c_log_debug("issue flow add success");
    }else
    {
        c_log_debug("issue flow add fail");
    }
    
                        
    return 1;
}