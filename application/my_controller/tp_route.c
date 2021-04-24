#include "tp_route.h"
#include "heap.h"

extern tp_sw * tp_graph;

rt_node * rt_find_node(uint64_t key, rt_node * rt_set)
{
    rt_node *s = NULL;
    HASH_FIND(hh, rt_set, &key, sizeof(uint64_t), s);
    return s;
}

rt_node * rt_add_node(uint64_t key, tp_sw *prev, tp_link *node, rt_node * rt_set)
{
    rt_node *s = NULL;

    if(rt_find_node(key, rt_set))return NULL;

    s = malloc(sizeof(rt_node));
    memset(s, 0, sizeof(rt_node));
    s->key = key;
    s->prev = prev;
    s->node = node;
    HASH_ADD(hh, rt_set, key, sizeof(uint64_t), s);

    return s;
}

int rt_del_node(uint64_t key, rt_node * rt_set)
{
    rt_node *s = NULL;

    s = rt_find_node(key, rt_set);
    if(!s)return 0;
    HASH_DEL(rt_set, s);
    free(s);

    return 1;
}

void rt_distory(rt_node * rt_set)
{
    rt_node * s, * tmp;

    HASH_ITER(hh, rt_set, s, tmp) 
    {
        HASH_DEL(rt_set, s);
        free(s);
    }

    rt_set = NULL;
}

void tp_set_ip_flow_path(uint64_t src_key, uint64_t dst_key, rt_node * rt_visited_set)
{
    rt_node *s = NULL;
    while(dst_key != src_key)
    {
        s = rt_find_node(dst_key, rt_visited_set);
        //流表下发
    }
}

int tp_rt_ip(mul_switch_t *sw, struct flow *fl, uint32_t inport, uint32_t buffer_id, \
             uint8_t *raw, size_t pkt_len)
{
    tp_sw * src_node = tp_find_sw(fl->ip.nw_src, tp_graph);
    tp_sw * dst_node = tp_find_sw(fl->ip.nw_dst, tp_graph);
    tp_sw * start_node = src_node;
    tp_link * adj_node;
    heap rt_minheap;
    rt_node * rt_visited_set;
    uint64_t * delay_get;
    uint64_t * sw_visiting_dpid;
    uint64_t delay_set = 0;

    if(!src_node && !dst_node) return 0;

    heap_create(&rt_minheap, 0, NULL);
    rt_add_node(src_node->key, NULL, src_node, rt_visited_set);
    heap_insert(&rt_minheap, (void*)&sw->dpid, (void*)&src_node->delay);

    while(heap_size(&rt_minheap))
    {
        heap_min(&rt_minheap, (void*)&sw_visiting_dpid, (void*)&delay_get);
        start_node = tp_find_sw(*sw_visiting_dpid, tp_graph);
        adj_node = start_node->list_link;//找到一个点开始遍历
        if(adj_node = NULL) continue;
        while(adj_node)//遍历邻接点
        {
            if(adj_node->key == dst_node->key)
            {
                //找到了，下发流表
                rt_add_node(dst_node->key, start_node, dst_node, rt_visited_set);
                tp_set_ip_flow_path(src_node->key, dst_node->key, rt_visited_set);
                rt_distory(rt_visited_set);
                heap_destroy(&rt_minheap);
                return 1;
            }
            if(!rt_find_node(adj_node->key, rt_visited_set))//没有被遍历过
            {
                delay_set = *delay_get + adj_node->delay;
                heap_insert(&rt_minheap, (void*)&adj_node->key, (void*)&delay_set);//添加要遍历的点
                rt_add_node(adj_node->key, start_node, adj_node, rt_visited_set);//添加已经遍历的点
            }
            adj_node = adj_node->next;
        }
    }

    rt_distory(rt_visited_set);
    heap_destroy(&rt_minheap);
    return 0;
}
