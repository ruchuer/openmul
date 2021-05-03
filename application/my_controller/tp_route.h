#ifndef __MUL_TP_ROUTE_H__
#define __MUL_TP_ROUTE_H__

#include "tp_graph.h"
#include "mul_common.h"

/**
 * the function of ip route and issue flow_table
 * @nw_src: ip source address
 * @nw_dst: ip destination
 * @sw_key: the switch of flow in
 * @return: success 1, fail 0
 */
int rt_ip(uint32_t nw_src, uint32_t nw_dst, uint32_t sw_key);

//use to store the pre_node
typedef struct rt_node_t{
    uint32_t prev_key; //read only
    uint32_t key; //node id
    UT_hash_handle hh;         /* makes this structure hashable */
}rt_node;

/**
 * use the key to find node from rt_set
 * @key: the node ky
 * @rt_set: route_set handler
 * @return: the corresponding rt_node
 */
rt_node * rt_find_node(uint32_t key, rt_node ** rt_set);

/**
 * add a node to re_set
 * @key: the node key
 * @prev_key: precursor key
 * @rt_set: route_set handler
 * @return: success added_node, fail NULL
 */
rt_node * rt_add_node(uint32_t key, uint32_t prev_key, rt_node ** rt_set);

/**
 * delete a node from re_set
 * @key: the node ky
 * @rt_set: route_set handler
 * @return: success 1, fail 0
 */
int rt_del_node(uint32_t key, rt_node ** rt_set);

/**
 * Destroys and cleans up route_set.
 * @rt_set: the dst set
 */
void rt_distory(rt_node ** rt_set);

/**
 * load the topo from redis
 * @return: success 1, fail 0
*/
int rt_load_glabol_topo(void);

/**
 * get the a path between src_ip to dst_ip from redis, and than set flow in this path
 * @src_ip: source ip address
 * @dst_ip: destination ip address
 * @return: success 1, fail 0
*/
int rt_set_ip_flow_path_from_redis(uint32_t src_ip, uint32_t dst_ip);

/**
 * set the flow in switch
 * @sw_dpid: switch dpid
 * @src_ip: source ip address
 * @dst_ip: destination ip address
 * @outport: the outport of flow
 * @return: success 1, fail 0
*/
int rt_issue_flow(uint64_t sw_dpid, uint32_t src_ip, uint32_t dst_ip, uint32_t outport);

/**
 * calculate the route path
 * @sw_start: source sw key
 * @sw_end: destination sw key
 * @return: success rt_node contained the route path, fail NULL
*/
rt_node* rt_ip_get_path(uint32_t sw_start, uint32_t sw_end);

#endif