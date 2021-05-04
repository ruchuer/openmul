#include "ARP.h"
#include "tp_graph.h"
#include "redis_interface.h"
#include <string.h>
#include <msg_udp.h>
#include <pthread.h>

extern tp_swdpid_glabolkey * key_table;
arp_hash_table_t * arp_table = NULL;
extern tp_sw * tp_graph;
extern uint32_t controller_area;
extern redisContext *context;

void* arp_listen(void *arg)
{
    uint8_t buf[UDP_BUFF_LEN] = {'\0'};
    uint8_t src_addr[OFP_ETH_ALEN] = {0x02, 0x42, 0xf7, 0x6d, 0x93, 0x67};
    struct of_pkt_out_params  parms;
    struct mul_act_mdata      mdata;
    struct arp_eth_header     *arp;
    struct eth_header         *eth;
    tp_sw * sw_tmp1, * sw_tmp2;
    int len = 0;

	while(1)
	{
        len = msg_udp_listen((uint8_t*)buf);
        if(len>0)
        {
            eth = (struct eth_header*)buf;
            arp = (void *)(buf + sizeof(struct eth_header));
            c_log_debug("get a arp req to %x", arp->ar_tpa);
            memset(&parms, 0, sizeof(parms));
            mul_app_act_alloc(&mdata);
            mdata.only_acts = true;
            mul_app_act_set_ctors(&mdata, tp_graph->sw_dpid);
            mul_app_action_output(&mdata, OF_ALL_PORTS); 
            parms.buffer_id = -1;
            parms.in_port =OF_NO_PORT;
            parms.action_list = mdata.act_base;
            parms.action_len = mul_app_act_len(&mdata);
            parms.data_len = len;
            memcpy(arp->ar_sha, src_addr, OFP_ETH_ALEN);
            memcpy(eth->eth_src, src_addr, OFP_ETH_ALEN);
            parms.data = buf;
            HASH_ITER(hh, tp_graph, sw_tmp1, sw_tmp2) 
            {
                if((sw_tmp1->key & 0xffff0000) == controller_area)
                {
                    mul_app_send_pkt_out(NULL, sw_tmp1->sw_dpid, &parms);
                }
            }
            mul_app_act_free(&mdata);
        }
        // c_log_debug("no arp req");
        pthread_testcancel();
	}
}

void arp_send_to_other_ctrl(uint8_t *raw, size_t pkt_len)
{
    redisReply * reply, **reply_tmp;
	char cmd[CMD_MAX_LENGHT] = {0};
    uint32_t cursor, dst_ip, dst_id;
    int i;

    if(!context)return;
    c_log_debug("Send arp req to other area, get all controller ip!");
    snprintf(cmd, CMD_MAX_LENGHT, "hscan ctrl 0 match * count 100");
	reply = (redisReply *)redisCommand(context, cmd);
	if (NULL == reply)
	{
		printf("%d execute command:%s failure\n", __LINE__, cmd);
		return;
	}
	// c_log_debug("get all controller ip! reply");
	if(reply->elements != 0)
	{
		cursor = atoi(reply->element[0]->str);
		while(cursor != 0)
		{
			reply_tmp = reply->element[1]->element;	
			for(i = 0; i<reply->element[1]->elements; i++)
			{
				dst_ip = atoi(reply_tmp[i++]->str);
				dst_id = atoi(reply_tmp[i]->str);
                if(dst_id !=controller_area)
                {
                    if(msg_send(dst_ip, raw, pkt_len))
                    {
                        c_log_debug("send arp req to ip%x", dst_ip);
                    }else
                    {
                        c_log_debug("send arp req fail");
                    }
                }  
			}
			freeReplyObject(reply);
			memset(cmd, 0, CMD_MAX_LENGHT);
			snprintf(cmd, CMD_MAX_LENGHT, "hscan ctrl %u match * count 100", cursor);
			reply = (redisReply *)redisCommand(context, cmd);
			if (NULL == reply)
			{
				printf("%d execute command:%s failure\n", __LINE__, cmd);
				return;
			}
			cursor = atoi(reply->element[0]->str);
		}
        c_log_debug("sw add to topo");
		reply_tmp = reply->element[1]->element;	
		for(i = 0; i<reply->element[1]->elements; i++)
		{
			dst_ip = atoi(reply_tmp[i++]->str);
            dst_id = atoi(reply_tmp[i]->str);
            if(dst_id !=controller_area)
            {
                if(msg_send(dst_ip, raw, pkt_len))
                {
                    c_log_debug("send arp req to ip%x", dst_ip);
                }else
                {
                    c_log_debug("send arp req fail");
                }
            }                
		}
	}
    freeReplyObject(reply);
}

