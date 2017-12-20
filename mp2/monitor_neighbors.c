#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>


#include <inttypes.h>
#include "monitor_neighbors.h"
#include "ls_router.h"
#define MSG_LEN                100
#define TIMEOUT   1000000
#define FORWARD  1
#define SEND  2
#define RECEIVE 3
#define UNREACHABLE  4
#define MS_500  500000000

extern int globalMyID;
//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
extern struct timeval globalLastHeartbeat[256];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
extern int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
extern struct sockaddr_in globalNodeAddrs[256];

extern char *logfile_path;
extern struct FTE forward_table[256];
extern char globalLSP[LSP_MAX_LEN];
extern unsigned int len_global_lsp;
/*extern pthread_mutex_t lock;*/

unsigned int parse_send(unsigned char *buf, uint16_t *destID, char *msg, unsigned int buf_length)
{
    unsigned int msg_length = buf_length - 4 - sizeof(uint16_t);
    buf += 4;
    memcpy(destID, buf, sizeof(uint16_t));
    *destID = ntohs(*destID);
    memcpy(msg, buf + sizeof(uint16_t), msg_length);
    return msg_length;
}

void parse_cost(unsigned char *buf, uint16_t *neighborID, uint32_t *newCost)
{
    buf += 4;
    memcpy(neighborID, buf, sizeof(uint16_t));
    memcpy(newCost, buf + sizeof(uint16_t), sizeof(uint32_t));
    *neighborID = ntohs(*neighborID);
    *newCost = ntohl(*newCost);
}

void writeLogType(int logtype, uint16_t dest, uint16_t nexthop, char *msg, unsigned int msg_length)
{
    FILE *fp = fopen(logfile_path, "a");
    char buf[1000];
    unsigned int temp_p;
    memset(buf, '\0', 1000);
    if(logtype == FORWARD){
        temp_p = (unsigned int)sprintf(buf, "forward packet dest %d nexthop %d message ", (int)dest, (int)nexthop);
        strncpy(buf + temp_p, msg, msg_length);
        buf[temp_p + msg_length] = '\n';
        printf("forward:%d -> %d\n", globalMyID, (int)nexthop);
        temp_p = temp_p + msg_length + 1;
    }

    if(logtype == SEND){
        temp_p = (unsigned int)sprintf(buf, "sending packet dest %d nexthop %d message ", (int)dest, (int)nexthop);
        strncpy(buf + temp_p, msg, msg_length);
        buf[temp_p + msg_length] = '\n';
        printf("send:%d -> %d\n", globalMyID, (int)dest);
        temp_p = temp_p + msg_length + 1;
    }

    if(logtype == RECEIVE){
        temp_p = (unsigned int)sprintf(buf, "receive packet message ");
        strncpy(buf + temp_p, msg, msg_length);
        buf[temp_p + msg_length] = '\n';
        printf("receive\n");
        temp_p = temp_p + msg_length + 1;
    }

    if(logtype == UNREACHABLE){
        temp_p = (unsigned int)sprintf(buf, "unreachable dest %d\n", (int)dest);
        printf("can't reach\n");
    }
    
    fwrite(buf, sizeof(char), temp_p, fp);
    fclose(fp);
}



void forward_packet(int nexthop, char *buf, unsigned int buf_length)
{
    sendto(globalSocketUDP, buf, buf_length, 0,(struct sockaddr*)&globalNodeAddrs[nexthop], sizeof(globalNodeAddrs[nexthop]));
}


void* check_timeout(void* unusedParam)
{
    int i;
    struct timeval now;
    struct timespec sleeptime;
    sleeptime.tv_sec = 0;
    sleeptime.tv_nsec = MS_500; //500 ms

    while (1) {
        for (i = 0; i < 256; i ++) {
            if (globalMyID == i) {
                continue;
            }
            if (cost_to_target(globalMyID, i)) {
                gettimeofday(&now, 0);
                if (((now.tv_sec - globalLastHeartbeat[i].tv_sec)*1000000 + now.tv_usec - globalLastHeartbeat[i].tv_usec) > TIMEOUT) {
                    change_cost(globalMyID, i, 0);
                    printf("cost from %d to %d is broken\n", globalMyID, i);
                }
            }
        }
        
        nanosleep(&sleeptime, 0);
    }
}

