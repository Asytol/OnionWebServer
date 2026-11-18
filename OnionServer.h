#ifndef _OnionServer_H
#define _OnionServer_H
#include <stdio.h>

#define CRLF "\r\n"
#define HTTP_OK "HTTP/1.1 200 OK"
#define HTTP_NoContent "HTTP/1.1 204 No Content"

void SendResponse(int ClientFd, char* msg, int ResponseLength);

struct Response {
    char* Content;
    size_t ResponseLength;
};

struct Response CraftHtmlOkResponse(char* HttpStatus, char* Date, char* ContentType, char* Content, size_t ContentLen);

struct Page {
    char* Content;
    size_t ContentLength;
};

//Reads a specific page from Pages section
struct Page ReadPage(char* PageName);

//The paramaters returned by a post function, having a list of strings and a Paramater amount
struct PostParamaters {
    size_t ParamaterAmount;
    char** Paramaters;
};

//Registers a post function
int RegisterPost(int (*handler)(int, char*, struct PostParamaters), char* name);

//The way Paramaters are extracted internally. This is exposed in the library incase you need it. (Requires in the whole client recv, not just the end)
struct PostParamaters ExtractPostParamaters(char* FullRequest);

//Should almost never be used as the onion client should already free paramaters automatically
int FreePostParamaters(struct PostParamaters Paramaters);

int StartServer();

#endif