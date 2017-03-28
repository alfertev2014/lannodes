#include "identity.h"

//getifaddrs
#include <ifaddrs.h>

#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>

static int getMACAddress(int fd, char *macAddressBuffer)
{
    struct ifaddrs * ifAddrStruct = NULL;

    if (getifaddrs(&ifAddrStruct) == -1) {
        // TODO: Log error
        return -1;
    }

    bool success = false;
    for (struct ifaddrs *ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr)
            continue;
        if (ifa->ifa_addr->sa_family != AF_INET ||
            ifa->ifa_addr->sa_family != AF_INET6) {
            continue;
        }
        struct ifreq ifr;

        ifr.ifr_addr.sa_family = AF_INET;
        strncpy(ifr.ifr_name , ifa->ifa_name , IFNAMSIZ - 1);

        if (ioctl(fd, SIOCGIFHWADDR, &ifr) == -1) {
            // TODO: Log error
            continue;
        }

        if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
            continue;
        }

        memcpy(macAddressBuffer, (unsigned char *)ifr.ifr_hwaddr.sa_data, 6);
        success = true;
        break;
    }
    if (ifAddrStruct != NULL)
        freeifaddrs(ifAddrStruct);

    return success ? 0 : -1;
}

int NodeIdentity::getSelfNodeIdentity(NodeIdentity *id)
{
    int socketFd = socket(AF_INET, SOCK_DGRAM, 0);

    if (socketFd < 0) {
        // TODO: Log error
        return -1;
    }

    if (getMACAddress(socketFd, id->macAddress) == -1) {
        // TODO: Log error
        return -1;
    }

    id->processId = getpid();

    if (close(socketFd) == -1) {
        // TODO: Log error
        return -1;
    }

    return 0;
}

int NodeIdentity::compareNodeIdentities(struct NodeIdentity *self, struct NodeIdentity *other)
{
    int maccmp = memcmp(self->macAddress, other->macAddress, 6);
    if (maccmp > 0)
        return 1;
    else if (maccmp < 0)
        return -1;

    if (self->processId > other->processId)
        return 1;
    else if (self->processId < other->processId)
        return -1;

    return 0;
}
