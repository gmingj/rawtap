
#include "logger.h"
#include "config.h"
#include "net.h"
#include "control.h"

Logger logger;
Config config;
Net net;
Control control;

void cleanup(void)
{
    pthread_cancel(control.ctrltid_);
    close(net.rawsock_);
    close(control.ctrlsock_);

    return;
}

int main(int argc, char *argv[])
{
    config.ParseOpts(argc, argv);

    PLOGD << "dev " << config.dev_;

    net.NetInit(config.dev_, "");

    control.CreateThread();

    if (atexit(cleanup) < 0) {
        PLOGF <<  "failed to register exit hook";
        exit(EXIT_FAILURE);
    }

    net.ProcessFrer();

    return EXIT_SUCCESS;
}

