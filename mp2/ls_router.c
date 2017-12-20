#include "monitor_neighbors.h"
#include "ls_router.h"

#define LEN_16 2
#define LEN_32 4
#define LEN_LSP 3
#define LEN_IP 256


extern int globalMyID;
extern int globalSocketUDP;
extern struct sockaddr_in globalNodeAddrs[LEN_IP];
extern struct FTE forward_table[LEN_IP];
char globalLSP[LSP_MAX_LEN];
unsigned int len_global_lsp;
unsigned int *adjacency_array;

void initialize_adjarray()
{
    adjacency_array = (unsigned int *)malloc(SIZE_ADJARRAY * sizeof(int));
    int i;
    for (i = 0; i < SIZE_ADJARRAY; i ++) {
        adjacency_array[i] = 0;
    }
}

unsigned int cost_to_target(int x, int y)
{
    int index;
    if(x<y){
        index = ((y-1)*y)/2+x;
    }
    else
        index = ((x-1)*x)/2+y;
    return adjacency_array[index];
}

void change_cost(int x, int y, unsigned int cost)
{
    int index;
    if(x<y){
        index = ((y-1)*y)/2+x;
    }
    else
        index = ((x-1)*x)/2+y;
    adjacency_array[index] = cost;
}


unsigned int set_LSP(char *buf, int target)
{
    uint16_t node = htons((uint16_t)target);
    uint32_t num_seq = htonl(forward_table[target].num_seq);
    sprintf(buf, "LSP");
    memcpy(buf + LEN_LSP, &node, LEN_16);
    int i, temp_p = 0;
    for (i = 0; i < LEN_IP; i ++) {
        if(i!=target){
            if (cost_to_target(target, i)) {
                uint16_t neighbor = htons((uint16_t)i);
                uint32_t cost = htonl((uint32_t)cost_to_target(target, i));
                memcpy(buf + LEN_LSP + LEN_16 + LEN_6 * temp_p, &neighbor, LEN_16);
                memcpy(buf + LEN_LSP + LEN_16 * 2 + LEN_6 * temp_p, &cost, LEN_32);
                temp_p ++;
            }
        }
    }
    if (temp_p == 0) {
        return temp_p;
    }
    memcpy(buf + LEN_LSP + LEN_16 + LEN_6 * temp_p, &num_seq, LEN_32);
    return LEN_LSP + LEN_6 * (temp_p + 1);
}

