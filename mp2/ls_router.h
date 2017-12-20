#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <pthread.h>

#define SIZE_ADJARRAY   32640
#define LEN_6        6
#define LSP_MAX_LEN          2000

struct node {
    int dest;
    unsigned int cost;
    int prevdest;
    int next_hop;
};

void initialize_adjarray();

unsigned int cost_to_target(int i, int j);
void change_cost(int i, int j, unsigned int newCost);
unsigned int set_LSP(char *buf, int target);
void sendLSP(int source, char *LSP, unsigned int LSP_length);
void parse_LSP(char *buf, uint16_t *node, uint16_t *neighbors, uint32_t *costs, int temp_p, uint32_t *num_seq, unsigned int buf_length);
void dijkstra();
