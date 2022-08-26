/*
 * Copyright (c) 2014 Putilov Andrey
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
#pragma once

#ifndef FUNC_H
#define	FUNC_H

#define BUF_LEN 1024


#ifndef bool
#define bool int
#endif
#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#include "websocket.h"
#include "list.h"

typedef struct
{
  int clientSocket;
  int *count;
  uint8_t *gBuffer;
  struct sockaddr_in clientAddr;
  bool isContinue;
  pthread_t ws_thread;
  pthread_mutex_t ws_mutex;
  list_head list;
} WEBSOCKET_PARAM;

extern int sock_count;
extern int listenSocket;
extern bool isContinue;
extern list_head sock_list;
extern WEBSOCKET_PARAM *sock;
extern bool hostExist;
extern in_port_t hostID;

void ws_stop(void);
void error(const char *msg);
void handler(int signo);
int safeSend(WEBSOCKET_PARAM *param, const uint8_t *buffer, size_t bufferSize);
void safeSendAll(const uint8_t *buffer, size_t bufferSize);
void *clientWorker(void *param);

#ifdef	__cplusplus
}
#endif

#endif	/* FUNC_H */