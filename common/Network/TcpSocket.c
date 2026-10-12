#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#define MAX_CONNECT_NUM 1

int TcpSocketInit(const char *Ip, int Port)
{
    int Optval = 1;
    int Fd = socket(AF_INET, SOCK_STREAM, 0);
    if (Fd < 0)
    {
        perror("socket");
        return -1;
    }

    /* 解除端口占用 */
    if (setsockopt(Fd, SOL_SOCKET, SO_REUSEADDR, &Optval, sizeof(Optval)) < 0)
    {
        perror("setsockopt\n");
        return -1;
    }

    int Flags = fcntl(Fd, F_GETFL, 0);

    fcntl(Fd, F_SETFL, Flags | O_NONBLOCK);

    struct sockaddr_in ServerAddr;
    bzero(&ServerAddr, sizeof(struct sockaddr));
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_port = htons(Port);
    if (NULL == Ip)
    {
        ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    else
    {
        ServerAddr.sin_addr.s_addr = inet_addr(Ip);
    }

    if (bind(Fd, (struct sockaddr *)&ServerAddr, sizeof(struct sockaddr)) < 0)
    {
        perror("bind");
        close(Fd);
        return -1;
    }

    if (listen(Fd, MAX_CONNECT_NUM) < 0)
    {
        perror("listen");
        close(Fd);
        return -1;
    }

    return Fd;
}

int TcpAccept(int Fd)
{
    fd_set Rdset;

    FD_ZERO(&Rdset);

    FD_SET(Fd, &Rdset);

    struct timeval Timeout;
    Timeout.tv_sec = 0;
    Timeout.tv_usec = 50000;

    int Ret = select(Fd + 1, &Rdset, NULL, NULL, &Timeout);

    if (Ret > 0)
    {
        FD_CLR(Fd, &Rdset);

        struct sockaddr_in ClientAddr = {0};
        socklen_t addrlen = sizeof(struct sockaddr);
        int NewFd = accept(Fd, (struct sockaddr *)&ClientAddr, &addrlen);
        if (NewFd < 0)
        {
            perror("accept");
            close(Fd);
            return -1;
        }
        printf("TcpAccept Client(Ip = %s, Port = %d)\n", inet_ntoa(ClientAddr.sin_addr), ntohs(ClientAddr.sin_port));

        return NewFd;
    }
    return 0;
}

int TcpConnect(const char *Ip, int Port)
{
    int Fd = socket(AF_INET, SOCK_STREAM, 0);
    if (Fd < 0)
    {
        perror("socket");
        return -1;
    }

    struct sockaddr_in ServerAddr;
    bzero(&ServerAddr, sizeof(struct sockaddr));
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_port = htons(Port);
    ServerAddr.sin_addr.s_addr = inet_addr(Ip);

    if (connect(Fd, (struct sockaddr *)&ServerAddr, sizeof(struct sockaddr)) < 0)
    {
        perror("connect");
        close(Fd);
        return -1;
    }

    return Fd;
}

int TcpNonblockingRecv(int ConnSockfd, void *RxBuf, int BufLen, int TimevalSec, int TimevalUsec)
{
    fd_set Readset;
    struct timeval Timeout = {0, 0};
    int Maxfd = 0;
    int Fp0 = 0;
    int RecvBytes = 0;
    int Ret = 0;

    Timeout.tv_sec = TimevalSec;
    Timeout.tv_usec = TimevalUsec;
    FD_ZERO(&Readset);
    FD_SET(ConnSockfd, &Readset);

    Maxfd = ConnSockfd > Fp0 ? (ConnSockfd + 1) : (Fp0 + 1);

    Ret = select(Maxfd, &Readset, NULL, NULL, &Timeout);
    if (Ret > 0)
    {
        if (FD_ISSET(ConnSockfd, &Readset))
        {
            if ((RecvBytes = recv(ConnSockfd, RxBuf, BufLen, MSG_DONTWAIT)) == -1)
            {
                perror("recv");
                return -1;
            }
        }
    }
    else
    {
        return -1;
    }

    return RecvBytes;
}

int TcpBlockingRecv(int ConnSockfd, void *RxBuf, uint16_t BufLen)
{
    return recv(ConnSockfd, RxBuf, BufLen, 0);
}

int TcpSend(int ConnSockfd, uint8_t *TxBuf, uint16_t BufLen)
{
    return send(ConnSockfd, TxBuf, BufLen, 0);
}

void TcpClose(int Sockfd)
{
    close(Sockfd);
}