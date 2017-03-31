#include "nodes.h"

#include <string.h>

#include "logging.h"

#include <arpa/inet.h>

static void printNodeDescriptor(struct NodeDescriptor *node);
static void printNodeIdentity(struct NodeIdentity *nodeId);

int SelfNode::init(NetworkingConfig *netConfig)
{
    memset(&this->net, 0, sizeof(struct Networking));
    if (this->net.init(netConfig) == -1) {
        logPosition();
        return -1;
    }

    memset(&this->nodeIdentity, 0, sizeof(struct NodeIdentity));

    if (NodeIdentity::getSelfNodeIdentity(&this->nodeIdentity) == -1) {
        logPosition();
        return -1;
    }

    if (this->initTimers() == -1) {
        logPosition();
        return -1;
    }

    this->state = WithoutMaster;
    this->clientsCount = 0;

    memset(this->displayText, 0, DISPLAY_TEXT_MAX_SIZE);
    this->brightness = 0;

    return 0;
}

struct WriteByteStream {
    unsigned char *buffer;
    size_t bufferSize;

    void openStream(unsigned char *buffer, size_t bufferSize)
    {
        this->buffer = buffer;
        this->bufferSize = bufferSize;
    }

    int writeInt32(uint32_t value)
    {
        if (bufferSize < sizeof(uint32_t))
            return -1;
        ((uint32_t*)buffer)[0] = htonl(value);
        buffer += sizeof(uint32_t);
        bufferSize -= sizeof(uint32_t);
        return 0;
    }

    int writeBytes(const unsigned char *bytes, size_t bytesCount)
    {
        if (bufferSize < bytesCount)
            return -1;
        memcpy(buffer, bytes, bytesCount);
        buffer += bytesCount;
        bufferSize -= bytesCount;
        return 0;
    }
};

struct ReadByteStream {
    unsigned char *buffer;
    size_t bufferSize;

    void openStream(unsigned char *buffer, size_t bufferSize)
    {
        this->buffer = buffer;
        this->bufferSize = bufferSize;
    }

    int readInt32(uint32_t *value)
    {
        if (bufferSize < sizeof(uint32_t))
            return -1;
        *value = ntohl(((uint32_t*)buffer)[0]);
        buffer += sizeof(uint32_t);
        bufferSize -= sizeof(uint32_t);
        return 0;
    }

    int readBytes(unsigned char *bytes, size_t bytesCount)
    {
        if (bufferSize < bytesCount)
            return -1;
        memcpy(bytes, buffer, bytesCount);
        buffer += bytesCount;
        bufferSize -= bytesCount;
        return 0;
    }
};

int serializeNodeIdentity(struct WriteByteStream *s, struct NodeIdentity *id)
{
    if (s->writeInt32(id->processId) == -1)
        return -1;

    if (s->writeBytes(id->macAddress, 6) == -1)
        return -1;

    return 0;
}

int serializeMessage(struct WriteByteStream *s, MessageType type, struct NodeIdentity *nodeId)
{
    if (s->writeInt32(type) == -1)
        return -1;

    if (serializeNodeIdentity(s, nodeId) == -1)
        return -1;

    return 0;
}

int deserializeNodeIdentity(struct ReadByteStream *s, struct NodeIdentity *id)
{
    if (s->readInt32((uint32_t*)&id->processId) == -1)
        return -1;

    if (s->readBytes(id->macAddress, 6) == -1)
        return -1;

    return 0;
}

int deserializeMessage(struct ReadByteStream *s,  MessageType *type, struct NodeIdentity *nodeId)
{
    if (s->readInt32((uint32_t*)type) == -1)
        return -1;

    if (deserializeNodeIdentity(s, nodeId) == -1)
        return -1;

    return 0;
}

void SelfNode::recvDgramHandler(struct sockaddr_in* senderAddress, unsigned char *message, size_t messageSize, void *arg)
{
    struct SelfNode *self = (struct SelfNode*)arg;

    struct NodeDescriptor senderNode;
    senderNode.peerAddress = *senderAddress;

    ReadByteStream s;
    s.openStream(message, messageSize);

    MessageType type;
    if (deserializeMessage(&s, &type, &senderNode.id) == -1) {
        logError("Error deserialize message");
        logPosition();
        return;
    }

    if (self->compareWithSelf(&senderNode.id) != 0) {
        switch (type) {
        case ControlResponse: {
            int luminosity;
            int temperature;
            if (s.readInt32((uint32_t*)&luminosity) == -1) {
                logPosition();
                return;
            }
            if (s.readInt32((uint32_t*)&temperature) == -1) {
                logPosition();
                return;
            }
            self->onSensorsInfoReceived(&senderNode, luminosity, temperature);
            break;
        }
        case ControlSet: {
            int brightness;
            if (s.readInt32((uint32_t*)&brightness) == -1) {
                logPosition();
                return;
            }
            self->onDisplayInfoReceived(&senderNode, brightness, (char*)s.buffer, s.bufferSize);
            break;
        }
        default:
            self->onMessageReceived(type, &senderNode);
            break;
        }

    }
}

