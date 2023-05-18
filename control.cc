
#include "control.h"
#include "logger.h"
#include "net.h"

#include <iostream>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <linux/if_tun.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <pthread.h>

#include <linux/if_tun.h>
#include <linux/un.h>


#include <errno.h>

#define CONTROL_MSG_BUF_LEN 2048
#define CONTROL_CMD_BUF_LEN	256
#define FRER_UNIX_DOMAIN "/var/run/frer"
#define FRER_TAPNAME "frer"
#define COMMAND_TYPE_INVALID        0
#define COMMAND_TYPE_CREATE         1
#define COMMAND_TYPE_DESTROY        2
#define CONTROL_ERRMSG "invalid command arguments\n"

void exec_command_invalid(char *str, int sock);
void exec_command_create(char *str, int sock);
void exec_command_destroy(char *str, int sock);

extern Control control;

void (*exec_command_func[])(char *str, int ctrlsock) = {
    exec_command_invalid,
    exec_command_create,
    exec_command_destroy
};

int destroy_frer_instance(char *tapname)
{
    if (control.instancenum_ < 1) {
        return -1;
    }

    if (pthread_cancel(control.taptid_) != 0) {
        PLOGW << control.tapname_ << ": can not stop frer instance " << strerror(errno);
    }

    if (close(control.tapsock_) < 0) {
        PLOGW << control.tapname_ << ": can not close frer socket" << strerror(errno);
    }

    control.instancenum_--;
    return 0;
}

