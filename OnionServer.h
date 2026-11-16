#ifndef _OnionServer_H
#define _OnionServer_H
#include <stdio.h>

void Cleanup();

void SendResponse(int ClientFd, char* msg, int ResponseLength);

struct Response {
    char* Content;
    size_t ResponseLength;
};

struct Response CraftResponse(char* HttpStatus, char* Date, char* ContentType, char* Content, size_t ContentLen);

struct Page {
    char* Content;
    size_t ContentLength;
};

int RegisterPost(int (*handler)(int, char*), char* name);

struct Page ReadPage(char* PageName);

int StartServer();

#endif