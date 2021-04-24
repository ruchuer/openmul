#include "LLDP.h"
#include "tp_graph.h"
#include <sys/time.h>

extern tp_sw * tp_graph;
extern tp_swdpid_glabolkey * key_table;

uint64_t lldp_get_timeval(void)
{
    struct timeval t;
    gettimeofday(&t, 0);
    //c_log_debug("get time sec: %ld, usec: %ld", t.tv_sec, t.tv_usec);
    return t.tv_sec*1000000 + t.tv_usec;//us
}

/**
 * lldp_create_packet -
 *
 * Generates LLDP_PACKET with source switch id/port into specified buffer
 * src_addr:sw_port MAC address
 * srcID: sw_dpid
 */
void lldp_create_packet(void *src_addr, uint64_t srcId, uint32_t srcPort, 
                        lldp_pkt_t *buffer, uint8_t user_type)
{
    //nearest bridge
    uint8_t dest_addr[OFP_ETH_ALEN] = {0x01,0x80,0xc2,0x00,0x00,0x0e};

    //data link information
    memcpy(buffer->eth_head.eth_dst,dest_addr,OFP_ETH_ALEN);
    memcpy(buffer->eth_head.eth_src,src_addr,OFP_ETH_ALEN);
    buffer->eth_head.eth_type = htons(0x88cc);
    
    //the switch information (sw_dpid)
    buffer->chassis_tlv_type = LLDP_CHASSIS_ID_TLV;
    buffer->chassis_tlv_length = sizeof(uint64_t) + 1;
    buffer->chassis_tlv_subtype = LLDP_CHASSIS_ID_LOCALLY_ASSIGNED;
    buffer->chassis_tlv_id = htonll(srcId);

    //the switch outport
    buffer->port_tlv_type = LLDP_PORT_ID_TLV;
    buffer->port_tlv_length = sizeof(uint32_t) + 1;
    buffer->port_tlv_subtype = LLDP_PORT_ID_LOCALLY_ASSIGNED;
    buffer->port_tlv_id = htons(srcPort);

    //lldp pkt ttl set
    buffer->ttl_tlv_type = LLDP_TTL_TLV;
    buffer->ttl_tlv_length = 2;
    buffer->ttl_tlv_ttl = htons(LLDP_DEFAULT_TTL);

    //lldp user data(include time stamp)
    buffer->user_tlv_type = LLDP_USERE_LINK_DATA;
    buffer->user_tlv_length = sizeof(uint64_t)+sizeof(uint8_t);
    buffer->user_tlv_data_type = user_type;
    buffer->user_tlv_data_timeval = htonll(lldp_get_timeval());

    //lldp pkt end
    buffer->end_of_lldpdu_tlv_type = LLDP_END_OF_LLDPDU_TLV;
    buffer->end_of_lldpdu_tlv_length = 0;
}

void lldp_measure_delay_ctos(uint64_t sw_dpid)
{
    uint8_t src_addr[OFP_ETH_ALEN] = {0x01,0x01,0x01,0x01,0x01,0x01};
    lldp_pkt_t buffer;
    lldp_create_packet(src_addr, sw_dpid, OF_NO_PORT, &buffer, USER_TLV_DATA_TYPE_CTOS);
    lldp_send_packet(sw_dpid, &buffer, 0);
}

//process the lldp packet
void lldp_proc(mul_switch_t *sw, struct flow *fl, uint32_t inport, uint32_t buffer_id, \
              uint8_t *raw, size_t pkt_len)
{
    uint64_t now_timeval, delay;
    uint64_t sw_dpid1 = sw->dpid;
    tp_sw * sw1 = tp_find_sw(tp_get_sw_glabol_id(sw_dpid1, key_table), tp_graph);
    lldp_pkt_t * lldp = (lldp_pkt_t*)raw;
    uint64_t sw_dpid2 = ntohll(lldp->chassis_tlv_id);
    tp_sw * sw2 = tp_find_sw(tp_get_sw_glabol_id(sw_dpid2, key_table), tp_graph);
    uint8_t ret = 0;

    gettimeofday(&now_timeval, NULL);

    switch (lldp->user_tlv_data_type)
    {
    case USER_TLV_DATA_TYPE_CTOS:
        sw1->delay = (now_timeval-ntohll(lldp->user_tlv_data_timeval))/2;
        //flood lldp 
        lldp_flood(sw);
        break;
    case USER_TLV_DATA_TYPE_STOS:
        if(sw2)
        {//in the same area
            //add edge between the sw_node. return 0 if added them before
            tp_add_link(sw1->key, inport, sw2->key, lldp->port_tlv_id, tp_graph);
            delay = now_timeval-ntohll(lldp->user_tlv_data_timeval);
            delay -= (tp_find_sw(sw1->key, tp_graph)->delay + tp_find_sw(sw2->key, tp_graph)->delay);
            //connect to the database if the link between area and area
            do{
                TP_SET_LINK(sw1->key, sw2->key, delay, tp_graph, ret);
            }while(ret == 0);
        }
        else
        {
            //LLDP from other area
        }
        
        break;
    default:
        c_log_debug("ERROR LLDP TYPE");
        break;
    }
}

void lldp_send_packet(uint64_t sw_dpid, lldp_pkt_t *buffer, uint32_t outport)
{
    struct of_pkt_out_params  parms;
    struct mul_act_mdata      mdata;

    memset(&parms, 0, sizeof(parms));
    mul_app_act_alloc(&mdata);
    mdata.only_acts = true;
    mul_app_act_set_ctors(&mdata, sw_dpid);
    mul_app_action_output(&mdata, outport);
    parms.buffer_id = -1;
    parms.in_port = OF_NO_PORT;
    parms.action_list = mdata.act_base;
    parms.action_len = mul_app_act_len(&mdata);
    parms.data_len = sizeof(lldp_pkt_t);
    parms.data = buffer;
    mul_app_send_pkt_out(NULL, sw_dpid, &parms);
    mul_app_act_free(&mdata);
}

void lldp_flood(mul_switch_t *sw)
{
    //uint64_t sw_srcdpid = sw->dpid;
    lldp_pkt_t buffer;
    tp_sw_port * port_list = tp_find_sw(tp_get_sw_glabol_id(sw->dpid, key_table), tp_graph)->list_port;
    
    //create the lldp packet due to get the adj
    //lldp_create_packet(port_infor.hw_addr, sw_srcdpid, port_infor.port_no, &buffer);
    while(port_list)
    {
        lldp_create_packet(port_list->dl_hw_addr, sw->dpid, port_list->port_no, &buffer,\
                           USER_TLV_DATA_TYPE_STOS);
        lldp_send_packet(sw->dpid, &buffer, port_list->port_no);
        port_list = port_list->next;
    }    
}