#include "nodes.h"

#include <string.h>

#include "logging.h"

#include <arpa/inet.h>

int SelfNode::init(NetworkingConfig *netConfig)
{
    memset(&this->net, 0, sizeof(struct Networking));
    if (this->net.init(netConfig) == -1) {
        logError();
        return -1;
    }

    memset(&this->nodeIdentity, 0, sizeof(struct NodeIdentity));

    if (NodeIdentity::getSelfNodeIdentity(&this->nodeIdentity) == -1) {
        logError();
        return -1;
    }

    if (this->initTimers() == -1) {
        logError();
        return -1;
    }

    this->state = WithoutMaster;

    return 0;
}

size_t deserializeNodeIdentity(char *buffer, struct NodeIdentity *id)
{
    size_t bytesRead = 0;
    id->processId = ntohl(((uint32_t*)buffer)[0]);
    bytesRead += sizeof(uint32_t);
    buffer += bytesRead;
    memcpy(id->macAddress, buffer, 6);
    bytesRead += 6;
    return bytesRead;
}

size_t serializeNodeIdentity(struct NodeIdentity *id, char *buffer)
{
    size_t bytesWritten = 0;

    ((uint32_t*)buffer)[0] = htonl(id->processId);

    bytesWritten += sizeof(uint32_t);
    buffer += bytesWritten;

    memcpy(buffer, id->macAddress, 6);

    bytesWritten += 6;

    return bytesWritten;
}

size_t serializeMessage(MessageType type, struct NodeIdentity *nodeId, char *buffer)
{
    char *bufferBegin = buffer;

    ((uint32_t*)buffer)[0] = htonl(type);
    buffer += sizeof(uint32_t);

    size_t size = serializeNodeIdentity(nodeId, buffer);
    buffer += size;

    return (size_t)(buffer - bufferBegin);
}

size_t deserializeMessage(char *buffer, MessageType *type, struct NodeIdentity *nodeId)
{
    char *bufferBegin = buffer;
    *type = (MessageType)ntohl(((uint32_t*)buffer)[0]);
    buffer += sizeof(uint32_t);

    size_t size = deserializeNodeIdentity(buffer, nodeId);
    buffer += size;

    return (size_t)(buffer - bufferBegin);
}

void SelfNode::recvDgramHandler(struct sockaddr_in* senderAddress, char *message, size_t messageSize, void *arg)
{
    struct SelfNode *self = (struct SelfNode*)arg;

    struct NodeDescriptor senderNode;
    senderNode.peerAddress = *senderAddress;

    MessageType type;
    deserializeMessage(message, &type, &senderNode.id);

    if (self->compareWithSelf(&senderNode.id) != 0) {
        self->onMessageReceived(type, &senderNode);
    }
}

int SelfNode::run()
{
    printf("Running...\n");
    if (this->becameWithoutMaster() == -1) {
        logError();
        return -1;
    }
    printf("Run recv loop\n");
    if (this->net.runRecvLoop(recvDgramHandler, (void*)this) == -1) {
        logError();
        return -1;
    }
    printf("Exit from recv loop\n");
    return 0;
}

int SelfNode::becameWithoutMaster()
{
    printf("Become WithoutMaster\n");
    this->state = WithoutMaster;
    printf("Broadcast WhoIsMaster\n");
    if (this->broadcastMessage(WhoIsMaster)) {
        logError();
        return -1;
    }
    printf("Start WhoIsMaster timer\n");
    if (this->whoIsMasterTimer.start()) {
        logError();
        return -1;
    }
    return 0;
}

#define MESSAGE_BUFFER_SIZE 8192
char sendMessageBuffer[MESSAGE_BUFFER_SIZE];

int SelfNode::sendMessage(MessageType type, struct sockaddr_in *peerAddress)
{
    size_t size = serializeMessage(type, &this->nodeIdentity, sendMessageBuffer);

    if (this->net.sendDgram(peerAddress, sendMessageBuffer, size) == -1) {
        logError();
        return -1;
    }

    return 0;
}

int SelfNode::broadcastMessage(MessageType type)
{
    size_t size = serializeMessage(type, &this->nodeIdentity, sendMessageBuffer);

    if (this->net.broadcastDgram(sendMessageBuffer, size) == -1) {
        logError();
        return -1;
    }

    return 0;
}

int SelfNode::compareWithSelf(NodeIdentity *senderId)
{
    return NodeIdentity::compareNodeIdentities(senderId, &this->nodeIdentity);
}

int SelfNode::compareWithCurrentMaster(NodeIdentity *senderId)
{
    return NodeIdentity::compareNodeIdentities(senderId, &this->myMaster.id);
}