int SelfNode::run()
{
    logInfo("Running...");
    printNodeIdentity(&this->nodeIdentity);

    this->generateSensorsInfo();

    if (this->sensorsEmulationTimer.start() == -1) {
        logPosition();
        return -1;
    }

    logInfo("\t\tBroadcast WhoIsMaster");
    if (this->broadcastMessage(WhoIsMaster)) {
        logPosition();
        return -1;
    }
    if (this->becomeWithoutMaster() == -1) {
        logPosition();
        return -1;
    }

    logInfo("Run recv loop");
    if (this->net.runRecvLoop(recvDgramHandler, (void*)this) == -1) {
        logPosition();
        return -1;
    }
    logInfo("Exit from recv loop");
    return 0;
}

int SelfNode::becomeWithoutMaster()
{
    logInfo("\033[1;33m\tBecome WithoutMaster\033[0m");
    if (this->stopMasterTimers() == -1) {
        logPosition();
        return -1;
    }

    this->state = WithoutMaster;
    logInfo("\t\tStart WhoIsMaster timer");
    if (this->whoIsMasterTimer.start()) {
        logPosition();
        return -1;
    }
    return 0;
}

int SelfNode::becomeWaitingForMaster()
{
    logInfo("\033[1;33m\tWaiting for master\033[0m");
    if (this->stopMasterTimers() == -1) {
        logPosition();
        return -1;
    }

    this->state = WithoutMaster;

    logInfo("\t\tStщз WhoIsMaster timer");
    if (this->whoIsMasterTimer.stop() == -1) {
        logPosition();
        return -1;
    }
    logInfo("\t\tStart WaitForMaster timer");
    if (this->waitForMasterTimer.start()) {
        logPosition();
        return -1;
    }
    return 0;
}

int SelfNode::becomeMaster()
{
    logInfo("\033[1;33m\tBecome Master\033[0m");
    this->state = Master;
    logInfo("\t\tBroadcast IAmMaster");
    if (this->broadcastMessage(IAmMaster) == -1) {
        logPosition();
        return -1;
    }
    logInfo("\t\tStart IAmAlive heartbeet timer");
    if (this->iAmAliveHeartbeetTimer.start()) {
        logPosition();
        return -1;
    }

    logInfo("\t\tStart ControlRequest timer");
    if (this->controlRequestTimer.start()) {
        logPosition();
        return -1;
    }
    return 0;
}

int SelfNode::becomeSlave(NodeDescriptor *master)
{
    logInfo("\033[1;33m\tBecome Slave\033[0m");
    if (this->stopMasterTimers() == -1) {
        logPosition();
        return -1;
    }

    this->state = Slave;
    this->myMaster = *master;

    if (this->whoIsMasterTimer.stop() == -1) {
        logPosition();
        return -1;
    }
    logInfo("\t\tRestart MonitoringMaster timer");
    if (this->monitoringMasterTimer.start() == -1) {
        logPosition();
        return -1;
    }

    return 0;
}

int SelfNode::stopMasterTimers()
{
    if (this->state == Master) {
        logInfo("\t\tStop IAmAlive heartbeet timer");
        if (this->iAmAliveHeartbeetTimer.stop()) {
            logPosition();
            return -1;
        }
        logInfo("\t\tStop ControlRequest  timer");
        if (this->controlRequestTimer.stop()) {
            logPosition();
            return -1;
        }
    }
    return 0;
}

#define MESSAGE_BUFFER_SIZE 8192
unsigned char sendMessageBuffer[MESSAGE_BUFFER_SIZE];

