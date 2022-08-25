#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <pthread.h>
#include <inttypes.h>

#include "func.h"


#define PORT 8088

int main()
{
  while(true)
  {
    struct sockaddr_in local;
    int on = 1;
    if (signal(SIGINT, handler) == SIG_ERR)
    {
      error("SIGINT error\n");
    }

    listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == -1)
    {
      error("create socket failed");
    }

    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(PORT);
    isContinue = true;

    if (bind(listenSocket, (struct sockaddr *)&local, sizeof(local)) < 0)
    {
      error("bind failed");
    }
    if (listen(listenSocket, 1) == -1)
    {
      error("listen failed");
    }

    printf("waiting connection\n");
    while (isContinue)
    {
      struct sockaddr_in remote;
      int clientSocket;
      socklen_t sockaddrLen = sizeof(remote);
      clientSocket = accept(listenSocket, (struct sockaddr *)&remote, &sockaddrLen);
      if ((clientSocket == -1) || (clientSocket == -EAGAIN) || (clientSocket == -EWOULDBLOCK))
      {
        usleep(1000);
        continue;
      }
      printf("connect from %s:%d\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));
      // list
      sock_count++;
      sock = (WEBSOCKET_PARAM *)malloc(sizeof(WEBSOCKET_PARAM));
      sock->count = &sock_count;
      sock->clientSocket = clientSocket;
      sock->gBuffer = (uint8_t *)malloc(BUF_LEN);
      memcpy(&sock->clientAddr, &remote, sizeof(remote));
      pthread_mutex_init(&sock->ws_mutex, NULL);
      INIT_LIST_HEAD(&sock->list);
      list_add_tail(&sock->list, &sock_list);

      pthread_create(&sock->ws_thread, NULL, clientWorker, sock);
    }
    WEBSOCKET_PARAM *data, *dend;
    list_for_each_entry_safe(data, dend, &sock_list, list)
    {
      pthread_join(data->ws_thread, NULL);
    }
  }
  // stop thread
  return EXIT_SUCCESS;
}