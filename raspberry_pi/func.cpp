#include <stdio.h>
#include <string.h>
#include <stdbool.h>
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

LIST_HEAD(sock_list);
WEBSOCKET_PARAM *sock = NULL;
int sock_count = 0;
int listenSocket = 0;
bool isContinue = false;

void error(const char *msg)
{
  perror(msg);
  exit(EXIT_FAILURE);
}

void ws_stop(void)
{
  printf("stop connection\n");
  isContinue = false;
  close(listenSocket);
}

void handler(int signo)
{
  if (signo == SIGINT)
  {
    printf("\nreceived SIGINT\n");
    ws_stop();
  }
  else
  {
    printf("catch signal hander of %d\n", signo);
  }
}

int safeSend(WEBSOCKET_PARAM *param, const uint8_t *buffer, size_t bufferSize)
{
#ifdef PACKET_DUMP
  printf("out packet:\n");
  fwrite(buffer, 1, bufferSize, stdout);
  printf("\n");
#endif
  pthread_mutex_lock(&param->ws_mutex);
  ssize_t written = send(param->clientSocket, buffer, bufferSize, 0);
  pthread_mutex_unlock(&param->ws_mutex);
  if (written == -1)
  {
    return EXIT_FAILURE;
  }
  if (written != bufferSize)
  {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

void safeSendAll(const uint8_t *buffer, size_t bufferSize)
{
  WEBSOCKET_PARAM *data, *dend;
  list_for_each_entry_safe(data, dend, &sock_list, list)
  {
    if (safeSend(data, buffer, bufferSize) != EXIT_SUCCESS)
    {
      data->isContinue = false;
    }
  }
}

void sendMsg(const char )
{

}

void *clientWorker(void *param)
{
  WEBSOCKET_PARAM *ths = (WEBSOCKET_PARAM *)param;
  ths->isContinue = true;
  memset(ths->gBuffer, 0, BUF_LEN);
  size_t readedLength = 0;
  size_t frameSize = BUF_LEN;
  enum wsState state = WS_STATE_OPENING;
  uint8_t *data = NULL;
  size_t dataSize = 0;
  enum wsFrameType frameType = WS_INCOMPLETE_FRAME;
  struct handshake hs;
  nullHandshake(&hs);

#define prepareBuffer  \
  frameSize = BUF_LEN; \
  memset(ths->gBuffer, 0, BUF_LEN);
#define initNewFrame               \
  frameType = WS_INCOMPLETE_FRAME; \
  readedLength = 0;                \
  memset(ths->gBuffer, 0, BUF_LEN);

  while (isContinue && ths->isContinue && (frameType == WS_INCOMPLETE_FRAME))
  {
    ssize_t readed = recv(ths->clientSocket, ths->gBuffer + readedLength, BUF_LEN - readedLength, MSG_DONTWAIT);
    if ((readed == -EAGAIN) || (readed == -EWOULDBLOCK))
    {
      printf("readed = %ld\n", readed);
      perror("recv failed");
      break;
    }
    else if (readed <= 0)
    {
      usleep(1000);
      continue;
    }
#ifdef PACKET_DUMP
    printf("in packet:\n");
    fwrite(ths->gBuffer, 1, readed, stdout);
    printf("\n");
#endif
    readedLength += readed;
    assert(readedLength <= BUF_LEN);

    if (state == WS_STATE_OPENING)
    {
      frameType = wsParseHandshake(ths->gBuffer, readedLength, &hs);
    }
    else
    {
      frameType = wsParseInputFrame(ths->gBuffer, readedLength, &data, &dataSize);
    }

    if ((frameType == WS_INCOMPLETE_FRAME && readedLength == BUF_LEN) || frameType == WS_ERROR_FRAME)
    {
      if (frameType == WS_INCOMPLETE_FRAME)
        printf("buffer too small");
      else
        printf("error in incoming frame\n");

      if (state == WS_STATE_OPENING)
      {
        prepareBuffer;
        frameSize = sprintf((char *)ths->gBuffer,
                            "HTTP/1.1 400 Bad Request\r\n"
                            "%s%s\r\n\r\n",
                            versionField,
                            version);
        safeSendAll(ths->gBuffer, frameSize);
        break;
      }
      else
      {
        prepareBuffer;
        wsMakeFrame(NULL, 0, ths->gBuffer, &frameSize, WS_CLOSING_FRAME);
        safeSend(ths, ths->gBuffer, frameSize);
        state = WS_STATE_CLOSING;
        initNewFrame;
      }
    }

    if (state == WS_STATE_OPENING)
    {
      assert(frameType == WS_OPENING_FRAME);
      if (frameType == WS_OPENING_FRAME)
      {
        // if resource is right, generate answer handshake and send it
        if (strcmp(hs.resource, "/echo") != 0)
        {
          frameSize = sprintf((char *)ths->gBuffer, "HTTP/1.1 404 Not Found\r\n\r\n");
          safeSend(ths, ths->gBuffer, frameSize);
          break;
        }

        prepareBuffer;
        wsGetHandshakeAnswer(&hs, ths->gBuffer, &frameSize);
        freeHandshake(&hs);
        safeSend(ths, ths->gBuffer, frameSize);
        state = WS_STATE_NORMAL;
        initNewFrame;
      }
    }
    else
    {
      if (frameType == WS_CLOSING_FRAME)
      {
        if (state == WS_STATE_CLOSING)
        {
          break;
        }
        else
        {
          prepareBuffer;
          wsMakeFrame(NULL, 0, ths->gBuffer, &frameSize, WS_CLOSING_FRAME);
          safeSend(ths, ths->gBuffer, frameSize);
          break;
        }
      }
      else if (frameType == WS_TEXT_FRAME)
      {
        char go_str[3] = "go";
        if (strcmp((char *)go_str, (char *)data) == 0)
        {
          uint8_t *recievedString = NULL;

          char sendMsg[] = "Hello, IF.";
          size_t sendMsg_len = sizeof(sendMsg) - 1;
          dataSize = sendMsg_len;

          recievedString = (uint8_t *)malloc(dataSize + 1);
          assert(recievedString);
          memcpy(recievedString, (uint8_t *)sendMsg, dataSize);
          
          recievedString[dataSize] = 0;

          prepareBuffer;
          wsMakeFrame(recievedString, dataSize, ths->gBuffer, &frameSize, WS_TEXT_FRAME);
          free(recievedString);
          safeSendAll(ths->gBuffer, frameSize);
          initNewFrame;
        }
        initNewFrame;
      }
    }
  } // read/write cycle
  printf("disconnected %s:%d\n", inet_ntoa(ths->clientAddr.sin_addr), ntohs(ths->clientAddr.sin_port));

  close(ths->clientSocket);
  free(ths->gBuffer);
  list_del(&ths->list);
  free(ths);
  pthread_exit(NULL);
  return NULL;
}