//Yes, this is terrible. It's also terrible that, in Linux, a socket
//can't receive broadcast packets unless it's bound to INADDR_ANY,
//which we can't do in this assignment.
// hackyBroadcast will broadcast a message to all other nodes
void hackyBroadcast(const char* buf, int length)
{
    int i;
    for(i=0;i<256;i++)
        if(i != globalMyID) //(although with a real broadcast you would also get the packet yourself)
            sendto(globalSocketUDP, buf, length, 0,
                  (struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
}

void* announceToNeighbors(void* unusedParam)
{
    struct timespec sleeptime;
    sleeptime.tv_sec = 0;
    sleeptime.tv_nsec = 300 * 1000 * 1000; //300 ms
    while(1)
    {
        forward_table[globalMyID].num_seq ++;
        hackyBroadcast("HEREIAM", 7);
        nanosleep(&sleeptime, 0);
    }
}

void listenForNeighbors()
{
    char fromAddr[100];
    struct sockaddr_in theirAddr;
    socklen_t theirAddrLen;
    unsigned char recvBuf[1000];

    int bytesRecvd;
    while(1)
    {
        theirAddrLen = sizeof(theirAddr);
        recvBuf[0] = '\0';
        if ((bytesRecvd = recvfrom(globalSocketUDP, recvBuf, 1000 , 0,
                    (struct sockaddr*)&theirAddr, &theirAddrLen)) == -1)
        {
            perror("connectivity listener: recvfrom failed");
            exit(1);
        }

        inet_ntop(AF_INET, &theirAddr.sin_addr, fromAddr, 100);

        short int heardFrom = -1;
        if(strstr(fromAddr, "10.1.1."))
        {
            // heardFrom is the id of the node from where recvFrom() gets the msg
            heardFrom = atoi(
                    strchr(strchr(strchr(fromAddr,'.')+1,'.')+1,'.')+1);

            //TODO: this node can consider heardFrom to be directly connected to it; do any such logic now.
            if (!cost_to_target(globalMyID, heardFrom)) {
                change_cost(globalMyID, heardFrom, forward_table[heardFrom].direct_cost);
            }

            //record that we heard from heardFrom just now.
            gettimeofday(&globalLastHeartbeat[heardFrom], 0);
        }

        //Is it a packet from the manager? (see mp2 specification for more details)
        //send format: 'send'<4 ASCII bytes>, destID<net order 2 byte signed>, <some ASCII message>
        if(!strncmp(recvBuf, "send", 4))
        {
            dijkstra();
            uint16_t destID;
            char msg[MSG_LEN];
            memset(msg, '\0', MSG_LEN);
            unsigned int msg_length = parse_send(recvBuf, &destID, msg, bytesRecvd);

            //TODO send the requested message to the requested destination node
            // ...

            if (globalMyID == (int)destID) {
                // receive message
                writeLogType(RECEIVE, 0,0,msg, msg_length);
            } else {
                int nexthop = forward_table[destID].next_hop;

                if (nexthop == -1) {
                    // unreachable
                    writeLogType(UNREACHABLE,destID,0,0,0);
                } else {
                    forward_packet(forward_table[destID].next_hop, recvBuf, bytesRecvd);
                    if (strncmp(fromAddr, "10.0.0.10", 9) == 0) {
                        writeLogType(SEND, destID, (uint16_t)nexthop, msg, msg_length);
                    } else {
                        writeLogType(FORWARD, destID, (uint16_t)nexthop, msg, msg_length);
                    }
                }
            }
        }
        //'cost'<4 ASCII bytes>, destID<net order 2 byte signed> newCost<net order 4 byte signed>
        else if(!strncmp(recvBuf, "cost", 4))
        {
            uint16_t neighbor;
            uint32_t newCost;

            parse_cost(recvBuf, &neighbor, &newCost);

            //TODO record the cost change (remember, the link might currently be down! in that case,
            //this is the new cost you should treat it as having once it comes back up.)
            // ...
            forward_table[neighbor].direct_cost = newCost;
            if (cost_to_target(globalMyID, neighbor)) {
                change_cost(globalMyID, neighbor, newCost);
            }
        }

        //TODO now check for the various types of packets you use in your own protocol
        else if(!strncmp(recvBuf, "LSP", 3))
        {
            uint16_t node;
            uint32_t num_seq;
            int i, temp_p = (bytesRecvd - 3 - sizeof(uint16_t) * 2) / LEN_6;
            uint16_t *neighbors = malloc(temp_p * sizeof(uint16_t));
            uint32_t *costs = malloc(temp_p * sizeof(uint32_t));
            parse_LSP(recvBuf, &node, neighbors, costs, temp_p, &num_seq, bytesRecvd);

            // forward LSP
            if (num_seq > forward_table[node].num_seq) {
                int j;
        
                for (i = 0; i < 256; i ++) {
                    if (i == node) {
                        continue;
                    }
                    if (cost_to_target(node, i)) {
                        int valid_link = 0;
                        for (j = 0; j < temp_p; j ++) {
                            if (neighbors[j] == (uint16_t)i) {
                                valid_link = 1;
                            }
                        }
                        if (!valid_link) {
                            change_cost(node, i, 0);
                        }
                    }
                }
                for (i = 0; i < temp_p; i ++) {
                    change_cost(neighbors[i], node, costs[i]);
                }
                forward_table[node].num_seq = num_seq;
               sendLSP(heardFrom, recvBuf, bytesRecvd);
            }
        }
    }
    //(should never reach here)
    close(globalSocketUDP);
}

void* send_LSP_repeatedly(void* unusedParam)
{
    struct timeval a;
    gettimeofday(&a, NULL);
    srand((unsigned int)(a.tv_sec * 1000000 + a.tv_usec));
    struct timespec begin;
    begin.tv_sec = 0;
    begin.tv_nsec = (rand() % 1000) * 1000000;
    nanosleep(&begin, 0);
    struct timespec sleeptime;
    sleeptime.tv_sec = 1;
    sleeptime.tv_nsec = 0;
    while(1)
    {
        len_global_lsp = set_LSP(globalLSP, globalMyID);
        sendLSP(globalMyID, globalLSP, len_global_lsp);
        nanosleep(&sleeptime, 0);
    }
}