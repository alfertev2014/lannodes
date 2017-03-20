#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

// int types
#include <stdint.h>

// memset
#include <string.h>

#include <sys/types.h>
#include <ifaddrs.h>

// socket, ip
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

// close fd
#include <unistd.h>

struct NetworkingConfig
{
    uint16_t udpPort;
};


int bindDgramSocket(struct sockaddr_in *addr)
{
    int socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (socketFd < 0)
    {
        // TODO: Log error
        return -1;
    }

    int broadcastSocketOption = 1;
    if (setsockopt(socketFd,
           SOL_SOCKET,
           SO_BROADCAST,
           &broadcastSocketOption,
           sizeof(broadcastSocketOption)
            ) < 0)
    {
        // TODO: Log error
        return -1;
    }


    if (bind(socketFd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0)
    {
        // TODO: Log error
        return -1;
    }

    // success
    return socketFd;
}

void initSockedAddress(struct sockaddr_in *addr, uint32_t ipAddress, uint16_t port)
{
    addr->sin_family = AF_INET;
    addr->sin_port = htonl(port);
    addr->sin_addr.s_addr = htonl(ipAddress);
}

struct Networking
{
    struct sockaddr_in selfDgramAddress;
    struct sockaddr_in broadcastDgramAddress;
    int dgramSocketFd;

    int init(struct NetworkingConfig *config) {


        initSockedAddress(&this->selfDgramAddress, INADDR_ANY, config->udpPort);
        initSockedAddress(&this->broadcastDgramAddress, INADDR_BROADCAST, config->udpPort);

        int socketFd = bindDgramSocket(&this->selfDgramAddress);
        if (socketFd < 0)
        {
            // TODO: Log error
            return -1;
        }

        this->dgramSocketFd = socketFd;
    }

    void deinit()
    {
        if (this->dgramSocketFd > 0)
        {
            if (close(this->dgramSocketFd) < 0)
            {
                // TODO: Log error
                return;
            }
            this->dgramSocketFd = 0;
        }
    }


    int broadcastDgram(char *content, size_t contentSize)
    {
        return sendto(this->dgramSocketFd,
            content, contentSize,
            0,
            (struct sockaddr*)&this->broadcastDgramAddress, sizeof(struct sockaddr_in));
    }
};


enum NodeState
{
    WithoutMaster,
    SubscribingToMaster,
    Master,
    Slave
};

enum MessageType
{
    WhoIsMaster,
    IAmMaster,
    IAmYourMaster,
    IAmYourSlave,
};

typedef int NodeID;

struct SelfNode
{
    NodeID id;
    enum NodeState state;

    struct Networking net;

    void init(struct NetworkingConfig *netConfig)
    {
        memset(&this->net, 0, sizeof(struct Networking));
        this->net.init(netConfig);

        // TODO: this->id = getUniqueNodeId();
        this->state = WithoutMaster;
    }
};



void Node_destroy(struct Node *node)
{
    // TODO: free
}

#define MAX_BUFFER_SIZE 10

struct MessageStruct {
    NodeID selfID;
    NodeID destID;
    enum MessageType message;
};


static char *NodeID_serializeTo(NodeID id, char *buffer)
{
    ((uint32_t*)buffer)[0] = htonl((uint32_t)id);
    buffer += sizeof(uint32_t);
    return buffer;
}

static int MessageStruct_serializeTo(struct MessageStruct *message, char *buffer)
{
    char *pos = buffer;
    pos = NodeID_serializeTo(message->selfID, buffer);
}

static void sendMessage(int socketFd, int ipAddress, short port, struct MessageStruct message, char *content)
{
    char *buffer = (char*)malloc(MAX_BUFFER_SIZE);
    if (!buffer) {
        // TODO: Log error
        return;
    }


    free(buffer);
}

void Node_sendTo(struct Node *node, struct Node *destNode, enum MessageType message)
{

}

void Node_broadcast(struct Node *node, enum MessageType message)
{

}

void Node_handleMessage(struct Node *node, enum MessageType message, NodeID senderNode)
{

}

int main(int argc, char * argv[])
{

}
