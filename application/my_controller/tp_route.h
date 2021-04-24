#ifndef __MUL_TP_ROUTE_H__
#define __MUL_TP_ROUTE_H__

#include "tp_graph.h"
#include "mul_common.h"

/**
 * the function of stp route and issue flow_table
 * @sw: switch argument passed by infra layer (read-only)
 * @fl: Flow associated with the packet-in
 * @inport: in-port that this packet-in was received
 * @buffer_id: packet_in buffer_id
 * @raw: Raw packet data pointer
 * @pkt_len: Packet length
 * @return: success 1, fail 0
 */
int tp_rt_arp_stp(mul_switch_t *sw, struct flow *fl, uint32_t inport, uint32_t buffer_id, \
                   uint8_t *raw, size_t pkt_len);

/**
 * the function of ip route and issue flow_table
 * @nw_src: ip source address
 * @nw_dst: ip destination
 * @return: success 1, fail 0
 */
int tp_rt_ip(uint32_t nw_src, uint32_t nw_dst);

//use to store the pre_node
typedef struct rt_node_t{
    tp_link * node; //read only
    tp_sw * prev; //read only
    uint32_t key; //node id
    UT_hash_handle hh;         /* makes this structure hashable */
}rt_node;

/**
 * use the key to find node from rt_set
 * @key: the node key(sw_dpid or host ip)
 * @rt_set: route_set handler
 * @return: the corresponding rt_node
 */
rt_node * rt_find_node(uint64_t key, rt_node * rt_set);

/**
 * add a node to re_set
 * @key: the node key(sw_dpid or host ip)
 * @prev: iprecursor node
 * @node: this link information
 * @rt_set: route_set handler
 * @return: success added_node, fail NULL
 */
rt_node * rt_add_node(uint64_t key, tp_sw *prev, tp_link *node, rt_node * rt_set);

/**
 * delete a node from re_set
 * @key: the node key(sw_dpid or host ip)
 * @rt_set: route_set handler
 * @return: success 1, fail 0
 */
int rt_del_node(uint64_t key, rt_node * rt_set);

/**
 * Destroys and cleans up route_set.
 * @rt_set: the dst set
 */
void rt_distory(rt_node * rt_set);

/**
 * rt_visited_set store a path from src_ip to dst_ip, and than set flow in this path
 * @src_ip: source ip address
 * @dst_ip: destination ip address
 * @rt_visited_set: store the visited switch that also store the path
*/
void rt_set_ip_flow_path(uint32_t src_ip, uint32_t dst_ip, rt_node * rt_visited_set);

#endif