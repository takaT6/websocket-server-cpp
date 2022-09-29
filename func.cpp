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
// #include <errno.h>
#include <pthread.h>
// #include <inttypes.h>
#include <chrono>
#include <thread>

#include <cmath>
#include <iostream>
// #include<math.h>

#include "func.h"


#define PACKET_DUMP
///////////////////////////////////////////////////////////////////////////////////////////////////////
LIST_HEAD(sock_list);
WEBSOCKET_PARAM *sock = NULL;
int sock_count = 0;
int listenSocket = 0;
bool isContinue = false;
bool isStop = false;
bool isProcess = false;
bool hostExists = false;
in_port_t hostID;
struct	tm*	tm_time;
struct timespec ts;
///////////////////////////////////////////////////////////////////////////////////////////////////////
void GET_TIME()
{
	uint64_t	TIME_PC;
	clock_gettime(CLOCK_REALTIME, &ts);
	TIME_PC  = (uint64_t)ts.tv_nsec / 1000000;	//ms
	TIME_PC += (uint64_t)ts.tv_sec * 1000;		//sec
	tm_time = localtime(&ts.tv_sec);			  		//tm?^?????
}
///////////////////////////////////////////////////////////////////////////////////////////////////////
void error(const char *msg)
{
  perror(msg);
  exit(EXIT_FAILURE);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////
void ws_stop(void)
{
  printf("stop connection\n");
  isContinue = false;
  close(listenSocket);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////
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
///////////////////////////////////////////////////////////////////////////////////////////////////////
int safeSend(WEBSOCKET_PARAM *param, const uint8_t *buffer, size_t bufferSize)
{
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
///////////////////////////////////////////////////////////////////////////////////////////////////////
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
///////////////////////////////////////////////////////////////////////////////////////////////////////
int sendMsg(WEBSOCKET_PARAM *param, const uint8_t *buffer, const char *msg)
{
  size_t frameSize = BUF_LEN;
  memset(param->gBuffer, 0, BUF_LEN);
  wsMakeFrame((uint8_t *)msg, strlen(msg), param->gBuffer, &frameSize, WS_TEXT_FRAME);
  pthread_mutex_lock(&param->ws_mutex);
  ssize_t written = send(param->clientSocket, buffer, frameSize, 0);
  pthread_mutex_unlock(&param->ws_mutex);
  if (written == -1)
  {
    return EXIT_FAILURE;
  }
  if (written != frameSize)
  {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////
void sendMsgToALL(WEBSOCKET_PARAM *param, const char *msg)
{ 
  size_t frameSize = BUF_LEN;
  memset(param->gBuffer, 0, BUF_LEN);
  size_t len = strlen(msg);
  wsMakeFrame((uint8_t *)msg, len, param->gBuffer, &frameSize, WS_TEXT_FRAME);
  WEBSOCKET_PARAM *data, *dend;
  list_for_each_entry_safe(data, dend, &sock_list, list)
  {
    if (safeSend(data, param->gBuffer, frameSize) != EXIT_SUCCESS)
    {
      data->isContinue = false;
    }
  }
}
///////////////////////////////////////////////////////////////////////////////////////////////////////

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
        prepareBuffer;
        wsGetHandshakeAnswer(&hs, ths->gBuffer, &frameSize);
        safeSend(ths, ths->gBuffer, frameSize);
        state = WS_STATE_NORMAL;
        freeHandshake(&hs);
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
        // msg =>>>> run
        if (strcmp("run", (char *)data) == 0 && !isProcess)
        {
          isStop = false;
          isProcess = true;
          sendMsg(ths, ths->gBuffer, "{\"type\":\"isProcess\",\"value\":true}");

          std::chrono::system_clock::time_point start, end;
          start = std::chrono::system_clock::now();
          char msg[60];

          double degree = 0.0;
          double ff = M_PI / 180.0;
          while(!isStop)
          {
            // end = std::chrono::system_clock::now();
            sprintf(msg, "{\"type\":\"data\",\"timestamp\":%.5f,\"value\":%.5f}"
              , static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now()-start).count())/1000000000///1000
              , sin(degree * ff)
            );
            degree += 0.5;
            // sendMsgToALL(ths,msg);
            sendMsg(ths, ths->gBuffer, msg);
            initNewFrame;
            usleep(50);
            // usleep(9000);
          }
          isProcess = false;
          sendMsg(ths, ths->gBuffer, "{\"type\":\"isProcess\",\"value\":false}");
        }
        // msg =>>>> stop
        if (strcmp("stop", (char *)data) == 0)
        {
          isStop = true;
          printf("stop\n");
        }
        // msg =>>>> beHost [send: hostExist, isHsot] [change: hostID, hostExist]
        if (strcmp("beHost", (char *)data) == 0)
        {
          if(!hostExists){
            hostExists = true;
            hostID = ths->clientAddr.sin_port;
            sendMsgToALL(ths, "{\"type\":\"hostExists\",\"value\":true}");
            sendMsg(ths, ths->gBuffer, "{\"type\":\"isHost\",\"value\":true}");
          }else{
            sendMsg(ths, ths->gBuffer, "{\"type\":\"isHost\",\"value\":false}");
          }
        }
        // msg =>>>> beGuest [send: isGuest]
        if (strcmp("beGuest", (char *)data) == 0)
        {
          sendMsg(ths, ths->gBuffer, "{\"type\":\"isGuest\",\"value\":true}");
        }
        // msg =>>>> resignHost [send: notHost, hostExists]
        if (strcmp("resignHost", (char *)data) == 0)
        {
          if(hostExists){
            hostExists = false;
            sendMsg(ths, ths->gBuffer, "{\"type\":\"notHost\",\"value\":true}");
            sendMsgToALL(ths, "{\"type\":\"hostExists\",\"value\":false}");
          }else{
            sendMsg(ths, ths->gBuffer, "{\"type\":\"notHost\",\"value\":false}");
          }
        }
        // msg =>>>> checkServer [send: hostExists, isProcess]
        if (strcmp("checkServer", (char *)data) == 0)
        {
          char msg2[50];
          sprintf(msg2, "{\"type\":\"hostExists\",\"value\":%d}",hostExists);
          sendMsg(ths, ths->gBuffer, msg2);
          char msg3[50];
          sprintf(msg3, "{\"type\":\"isProcess\",\"value\":%d}",isProcess);
          sendMsg(ths, ths->gBuffer, msg3);
        }
        initNewFrame;
      }
    }
  } // read/write cycle
  printf("disconnected %s:%d\n", inet_ntoa(ths->clientAddr.sin_addr), ntohs(ths->clientAddr.sin_port));
  bool sendHostQuit = false;
  if (hostID ==  ths->clientAddr.sin_port)sendHostQuit = true;
  close(ths->clientSocket);
  free(ths->gBuffer);
  list_del(&ths->list);
  free(ths);
  if (sendHostQuit) {
    printf("There is no host\n");
    hostExists = false;
    // sendMsgToALL(ths, "{\"hostExists\":false}");
    sendMsgToALL(ths, "{\"type\":\"hostExists\",\"value\":false}");
  }
  pthread_exit(NULL);
  return NULL;
}