void arp_add_key(uint32_t key_ip, uint8_t dl_hw_addr[ETH_ADDR_LEN]) 
{
    arp_hash_table_t *s;

    if(arp_find_key(key_ip))return;
    s = malloc(sizeof(arp_hash_table_t));
    s->id = key_ip;
    memcpy(s->dl_hw_addr, dl_hw_addr, ETH_ADDR_LEN);
    // c_log_debug("src s->dl_hw_addr learn mac %x%x%x%x%x%x", s->dl_hw_addr[0],s->dl_hw_addr[1],s->dl_hw_addr[2],s->dl_hw_addr[3],s->dl_hw_addr[4],s->dl_hw_addr[5]);
    // c_log_debug("src dl_hw_addr learn mac %x%x%x%x%x%x", dl_hw_addr[0],dl_hw_addr[1],dl_hw_addr[2],dl_hw_addr[3],dl_hw_addr[4],dl_hw_addr[5]);
    HASH_ADD_INT(arp_table, id, s);  /* id: name of key field */
}

arp_hash_table_t* arp_find_key(uint32_t key_ip)
{
    arp_hash_table_t* s = NULL;
    HASH_FIND_INT(arp_table, &key_ip, s);
    return s;
}

void arp_delete_key(uint32_t key_ip)
{
    arp_hash_table_t* s = NULL;
    s = arp_find_key(key_ip);
    HASH_DEL(arp_table, s);  /* user: pointer to deletee */
    free(s);             /* optional; it's up to you! */
}


void arp_distory(void)
{
    arp_hash_table_t * s, * tmp;

    HASH_ITER(hh, arp_table, s, tmp) {
      HASH_DEL(arp_table, s);
      free(s);
    }

    arp_table = NULL;
}

void arp_learn(struct arp_eth_header *arp_req, uint64_t sw_dpid, uint32_t port)
{
    arp_hash_table_t * tmp;
    c_log_debug("src learn mac %x%x%x%x%x%x", arp_req->ar_sha[0],arp_req->ar_sha[1],arp_req->ar_sha[2],arp_req->ar_sha[3],arp_req->ar_sha[4],arp_req->ar_sha[5]);
    arp_add_key(arp_req->ar_spa, arp_req->ar_sha);
    tmp = arp_find_key(arp_req->ar_spa);
    tmp->sw_key = tp_get_sw_glabol_id(sw_dpid);
    tmp->port_no = port;
    // c_log_debug("src learn end mac %x%x%x%x%x%x", tmp->dl_hw_addr[0], tmp->dl_hw_addr[1], tmp->dl_hw_addr[2], tmp->dl_hw_addr[3], tmp->dl_hw_addr[4], tmp->dl_hw_addr[5]);

    //write in redis database
    // c_log_debug("set port and sw_key");
    redis_Set_Pc_Sw_Port(arp_req->ar_spa, tmp->sw_key, port);
    // c_log_debug("set mac");
    redis_Set_Pc_MAC(arp_req->ar_spa, arp_req->ar_sha);
    // c_log_debug("set maced");
}