int SelfNode::sendMessage(MessageType type, struct sockaddr_in *peerAddress)
{

    switch (type) {
    case WhoIsMaster:
        logInfo("\t\tSend WhoIsMaster");
        break;
    case IAmMaster:
        logInfo("\t\tSend IAmMaster");
        break;
    case PleaseWait:
        logInfo("\t\tSend IAmMaster");
        break;
    case ControlRequest:
        logInfo("\t\tSend ControlRequest");
        break;
    }

    WriteByteStream s;
    s.openStream(sendMessageBuffer, MESSAGE_BUFFER_SIZE);
    if (serializeMessage(&s, type, &this->nodeIdentity) == -1) {
        logError("Error serialize message");
        logPosition();
        return -1;
    }

    size_t size = (size_t)(s.buffer - sendMessageBuffer);

    if (this->net.sendDgram(peerAddress, sendMessageBuffer, size) == -1) {
        logPosition();
        return -1;
    }

    return 0;
}

int SelfNode::broadcastMessage(MessageType type)
{
    switch (type) {
    case WhoIsMaster:
        logInfo("\t\tBroadcast WhoIsMaster");
        break;
    case IAmMaster:
        logInfo("\t\tBroadcast IAmMaster");
        break;
    case PleaseWait:
        logInfo("\t\tBroadcast IAmMaster");
        break;
    case ControlRequest:
        logInfo("\t\tBroadcast ControlRequest");
        break;
    }

    WriteByteStream s;
    s.openStream(sendMessageBuffer, MESSAGE_BUFFER_SIZE);
    serializeMessage(&s, type, &this->nodeIdentity);

    size_t size = (size_t)(s.buffer - sendMessageBuffer);

    if (this->net.broadcastDgram(sendMessageBuffer, size) == -1) {
        logPosition();
        return -1;
    }

    return 0;
}

int SelfNode::sendMessageWithSensorInfo(sockaddr_in *peerAddress, int luminosity, int temperature)
{
    logInfo("\t\tSend ControlResponse");

    WriteByteStream s;
    s.openStream(sendMessageBuffer, MESSAGE_BUFFER_SIZE);
    if (serializeMessage(&s, ControlResponse, &this->nodeIdentity) == -1) {
        logError("Error serialize message");
        logPosition();
        return -1;
    }

    if (s.writeInt32(luminosity) == -1)
        return -1;

    if (s.writeInt32(temperature) == -1)
        return -1;

    size_t size = (size_t)(s.buffer - sendMessageBuffer);

    if (this->net.sendDgram(peerAddress, sendMessageBuffer, size) == -1) {
        logPosition();
        return -1;
    }

    return 0;
}

