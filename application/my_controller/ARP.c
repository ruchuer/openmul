#include "ARP.h"
#include "tp_graph.h"

extern tp_swdpid_glabolkey * key_table;
arp_hash_table_t * arp_table = NULL;

void arp_add_key(uint32_t key_ip, uint8_t dl_hw_addr[ETH_ADDR_LEN]) 
{
    arp_hash_table_t *s;

    s = malloc(sizeof(arp_hash_table_t));
    s->id = key_ip;
    strncpy((char*)s->dl_hw_addr, (const char*)dl_hw_addr, ETH_ADDR_LEN);
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
    arp_add_key(arp_req->ar_spa, arp_req->ar_sha);
    tmp = arp_find_key(arp_req->ar_spa);
    tmp->sw_key = tp_get_sw_glabol_id(sw_dpid);
    tmp->port_no = port;
}


void arp_proc(mul_switch_t *sw, struct flow *fl, uint32_t inport, uint32_t buffer_id, \
              uint8_t *raw, size_t pkt_len)
{
    struct arp_eth_header     *arp;
    struct of_pkt_out_params  parms;
    struct mul_act_mdata      mdata;
    arp_hash_table_t * s = NULL;

    arp = (void *)(raw + sizeof(struct eth_header)  +
                   (fl->dl_vlan ? VLAN_HEADER_LEN : 0));

    //c_log_info("my_controller app - ARP SRC IP KEY %d!", arp->ar_tpa);
    s = arp_find_key(arp->ar_spa);
    if(s)
    {
        arp_delete_key(arp->ar_spa);
    }
    arp_learn(arp, sw->dpid, inport);

    memset(&parms, 0, sizeof(parms));
    mul_app_act_alloc(&mdata);
    mdata.only_acts = true;
    mul_app_act_set_ctors(&mdata, sw->dpid);
    if(htons(arp->ar_op) == 1){
        //arp request
        s = arp_find_key(arp->ar_tpa);
        if(s)
        {
            //arp cache reply
            c_log_info("ARP Cache reply!");
            mul_app_action_output(&mdata, inport);
            parms.buffer_id = buffer_id;
            parms.in_port = OF_NO_PORT;
            parms.action_list = mdata.act_base;
            parms.action_len = mul_app_act_len(&mdata);
            parms.data_len = sizeof(struct eth_header) + sizeof(struct arp_eth_header);
            parms.data = get_proxy_arp_reply(arp, s->dl_hw_addr);
            mul_app_send_pkt_out(NULL, sw->dpid, &parms);
            mul_app_act_free(&mdata);
        }else
        {
            //STP flood
            c_log_info("ARP Stp Flood!");
            mul_app_action_output(&mdata, OF_ALL_PORTS);
            parms.buffer_id = buffer_id;
            parms.in_port = inport;
            parms.action_list = mdata.act_base;
            parms.action_len = mul_app_act_len(&mdata);
            parms.data_len = pkt_len;
            parms.data = raw;
            mul_app_send_pkt_out(NULL, sw->dpid, &parms);
            mul_app_act_free(&mdata);
        }
    }else
    {
        //arp reply route trans
        c_log_info("ARP host Reply!");
        mul_app_action_output(&mdata, OF_ALL_PORTS);
        parms.buffer_id = buffer_id;
        parms.in_port = inport;
        parms.action_list = mdata.act_base;
        parms.action_len = mul_app_act_len(&mdata);
        parms.data_len = pkt_len;
        parms.data = raw;
        mul_app_send_pkt_out(NULL, sw->dpid, &parms);
        mul_app_act_free(&mdata);
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