void arp_proc(mul_switch_t *sw, struct flow *fl, uint32_t inport, uint32_t buffer_id, \
              uint8_t *raw, size_t pkt_len)
{
    struct arp_eth_header     *arp;
    struct eth_header         *eth;
    struct of_pkt_out_params  parms;
    struct mul_act_mdata      mdata;
    uint8_t src_addr[OFP_ETH_ALEN] = {0x02, 0x42, 0xf7, 0x6d, 0x93, 0x67};
    tp_sw * sw_tmp1, * sw_tmp2;
    uint8_t mac[6] = {'\0'};
    // uint32_t sw_key, port_no;

    eth = (struct eth_header*)raw;
    arp = (void *)(raw + sizeof(struct eth_header)  +
                   (fl->dl_vlan ? VLAN_HEADER_LEN : 0));

    // c_log_info("my_controller app - ARP SRC IP KEY %d!", arp->ar_tpa);
    // s = arp_find_key(arp->ar_spa);
    // if(s)
    // {
    //     arp_delete_key(arp->ar_spa);
    // }
    arp_learn(arp, sw->dpid, inport);

    memset(&parms, 0, sizeof(parms));
    mul_app_act_alloc(&mdata);
    mdata.only_acts = true;
    if(htons(arp->ar_op) == 1){
        //arp request
        // s = arp_find_key(arp->ar_tpa);
        // c_log_debug("1");
        if(redis_Get_Pc_MAC(arp->ar_tpa, mac))
        {
            //arp cache reply
            c_log_info("ARP Cache reply!");
            mul_app_act_set_ctors(&mdata, sw->dpid);
            mul_app_action_output(&mdata, inport);
            parms.buffer_id = buffer_id;
            parms.in_port = OF_NO_PORT;
            parms.action_list = mdata.act_base;
            parms.action_len = mul_app_act_len(&mdata);
            parms.data_len = sizeof(struct eth_header) + sizeof(struct arp_eth_header);
            parms.data = get_proxy_arp_reply(arp, mac);
            mul_app_send_pkt_out(NULL, sw->dpid, &parms);
            mul_app_act_free(&mdata);
        }else
        {
            //STP flood c2s
            c_log_info("ARP Flood!");
            mul_app_act_set_ctors(&mdata, sw->dpid);
            mul_app_action_output(&mdata, OF_ALL_PORTS); 
            parms.buffer_id = buffer_id;
            parms.in_port =OF_NO_PORT;
            parms.action_list = mdata.act_base;
            parms.action_len = mul_app_act_len(&mdata);
            parms.data_len = pkt_len;
            memcpy(arp->ar_sha, src_addr, OFP_ETH_ALEN);
            memcpy(eth->eth_src, src_addr, OFP_ETH_ALEN);
            parms.data = raw;
            arp_send_to_other_ctrl(raw, pkt_len);
            HASH_ITER(hh, tp_graph, sw_tmp1, sw_tmp2) 
            {
                if((sw_tmp1->key & 0xffff0000) == controller_area)
                {
                    mul_app_send_pkt_out(NULL, sw_tmp1->sw_dpid, &parms);
                    //c_log_debug("Send a arp req pkt to sw %x", sw_tmp1->sw_dpid);
                }
            }
            mul_app_act_free(&mdata);
        }
    }else
    {
        //arp reply route trans
        c_log_info("ARP host Reply! learn only");
    //     redis_Get_Pc_MAC(arp->ar_tpa, mac);
    //     memcpy(arp->ar_tha, mac, OFP_ETH_ALEN);
    //     c_log_debug("src mac %x%x%x%x%x%x", arp->ar_sha[0],arp->ar_sha[1],arp->ar_sha[2],arp->ar_sha[3],arp->ar_sha[4],arp->ar_sha[5]);
    //     memcpy(eth->eth_dst, arp->ar_tha, OFP_ETH_ALEN);
    //     c_log_debug("src mac %x%x%x%x%x%x", eth->eth_dst[0],eth->eth_dst[1],eth->eth_dst[2],eth->eth_dst[3],eth->eth_dst[4],eth->eth_dst[5]);
    //     redis_Get_Pc_Sw_Port(arp->ar_tpa, &sw_key, &port_no);
    //     mul_app_act_set_ctors(&mdata, tp_find_sw(sw_key)->sw_dpid);
    //     mul_app_action_output(&mdata, port_no);
    //     parms.buffer_id = buffer_id;
    //     parms.in_port = OF_NO_PORT;
    //     parms.action_list = mdata.act_base;
    //     parms.action_len = mul_app_act_len(&mdata);
    //     parms.data_len = pkt_len;
    //     parms.data = raw;
    //     mul_app_send_pkt_out(NULL, tp_find_sw(sw_key)->sw_dpid, &parms);
    //     mul_app_act_free(&mdata);
    }
}

void * get_proxy_arp_reply(struct arp_eth_header *arp_req, uint8_t fab_mac[ETH_ADDR_LEN])
{
    uint8_t               *out_pkt;
    struct eth_header     *eth;
    struct arp_eth_header *arp_reply;

    out_pkt = malloc(sizeof(struct arp_eth_header) +
                         sizeof(struct eth_header));

    eth = (struct eth_header *)out_pkt;
    arp_reply = (struct arp_eth_header *)(eth + 1);
    
    memcpy(eth->eth_dst, arp_req->ar_sha, ETH_ADDR_LEN);
    memcpy(eth->eth_src, fab_mac, ETH_ADDR_LEN);
    eth->eth_type = htons(ETH_TYPE_ARP);

    arp_reply->ar_hrd = htons(ARP_HRD_ETHERNET);
    arp_reply->ar_pro = htons(ARP_PRO_IP); 
    arp_reply->ar_pln = IP_ADDR_LEN;
    arp_reply->ar_hln = ETH_ADDR_LEN;
    arp_reply->ar_op = htons(ARP_OP_REPLY);

    memcpy(arp_reply->ar_sha, fab_mac, ETH_ADDR_LEN);
    arp_reply->ar_spa = arp_req->ar_tpa;
    memcpy(arp_reply->ar_tha, arp_req->ar_sha, ETH_ADDR_LEN);
    arp_reply->ar_tpa = arp_req->ar_spa;

    return out_pkt; 
}