int SelfNode::sendMessageWithDisplayInfo(int brightness, char *displayText, size_t textLength)
{
    logInfo("\t\tSend ControlSet");

    WriteByteStream s;
    s.openStream(sendMessageBuffer, MESSAGE_BUFFER_SIZE);
    if (serializeMessage(&s, ControlSet, &this->nodeIdentity) == -1) {
        logError("Error serialize message");
        logPosition();
        return -1;
    }

    if (s.writeInt32(brightness) == -1)
        return -1;

    if (s.writeBytes((unsigned char*)displayText, textLength) == -1)
        return -1;

    size_t size = (size_t)(s.buffer - sendMessageBuffer);

    if (this->net.broadcastDgram(sendMessageBuffer, size) == -1) {
        logPosition();
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

void logMessageReceived(MessageType type)
{
    switch (type) {
    case WhoIsMaster:
        logInfo("WhoIsMaster received");
        break;
    case IAmMaster:
        logInfo("IAmMaster received");
        break;
    case PleaseWait:
        logInfo("IAmMaster received");
        break;
    case ControlRequest:
        logInfo("ControlRequest received");
        break;
    }
}

void logState(NodeState state)
{
    switch (state) {
    case WithoutMaster:
        logInfo(" when I am WithoutMaster");
        break;
    case Slave:
        logInfo(" when I am Slave");
        break;
    case Master:
        logInfo(" when I am Master");
        break;
    default:
        break;
    }
}

void SelfNode::onMessageReceived(MessageType type, struct NodeDescriptor *sender)
{
    struct NodeIdentity *senderId = &sender->id;
    struct sockaddr_in *senderAddress = &sender->peerAddress;

    logInfo("Message received from ");
    printNodeDescriptor(sender);

    logMessageReceived(type);
    logState(this->state);

    switch (type) {
    case WhoIsMaster:
        if (this->state == Master) {
            if (this->compareWithSelf(senderId) < 0) {
                if (this->broadcastMessage(IAmMaster) == -1) {
                    logPosition();
                    return;
                }
            }
            else {
                logInfo("\t\tBroadcast WhoIsMaster");
                if (this->broadcastMessage(WhoIsMaster)) {
                    logPosition();
                    return;
                }
                if (this->becomeWithoutMaster() == -1) {
                    logPosition();
                    return;
                }
            }
        }
        else {
            if (this->compareWithSelf(senderId) < 0) {
                if (this->sendMessage(PleaseWait, senderAddress) == -1) {
                    logPosition();
                    return;
                }
            }
            break;
        }
    case IAmMaster:
        if (this->compareWithSelf(senderId) < 0) {
            if (this->state == Master) {
                if (this->broadcastMessage(IAmMaster)) {
                    logPosition();
                    return;
                }
            }
            else {
                if (this->sendMessage(PleaseWait, senderAddress) == -1) {
                    logPosition();
                    return;
                }
            }
        }
        else {
            if (this->becomeSlave(sender) == -1) {
                logPosition();
                return;
            }
        }
        break;
    case PleaseWait:
        if (this->state == Slave) {
            if (this->compareWithCurrentMaster(senderId) > 0) {
                if (this->becomeWaitingForMaster() == -1) {
                    logPosition();
                    return;
                }
            }
        }
        else {
            if (this->compareWithSelf(senderId) > 0) {
                if (this->becomeWaitingForMaster() == -1) {
                    logPosition();
                    return;
                }
            }
        }
        break;

    case ControlRequest:
        if (this->state == Slave) {
            if (this->sendMessageWithSensorInfo(&sender->peerAddress, this->luminosity, this->temperature) == -1) {
                logPosition();
                return;
            }
        }
        break;
    }
}

void SelfNode::onSensorsInfoReceived(NodeDescriptor *sender, int luminosity, int temperature)
{
    if (this->clientsCount == CLIENTS_MAX_COUNT - 1) {
        logPosition();
        return;
    }

    SensorInfo *sensorInfo = &this->receivedSensorsInfo[this->clientsCount];
    sensorInfo->luminosity = luminosity;
    sensorInfo->temperature = temperature;
    ++this->clientsCount;
}

void SelfNode::onDisplayInfoReceived(NodeDescriptor *sender, int brightness, char *displayText, size_t textLength)
{
    logInfo("Display info received");
    this->brightness = brightness;
    strncpy(this->displayText, displayText, textLength);
    this->displayInfo();
}

void SelfNode::onWhoIsMasterTimeout()
{
    logInfo("\033[0;32mWhoIsMaster timeout\033[0m");
    if (this->becomeMaster() == -1) {
        logPosition();
    }
}

void SelfNode::onWaitForMasterTimeout()
{
    logInfo("\033[0;32mWaitForMaster timeout\033[0m");
    if (this->becomeWithoutMaster() == -1) {
        logPosition();
    }
}

void SelfNode::onMonitoringMasterTimeout()
{
    logInfo("\033[0;32mMonitoringMaster timeout\033[0m");
    if (this->becomeWithoutMaster() == -1) {
        logPosition();
    }
}

void SelfNode::onIAmAliveHeartbeetTimeout()
{
    logInfo("\033[0;32mMasterAlive timeout\033[0m");
    if (this->state == Master) {
        logInfo("\t\tBroadcast IAmMaster");
        this->broadcastMessage(IAmMaster);
    }
    else {
        logError("Error: Master Heartbeet timer is not stopped!!");
    }
}

void SelfNode::onControlRequestTimeoutHandler()
{
    logInfo("\033[0;32mControlRequest timeout\033[0m");

    this->clientsCount = 0;

    if (this->broadcastMessage(ControlRequest) == -1) {
        logPosition();
        return;
    }

    if (this->controlWaitResponceTimer.start() == -1) {
        logPosition();
        return;
    }
}

void SelfNode::onControlWaitResponceTimeoutHandler()
{
    logInfo("\033[0;32mControlWaitResponce timeout\033[0m");

    if (this->clientsCount == 0) {
        logInfo("\tReceived sensors info is empty");
    }

    int meanTemperature = this->temperature;
    int meanLuminosity = this->luminosity;

    for(int i = 0; i < this->clientsCount; ++i) {
        meanLuminosity += this->receivedSensorsInfo[i].luminosity;
        meanTemperature += this->receivedSensorsInfo[i].temperature;
    }
    meanLuminosity /= this->clientsCount + 1;
    meanTemperature /= this->clientsCount + 1;

    sprintf(this->displayText, "Temperature: %d", temperature);

    this->brightness = meanLuminosity * 4 + 1000; // for example

    logInfo("\t\tSend display info");
    this->displayInfo();
    if (this->sendMessageWithDisplayInfo(this->brightness, this->displayText, strlen(this->displayText)) == -1) {
        logPosition();
        return;
    }

    this->clientsCount = 0;
}

void SelfNode::onSensorsEmulationTimeout()
{
    logInfo("Sensors values has been changed");
    this->generateSensorsInfo();
}

void SelfNode::generateSensorsInfo()
{
    this->luminosity = rand() % 100 + 1000;
    printf("\033[0;33mluminosity = %d\n\033[0m", this->luminosity);

    this->temperature = rand() % 20 + 10;
    printf("\033[0;33mtemperature = %d\n\033[0m", this->temperature);
}

void SelfNode::displayInfo()
{
    printf("\033[1;37m\nBrightness: %d, Text: %s\n\n\033[0m", this->brightness, this->displayText);
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

void SelfNode::waitForMasterTimeoutHandler(TimerHandlerArgument arg)
{
    if (arg.ptrValue)
        ((SelfNode*)arg.ptrValue)->onWaitForMasterTimeout();
}

void SelfNode::iAmAliveHeartbeetTimeoutHandler(TimerHandlerArgument arg)
{
    if (arg.ptrValue)
        ((SelfNode*)arg.ptrValue)->onIAmAliveHeartbeetTimeout();
}

void SelfNode::controlRequestTimeoutHandler(TimerHandlerArgument arg)
{
    if (arg.ptrValue)
        ((SelfNode*)arg.ptrValue)->onControlRequestTimeoutHandler();
}

void SelfNode::controlWaitResponceTimeoutHandler(TimerHandlerArgument arg)
{
    if (arg.ptrValue)
        ((SelfNode*)arg.ptrValue)->onControlWaitResponceTimeoutHandler();
}

void SelfNode::sensorsEmulationTimeoutHandler(TimerHandlerArgument arg)
{
    if (arg.ptrValue)
        ((SelfNode*)arg.ptrValue)->onSensorsEmulationTimeout();
}

int SelfNode::initTimers()
{
    if (Timer::initTimerSystem() == -1) {
        logPosition();
        return -1;
    }

    TimerHandlerArgument arg;
    arg.ptrValue = (void*)this;
    if (this->whoIsMasterTimer.init(5000, false, SelfNode::whoIsMasterTimeoutHandler, arg) == -1) {
        logPosition();
        return -1;
    }
    if (this->monitoringMasterTimer.init(16000, false, SelfNode::monitoringMasterTimeoutHandler, arg) == -1) {
        logPosition();
        return -1;
    }
    if (this->waitForMasterTimer.init(10000, false, SelfNode::waitForMasterTimeoutHandler, arg) == -1) {
        logPosition();
        return -1;
    }
    if (this->iAmAliveHeartbeetTimer.init(10000, true, SelfNode::iAmAliveHeartbeetTimeoutHandler, arg) == -1) {
        logPosition();
        return -1;
    }

    if (this->controlRequestTimer.init(20000, true, SelfNode::controlRequestTimeoutHandler, arg) == -1) {
        logPosition();
        return -1;
    }
    if (this->controlWaitResponceTimer.init(3000, false, SelfNode::controlWaitResponceTimeoutHandler, arg) == -1) {
        logPosition();
        return -1;
    }

    if (this->sensorsEmulationTimer.init(30000, true, SelfNode::sensorsEmulationTimeoutHandler, arg) == -1) {
        logPosition();
        return -1;
    }
    return 0;
}

static void printNodeDescriptor(struct NodeDescriptor *node)
{
    struct NodeIdentity *nodeId = &node->id;
    struct sockaddr_in *senderAddress = &node->peerAddress;
    unsigned char *mac = nodeId->macAddress;

    char ipString[INET_ADDRSTRLEN];
    memset(ipString, 0, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &senderAddress->sin_addr, ipString, INET_ADDRSTRLEN);
    printf("%s, %d, %02x:%02x:%02x:%02x:%02x:%02x\n", ipString, nodeId->processId,
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void printNodeIdentity(struct NodeIdentity *nodeId)
{
    unsigned char *mac = nodeId->macAddress;
    printf("%d, %02x:%02x:%02x:%02x:%02x:%02x\n", nodeId->processId,
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
