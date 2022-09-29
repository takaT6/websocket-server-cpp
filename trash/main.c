/*
 * Copyright (c) 2014 Putilov Andrey
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of ths software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and ths permission notice shall be included in
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
#include "websocket.h"
#include <errno.h>
#include <pthread.h>
#include "list.h"
#include <inttypes.h>
#define PORT 8088
#define BUF_LEN 1024

//#define PACKET_DUMP
#ifndef bool
	#define bool int
#endif
#ifndef true
	#define true 1
#endif
#ifndef false
	#define false 0
#endif

typedef struct {
	int clientSocket;
	int *count;
	uint8_t *gBuffer;
	struct sockaddr_in clientAddr;
	bool isContinue;
	pthread_t ws_thread;
	pthread_mutex_t ws_mutex;
	list_head list;
} WEBSOCKET_PARAM;

LIST_HEAD(sock_list);
WEBSOCKET_PARAM *sock=NULL;
int sock_count=0;
int listenSocket;

bool isContinue = false;

void ws_stop(void);

void handler(int signo)
{
	if (signo == SIGINT) {
		printf("\nreceived SIGINT\n");
		ws_stop();
	} else {
		printf("catch signal hander of %d\n",signo);
	}
}
void error(const char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
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
	if (written == -1) {
		return EXIT_FAILURE;
	}
	if (written != bufferSize) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

void safeSendAll(const uint8_t *buffer, size_t bufferSize) {
	int result;
	WEBSOCKET_PARAM *data,*dend;
    list_for_each_entry_safe(data,dend,&sock_list,list){
		if(safeSend(data,buffer,bufferSize)!=EXIT_SUCCESS) {
			data->isContinue = false;
		}
    }
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

#define prepareBuffer frameSize = BUF_LEN; memset(ths->gBuffer, 0, BUF_LEN);
#define initNewFrame frameType = WS_INCOMPLETE_FRAME; readedLength = 0; memset(ths->gBuffer, 0, BUF_LEN);

	while (isContinue && ths->isContinue &&(frameType == WS_INCOMPLETE_FRAME)) {
		ssize_t readed = recv(ths->clientSocket, ths->gBuffer+readedLength, BUF_LEN-readedLength, MSG_DONTWAIT);
		if((readed == -EAGAIN) || (readed == -EWOULDBLOCK)){
			printf("readed = %ld\n",readed);
			perror("recv failed");
			break;
		} else if(readed <= 0) {
			usleep(1000);
			continue;
		}
#ifdef PACKET_DUMP
		printf("in packet:\n");
		fwrite(ths->gBuffer, 1, readed, stdout);
		printf("\n");
#endif
		readedLength+= readed;
		assert(readedLength <= BUF_LEN);

		if (state == WS_STATE_OPENING) {
			frameType = wsParseHandshake(ths->gBuffer, readedLength, &hs);
		} else {
			frameType = wsParseInputFrame(ths->gBuffer, readedLength, &data, &dataSize);
		}

		if ((frameType == WS_INCOMPLETE_FRAME && readedLength == BUF_LEN) || frameType == WS_ERROR_FRAME) {
			if (frameType == WS_INCOMPLETE_FRAME)
				printf("buffer too small");
			else
				printf("error in incoming frame\n");

			if (state == WS_STATE_OPENING) {
				prepareBuffer;
				frameSize = sprintf((char *)ths->gBuffer,
						"HTTP/1.1 400 Bad Request\r\n"
						"%s%s\r\n\r\n",
						versionField,
						version);
				safeSendAll(ths->gBuffer, frameSize);
				break;
			} else {
				prepareBuffer;
				wsMakeFrame(NULL, 0, ths->gBuffer, &frameSize, WS_CLOSING_FRAME);
				safeSend(ths,ths->gBuffer, frameSize);
				state = WS_STATE_CLOSING;
				initNewFrame;
			}
		}

		if (state == WS_STATE_OPENING) {
			assert(frameType == WS_OPENING_FRAME);
			if (frameType == WS_OPENING_FRAME) {
				// if resource is right, generate answer handshake and send it
				if (strcmp(hs.resource, "/echo") != 0) {
					frameSize = sprintf((char *)ths->gBuffer, "HTTP/1.1 404 Not Found\r\n\r\n");
					safeSend(ths,ths->gBuffer, frameSize);
					break;
				}

				prepareBuffer;
				wsGetHandshakeAnswer(&hs, ths->gBuffer, &frameSize);
				freeHandshake(&hs);
				safeSend(ths,ths->gBuffer, frameSize);
				state = WS_STATE_NORMAL;
				initNewFrame;
			}
		} else {
			if (frameType == WS_CLOSING_FRAME) {
				if (state == WS_STATE_CLOSING) {
					break;
				} else {
					prepareBuffer;
					wsMakeFrame(NULL, 0, ths->gBuffer, &frameSize, WS_CLOSING_FRAME);
					safeSend(ths,ths->gBuffer, frameSize);
					break;
				}
			} else if (frameType == WS_TEXT_FRAME) {
				char go_str[3] = "go";
				if( strcmp((char*)go_str,(char*)data) == 0 ){
					char str[] = "Hello, IF.";
					size_t str_len = sizeof(str) -1;
					dataSize = str_len;
					data = (uint8_t *)str;
					askOpinion();
				}

				uint8_t *recievedString = NULL;
				recievedString = (uint8_t *)malloc(dataSize+1);
				assert(recievedString);
				memcpy(recievedString, data, dataSize);
				recievedString[ dataSize ] = 0;
				prepareBuffer;
				wsMakeFrame(recievedString, dataSize, ths->gBuffer, &frameSize, WS_TEXT_FRAME);
				free(recievedString);
				safeSendAll(ths->gBuffer, frameSize);
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


int main(int argc,char *argv[]){
	struct sockaddr_in local;
	int on = 1;
	
	// add first connection

	if (signal(SIGINT, handler) == SIG_ERR) {
		error("SIGINT error\n");
	}

	listenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (listenSocket == -1) {
		error("create socket failed");
	}

	setsockopt(listenSocket, SOL_SOCKET,  SO_REUSEADDR,&on, sizeof(on));

	memset(&local, 0, sizeof(local));
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_port = htons(PORT);
	isContinue = true;

	if (bind(listenSocket, (struct sockaddr *) &local, sizeof(local)) <0 ) {
		error("bind failed");
	}
	if (listen(listenSocket, 1) == -1) {
		error("listen failed");
	}

	printf("waiting connection\n");
	while (isContinue) {
		struct sockaddr_in remote;
		int clientSocket;
		socklen_t sockaddrLen = sizeof(remote);
		clientSocket = accept(listenSocket, (struct sockaddr*)&remote, &sockaddrLen);
		if((clientSocket == -1) || ( clientSocket == -EAGAIN ) || (clientSocket == -EWOULDBLOCK)) {
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
		memcpy(&sock->clientAddr,&remote,sizeof(remote));
		pthread_mutex_init(&sock->ws_mutex, NULL);
		INIT_LIST_HEAD(&sock->list);
		list_add_tail(&sock->list, &sock_list);

		pthread_create(&sock->ws_thread, NULL, clientWorker, sock);
	}
	WEBSOCKET_PARAM *data,*dend;
    list_for_each_entry_safe(data,dend,&sock_list,list){
		pthread_join(data->ws_thread, NULL);
    }
	// stop thread
	return EXIT_SUCCESS;
}

void ws_stop(void) {
	printf("stop connection\n");
	isContinue = false;
	close(listenSocket);
}