void SelfNode::onMessageReceived(MessageType type, struct NodeDescriptor *sender)
{
    struct NodeIdentity *senderId = &sender->id;
    struct sockaddr_in *senderAddress = &sender->peerAddress;
    unsigned char *mac = senderId->macAddress;

    char ipString[INET_ADDRSTRLEN];
    memset(ipString, 0, INET_ADDRSTRLEN);

    inet_ntop(AF_INET, &senderAddress->sin_addr, ipString, INET_ADDRSTRLEN);
    printf("Message received from %s, %d, %02x:%02x:%02x:%02x:%02x:%02x\n", ipString, sender->id.processId,
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    switch (type) {
    case WhoIsMaster:
        printf("WhoIsMaster received\n");
        if (this->state == Master) {
            printf(" when I am Master\n");
            int cmp = this->compareWithSelf(senderId);
            if (cmp < 0) {
                printf("Broadcast IAmMaster\n");
                if (this->broadcastMessage(IAmMaster) == -1) {
                    logError();
                    return;
                }
            }
            else if (cmp > 0) {
                this->becameWithoutMaster();
            }
        }
        else if (this->state == Slave) {
            printf(" when I am Slave\n");
            if (this->compareWithSelf(senderId) < 0) {
                printf("Send PleaseWait\n");
                if (this->sendMessage(PleaseWait, senderAddress) == -1) {
                    logError();
                    return;
                }
            }
        }
        else if (this->state == WithoutMaster) {
            printf(" when I am WithoutMaster\n");
            if (this->compareWithSelf(senderId) < 0) {
                printf("Send PleaseWait\n");
                if (this->sendMessage(PleaseWait, senderAddress) == -1) {
                    logError();
                    return;
                }
            }
        }
        break;
    case IAmMaster:
        printf("IAmMaster received\n");
        if (this->state == WithoutMaster) {
            printf(" when I am WithoutMaster\n");
            printf("Become Slave\n");
            this->state = Slave;
            this->myMaster = *sender;
            printf("Restart MonitoringMaster timer\n");
            if (this->monitoringMasterTimer.start() == -1) {
                logError();
                return;
            }
        }
        else if (this->state == Slave) {
            printf(" when I am Slave\n");
            this->myMaster = *sender;
            printf("Restart MonitoringMaster timer\n");
            if (this->monitoringMasterTimer.start() == -1) {
                logError();
                return;
            }
        }
        else if (this->state == Master) {
            printf(" when I am Master\n");
            int cmp = this->compareWithSelf(senderId);
            if (cmp < 0) {
                printf("Broadcast IAmMaster\n");
                if (this->broadcastMessage(IAmMaster)) {
                    logError();
                    return;
                }
            }
            else if (cmp > 0) {
                printf("Become Slave\n");
                this->state = Slave;
                this->myMaster = *sender;
                printf("Restart MonitoringMaster timer\n");
                if (this->monitoringMasterTimer.start() == -1) {
                    logError();
                    return;
                }
            }
        }
        break;
    case PleaseWait:
        printf("PleaseWait received\n");
        if (this->state == WithoutMaster) {
            printf(" when I am WithoutMaster\n");
            int cmp = this->compareWithSelf(senderId);
            if (cmp > 0) {
                printf("Restart WhoIsMaster timer\n");
                if (this->whoIsMasterTimer.start() == -1) {
                    logError();
                    return;
                }
            }
        }
        break;
    default:
        break;
    }
}

void SelfNode::onWhoIsMasterTimeout()
{
    printf("WhoIsMaster timeout");
    printf("Become Master\n");
    this->state = Master;
    printf("Broadcast IAmMaster\n");
    if (this->broadcastMessage(IAmMaster) == -1) {
        logError();
        return;
    }
}

void SelfNode::onMonitoringMasterTimeout()
{
    printf("MonitoringMaster timeout");
    this->becameWithoutMaster();
}

void SelfNode::whoIsMasterTimeoutHandler(TimerHandlerArgument arg)
{
    if (arg.ptrValue)
        ((SelfNode*)arg.ptrValue)->onWhoIsMasterTimeout();
}

void SelfNode::monitoringMasterTimeoutHandler(TimerHandlerArgument arg)
{
    if (arg.ptrValue)
        ((SelfNode*)arg.ptrValue)->onMonitoringMasterTimeout();
}

int SelfNode::initTimers()
{
    if (Timer::initTimerSystem() == -1) {
        logError();
        return -1;
    }

    TimerHandlerArgument arg;
    arg.ptrValue = (void*)this;
    if (this->whoIsMasterTimer.init(10000, false, SelfNode::whoIsMasterTimeoutHandler, arg) == -1) {
        logError();
        return -1;
    }
    if (this->monitoringMasterTimer.init(50000, false, SelfNode::monitoringMasterTimeoutHandler, arg) == -1) {
        logError();
        return -1;
    }
    return 0;
}