void sendLSP(int source, char *LSP, unsigned int LSP_length)
{
    int i;
    for (i = 0; i < LEN_IP; i ++) {
        if (i != source && i != globalMyID) {
            if (cost_to_target(i, globalMyID)) {
                sendto(globalSocketUDP, LSP, LSP_length, 0, (struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
            }
        }
    }
}



void parse_LSP(char *buf, uint16_t *node, uint16_t *neighbors, uint32_t *costs, int temp_p, uint32_t *num_seq, unsigned int buf_length)
{
    memcpy(node, buf + LEN_LSP, LEN_16);
    memcpy(num_seq, buf + LEN_LSP + LEN_16 + LEN_6 * temp_p, LEN_32);
    *node = ntohs(*node);
    *num_seq = ntohl(*num_seq);
    uint16_t temp_neighbor;
    uint32_t temp_cost;
    int i;
    for (i = 0; i < temp_p; i ++) {
        memcpy(&temp_neighbor, buf + LEN_LSP + LEN_16 + LEN_6 * i, LEN_16);
        memcpy(&temp_cost, buf + LEN_LSP + LEN_16 * 2 + LEN_6 * i, LEN_32);
        neighbors[i] = ntohs(temp_neighbor);
        costs[i] = ntohl(temp_cost);
    }
}


void set_array(struct node *frontier, int frontier_elems, struct node set)
{
    int i;
    for (i = 0; i < frontier_elems; i ++) {
        if (frontier[i].dest == set.dest) {
            frontier[i] = set;
            break;
        }
    }
}


struct node find_next(struct node *frontier, int frontier_elems, int target)
{
    struct node result;
    result.dest = -1;
    result.cost = 0;
    result.next_hop = -1;

    int i;
    for (i = 0; i < frontier_elems; i ++) {
        if (frontier[i].dest == target) {
            return frontier[i];
        }
    }
    return result;
}


void setNode(struct node *node,unsigned int cost,int prevdest,int next_hop){
    node->cost = cost;
    node->prevdest = prevdest;
    node->next_hop = next_hop;
}

struct node *update_frontier(struct node *frontier, int *frontier_elems, int source)
{
    int source_cost = forward_table[source].cost;
    int i;
    for (i = 0; i < LEN_IP; i ++) {
        if (i != source){
            //add all connected nodes
            if (cost_to_target(source, i) && forward_table[i].cost == -1) {
                int edge_cost = cost_to_target(source, i);
                struct node current = find_next(frontier, *frontier_elems, i);
                if (current.dest != -1) {
                    if (current.cost > source_cost + edge_cost) {
                        setNode(&current, source_cost + edge_cost,source, forward_table[source].next_hop);
                        set_array(frontier, *frontier_elems, current);
                    } 

                    else if (current.cost == source_cost + edge_cost) {
                        //tie
                        if (current.prevdest > source) {
                            setNode(&current, source_cost + edge_cost,source, forward_table[source].next_hop);
                            set_array(frontier, *frontier_elems, current);
                        }
                    }
                } 

                else {
                    // add i into frontier
                    if (*frontier_elems == 0) {
                        frontier = (struct node*)malloc(sizeof(struct node));
                    } 

                    else {
                        frontier = (struct node*)realloc(frontier, (*frontier_elems + 1) * sizeof(struct node));
                    }
                    frontier[*frontier_elems].dest = i;
                    frontier[*frontier_elems].prevdest = source;
                    frontier[*frontier_elems].cost = source_cost + edge_cost;
                    
                    if (forward_table[source].next_hop == -1) {
                        frontier[*frontier_elems].next_hop = i;
                    } 

                    else {
                        frontier[*frontier_elems].next_hop = forward_table[source].next_hop;
                    }
                    *frontier_elems += 1;
                }
            }
        }
    }
    return frontier;
}

struct node *updata_forwardtable(struct node *frontier, int *frontier_elems, int *source)
{
    if (*frontier_elems == 0) {
        *source = -1;
        return frontier;
    }
    //find the smallest
    int total_elem = *frontier_elems;
    int j = 0;
    struct node shortest = frontier[0];
    for (j = 0; j < total_elem; j ++) {
        if (shortest.cost > frontier[j].cost) {
            shortest = frontier[j];
        }
    }

    // struct node shortest = getshortest(frontier, *frontier_elems);
    *frontier_elems = *frontier_elems - 1;
    *source = shortest.dest;

    // set forward_table
    forward_table[shortest.dest].cost = shortest.cost;
    forward_table[shortest.dest].next_hop = shortest.next_hop;

    if (*frontier_elems == 0) {
        return NULL;
    }
    struct node *result = (struct node*)malloc(*frontier_elems * sizeof(struct node));
    int i, temp_p = 0;
    for (i = 0; i < *frontier_elems + 1; i ++) {
        if (shortest.dest != frontier[i].dest) {
            memcpy(&result[temp_p], &(frontier[i]), sizeof(struct node));
            temp_p ++;
        }
    }
    free(frontier);
    frontier = NULL;
    return result;
}


void dijkstra()
{
    struct node *frontier;
    int i;
    int frontier_elems = 0;
    int source = globalMyID;
    for (i = 0; i < LEN_IP; i ++) {
        forward_table[i].next_hop = -1;
        forward_table[i].cost = -1;
    }
    //cost to self 0
    forward_table[globalMyID].cost = 0;
    //start of dijkstra
    while (source != -1) {
        frontier = update_frontier(frontier, &frontier_elems, source);
        frontier = updata_forwardtable(frontier, &frontier_elems, &source);
    }
    return;
}