int create_frer_instance(char *tapname)
{
    int fd, udpfd;
    struct ifreq ifr;

    if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
        PLOGF << "cannot create a control cahnnel of the tun intface.";
        exit(EXIT_FAILURE);
    }

    memset(&ifr, 0, sizeof (ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    strncpy(ifr.ifr_name, tapname, IFNAMSIZ);

    if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) {
        close(fd);
        PLOGF << "cannot create " << tapname << " interface.";
        exit(EXIT_FAILURE);
    }

	if ((udpfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        close(fd);
        PLOGF << "failt to create control socket of tap interface.";
        exit(EXIT_FAILURE);
    }

	memset(&ifr, 0, sizeof (ifr));
    ifr.ifr_flags = IFF_UP;
	strncpy(ifr.ifr_name, tapname, IFNAMSIZ);
    if (ioctl(udpfd, SIOCSIFFLAGS, (void *)&ifr) < 0) {
        close(udpfd);
        close(fd);
        PLOGF << "failed to make " << tapname << " up.";
        exit(EXIT_FAILURE);
    }

    close(udpfd);
    return fd;
}

void exec_command_invalid(char *str, int ctrlsock)
{
    write(ctrlsock, CONTROL_ERRMSG, sizeof(CONTROL_ERRMSG));
}

void exec_command_create(char *str, int ctrlsock)
{
    char cmd[CONTROL_CMD_BUF_LEN];
    char tapname[IFNAMSIZ];

    if (sscanf (str, "%s %d", cmd, &control.instanceid_) < 2) {
        write(ctrlsock, CONTROL_ERRMSG, sizeof(CONTROL_ERRMSG));
        return;
    }

    snprintf(tapname, IFNAMSIZ, "%s%X", FRER_TAPNAME, control.instanceid_);
    control.tapsock_ = create_frer_instance(tapname);
    control.CreateTapInsThread();
    control.instancenum_++;

    char msgbuf[] = "created\n";
    write(ctrlsock, msgbuf, strlen (msgbuf));
    return;
}

void exec_command_destroy(char *str, int ctrlsock)
{
    char cmd[CONTROL_CMD_BUF_LEN];
    char tapname[IFNAMSIZ];

    if (sscanf(str, "%s %d", cmd, &control.instanceid_) < 2) {
        write (ctrlsock, CONTROL_ERRMSG, sizeof (CONTROL_ERRMSG));
        return;
    }

    snprintf(tapname, IFNAMSIZ, "%s%X", FRER_TAPNAME, control.instanceid_);
    if (destroy_frer_instance(tapname) < 0) {
        char errbuf[] = "Delete frer instance failed\n";
        write(ctrlsock, errbuf, strlen(errbuf));
    }
    else {
        char errbuf[] = "Deleted\n";
        write(ctrlsock, errbuf, strlen(errbuf));
    }

    return;	
}

int strtocmdtype(char *str)
{
    if (strncmp(str, "create", 6) == 0) 
        return COMMAND_TYPE_CREATE;

    if (strncmp(str, "destroy", 7) == 0)
        return COMMAND_TYPE_DESTROY;		

    return COMMAND_TYPE_INVALID;
}

void *ProcessControl(void *param)
{
    char *c;
    int ctrlsock, accept_socket;
    char buf[CONTROL_MSG_BUF_LEN];

    ctrlsock = control.CreateCtrlSock(FRER_UNIX_DOMAIN);
    control.ctrlsock_ = ctrlsock;
    PLOGI << "succeed to create control socket " << control.ctrlsock_;

    listen(ctrlsock, 1);

    while (1) {
        memset(buf, 0, sizeof(buf));
        accept_socket = accept(ctrlsock, NULL, 0);
        PLOGD << "control accept socket " << accept_socket;
        if (read(accept_socket, buf, sizeof(buf)) < 0) {
            PLOGW << "read(2) faild: control accept socket";
            shutdown(accept_socket, 1);
            close(accept_socket);
            continue;
        }

        for (c = buf; *c == ' '; c++);
        exec_command_func[strtocmdtype(c)](c, accept_socket);

        if (shutdown(accept_socket, SHUT_RDWR) != 0) {
            PLOGW << "shutdown: ", strerror(errno);
        }

        if (close(accept_socket) != 0) {
            PLOGW << "close: ", strerror(errno);
        }
    }

    shutdown(control.ctrlsock_, SHUT_RDWR);
    if (close(control.ctrlsock_) < 0) {
        PLOGW << "close control socket failed: ", strerror(errno);
    }

    /* not reached */
    return NULL;
}
void TxFromLocalToFrer(struct ether_header *ether, int len)
{
    struct FrerHeader fhdr;
    struct msghdr mhdr;
    struct iovec iov[2];

    memset(&fhdr, 0, sizeof (fhdr));
    memcpy(&fhdr.eh, ether, sizeof(struct ether_header));
    fhdr.eh.ether_type = 0XF1C1;

    iov[0].iov_base = &fhdr;
    iov[0].iov_len  = sizeof(fhdr);
    iov[1].iov_base = ether;
    iov[1].iov_len  = len;

    mhdr.msg_name = ether->ether_dhost;
    mhdr.msg_namelen = ETH_ALEN;
    mhdr.msg_iov = iov;
    mhdr.msg_iovlen = 2;
    mhdr.msg_control = 0;
    mhdr.msg_controllen = 0;

    if (sendmsg(control.tapsock_, &mhdr, 0) < 0) {
        PLOGW << "sendmsg failed: " << strerror(errno);
    }

    return;
}

void *ProcessFrerInstance(void * param)
{
    int len;
    char buf[CONTROL_MSG_BUF_LEN];

    while (1) {
        if ((len = read(control.tapsock_, buf, sizeof (buf))) < 0) {
            PLOGW << "read from tap socket failed " << strerror(errno); 
            continue;
        }
        PLOGD << "read tapsock " << len << " bytes";

        TxFromLocalToFrer((struct ether_header * )buf, len);
    }

    /* not reached */
    return NULL;
}

void Control::CreateTapInsThread(void)
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&control.taptid_, &attr, ProcessFrerInstance, NULL);
}

void Control::CreateThread(void)
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&ctrltid_, &attr, ProcessControl, NULL);
}

int Control::CreateCtrlSock(std::string domain)
{
    int ctrlsock;
    struct sockaddr_un saddru;

    memset(&saddru, 0, sizeof(saddru));
    saddru.sun_family = AF_UNIX;
    strncpy(saddru.sun_path, domain.c_str(), UNIX_PATH_MAX);

    if ((ctrlsock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        PLOGF << "can not create unix socket";
        exit(EXIT_FAILURE);
    }

    if (bind(ctrlsock, (struct sockaddr *)&saddru, sizeof(saddru)) < 0)  {
        PLOGF << FRER_UNIX_DOMAIN << " exists";
        exit(EXIT_FAILURE);
    }

    return ctrlsock;
}

