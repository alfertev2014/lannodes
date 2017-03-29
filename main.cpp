#include <stdio.h>

#include <stdlib.h>
#include <time.h>

#include "nodes.h"
#include "logging.h"

int main(int argc, char * argv[])
{
    srand(time(NULL));

    struct NetworkingConfig config;
    config.udpPort = 10500;

    SelfNode node;
    if (node.init(&config) == -1) {
        logPosition();
        return -1;
    }

    node.run();

    return 0;
}
