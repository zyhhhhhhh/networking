
#include <netinet/in.h>
#include <string.h>
#include <sys/time.h>

struct FTE {
    unsigned int direct_cost;
    int cost;
    int next_hop;
    uint32_t num_seq;
    
};

void* check_timeout(void* unusedParam);
void* announceToNeighbors(void* unusedParam);
void hackyBroadcast(const char* buf, int length);
void listenForNeighbors();
void* send_LSP_repeatedly(void* unusedParam);
void writeLogType(int logtype, uint16_t dest, uint16_t nexthop, char *msg, unsigned int msg_length);
