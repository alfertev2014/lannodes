#include "networking.h"

static int bindDgramSocket(sockaddr_in *addr)
{
    int socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (socketFd < 0)
    {
        // TODO: Log error
        return -1;
    }

    int broadcastSocketOption = 1;
    int err = setsockopt(socketFd,
                         SOL_SOCKET, SO_BROADCAST,
                         &broadcastSocketOption, sizeof(broadcastSocketOption));
    if (err < 0)
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

static void initSockedAddress(sockaddr_in *addr, uint32_t ipAddress, uint16_t port)
{
    addr->sin_family = AF_INET;
    addr->sin_port = htonl(port);
    addr->sin_addr.s_addr = htonl(ipAddress);
}

int Networking::init(NetworkingConfig *config) {
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

void Networking::deinit()
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

int Networking::broadcastDgram(char *content, size_t contentSize)
{
    return sendto(this->dgramSocketFd,
                  content, contentSize,
                  0,
                  (struct sockaddr*)&this->broadcastDgramAddress, sizeof(struct sockaddr_in));
}
