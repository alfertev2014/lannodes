#include "nodes.h"

#include <string.h>

int SelfNode::init(NetworkingConfig *netConfig)
{
    memset(&this->net, 0, sizeof(struct Networking));
    if (this->net.init(netConfig) == -1) {
        // TODO: Log error
        return -1;
    }

    memset(&this->nodeIdentity, 0, sizeof(struct NodeIdentity));

    if (NodeIdentity::getSelfNodeIdentity(&this->nodeIdentity) == -1) {
        // TODO: Log error
        return -1;
    }

    if (this->initTimers() == -1) {
        // TODO: Log error
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

void SelfNode::run()
{
    this->becameWithoutMaster();
    this->net.runRecvLoop(recvDgramHandler, (void*)this);
}

void SelfNode::becameWithoutMaster()
{
    this->state = WithoutMaster;
    if (this->broadcastMessage(WhoIsMaster)) {
        // TODO: Log error
        return;
    }
    if (this->whoIsMasterTimer.start()) {
        // TODO: Log error
        return;
    }
}

#define MESSAGE_BUFFER_SIZE 8192
char sendMessageBuffer[MESSAGE_BUFFER_SIZE];

int SelfNode::sendMessage(MessageType type, struct sockaddr_in *peerAddress)
{
    size_t size = serializeMessage(type, &this->nodeIdentity, sendMessageBuffer);

    if (this->net.sendDgram(peerAddress, sendMessageBuffer, size) == -1) {
        // TODO: Log error
        return -1;
    }

    return 0;
}

int SelfNode::broadcastMessage(MessageType type)
{
    size_t size = serializeMessage(type, &this->nodeIdentity, sendMessageBuffer);

    if (this->net.broadcastDgram(sendMessageBuffer, size) == -1) {
        // TODO: Log error
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

    switch (type) {
    case WhoIsMaster:
        if (this->state == Master) {
            int cmp = this->compareWithSelf(senderId);
            if (cmp < 0) {
                if (this->broadcastMessage(IAmMaster) == -1) {
                    // TODO: Log error
                    return;
                }
            }
            else if (cmp > 0) {
                this->becameWithoutMaster();
            }
        }
        else if (this->state == Slave) {
            if (this->compareWithSelf(senderId) < 0) {
                if (this->sendMessage(PleaseWait, senderAddress) == -1) {
                    // TODO: Log error
                    return;
                }
            }
        }
        else if (this->state == WithoutMaster) {
            if (this->compareWithSelf(senderId) < 0) {
                if (this->sendMessage(PleaseWait, senderAddress) == -1) {
                    // TODO: Log error
                    return;
                }
            }
        }
        break;
    case IAmMaster:
        if (this->state == WithoutMaster) {
            this->state = Slave;
            this->myMaster = *sender;
            if (this->monitoringMasterTimer.start() == -1) {
                // TODO: Log error
                return;
            }
        }
        else if (this->state == Slave) {
            this->myMaster = *sender;
            if (this->monitoringMasterTimer.start() == -1) {
                // TODO: Log error
                return;
            }
        }
        else if (this->state == Master) {
            int cmp = this->compareWithSelf(senderId);
            if (cmp < 0) {
                if (this->broadcastMessage(IAmMaster)) {
                    // TODO: Log error
                    return;
                }
            }
            else if (cmp > 0) {
                this->state = Slave;
                this->myMaster = *sender;
                if (this->monitoringMasterTimer.start() == -1) {
                    // TODO: Log error
                    return;
                }
            }
        }
        break;
    case PleaseWait:
        if (this->state == WithoutMaster) {
            int cmp = this->compareWithSelf(senderId);
            if (cmp > 0) {
                if (this->whoIsMasterTimer.start() == -1) {
                    // TODO: Log error
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
    this->state = Master;
    if (this->broadcastMessage(IAmMaster) == -1) {
        // TODO: Log error
        return;
    }
}

void SelfNode::onMonitoringMasterTimeout()
{
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
    TimerHandlerArgument arg;
    arg.ptrValue = (void*)this;
    if (this->whoIsMasterTimer.init(1000, false, SelfNode::whoIsMasterTimeoutHandler, arg) == -1) {
        // TODO: Log error
        return -1;
    }
    if (this->monitoringMasterTimer.init(5000, false, SelfNode::monitoringMasterTimeoutHandler, arg) == -1) {
        // TODO: Log error
        return -1;
    }
    return 0;
}

