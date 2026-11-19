#include "OnionServer.h"
#include <asm-generic/socket.h>
#include <bits/types/struct_timeval.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <signal.h>

int SocketFd;

struct sockaddr_in SocketAddress;
socklen_t AddrLength;
uint16_t MaxClients = 10;
int* AcceptedSockets;

uint32_t PostRegisterLength = 0;
uint32_t PostRegisterElements = 0;
struct PostReqHandler* PostRegister = NULL;

//struct PostParamaters;

struct PostReqHandler{
    int (*handler)(int,char*,struct PostParamaters);
    char* name;
};

void Cleanup(int signal){
    for(int i = 0; i < MaxClients; i++){
        if (AcceptedSockets[i] != 0){
            close(AcceptedSockets[i]);
        }
    }

    close(SocketFd);
    exit(signal);
}

void SendResponse(int ClientFd, char* msg, int ResponseLength){

    if (send(ClientFd,msg,ResponseLength,0) < 0){
        printf("Failed to send to client\n");
    }
}
/*
struct Response {
    char* Content;
    size_t ResponseLength;
};*/

struct Response CraftHtmlOkResponse(char* Date, char* ContentType, char* Content, size_t ContentLen){
    struct Response Response = {0};

    size_t Header_length = snprintf(NULL, 0, 
        "%s\r\nDate: %s\r\nConnection: close\r\nContent-Length: %d\r\nContent-Type: %s\r\n\r\n", 
        HTTP_OK, Date, (int)ContentLen, ContentType);

    if (Header_length < 0){return Response;}

    size_t TotalLen = Header_length + ContentLen + 1;

    Response.Content = malloc(sizeof(char) * TotalLen + 1);
    Response.ResponseLength = 0;

    Header_length = snprintf(Response.Content, TotalLen, 
        "%s\r\nDate: %s\r\nConnection: close\r\nContent-Length: %d\r\nContent-Type: %s\r\n\r\n", 
        HTTP_OK, Date, (int)ContentLen, ContentType);
    

    memcpy(Response.Content + Header_length, Content, ContentLen);
    Response.Content[TotalLen] = '\0';

    Response.ResponseLength = TotalLen;

    return Response;
}

int SendHtmlNoResponse(int ClientFd){
    return send(ClientFd,HTTP_NoContent,24,0);
}

/*
struct Page {
    char* Content;
    size_t ContentLength;
};*/
struct Body GetBodyPosition(char* PageName){
    struct Body body = {0};
    FILE* page;

    page = fopen(PageName,"r");

    if (page == NULL){
        printf("Tried reading null page\n");
        return body;
    }


    int GetcReturn;
    size_t CurrentPosition = 0;
    uint64_t BodyStart = 0;
    uint64_t BodyEnd = 0;

    while((GetcReturn = fgetc(page)) != EOF){
        CurrentPosition++;
        char character = (char)GetcReturn;

        if (character == '<'){
            char Buffer[6];

            CurrentPosition += fread(Buffer,1,5,page);
            Buffer[5] = '\0';

            for (int i = 0; i < sizeof(Buffer) - 1; i++){
                Buffer[i] = tolower(Buffer[i]);
            }

            if (strncmp(Buffer, "body",sizeof("body") - 1) == 0){
                while((char)(GetcReturn = fgetc(page)) != '\n'){if (GetcReturn == EOF){fclose(page); return body;}}
                BodyStart = CurrentPosition + 1;
            }
            else if (strncmp(Buffer, "/body", sizeof("/body")) == 0){
                BodyEnd = CurrentPosition - 6;
                break;
            }
        }
    }

    body.body_start = BodyStart;
    body.body_end = BodyEnd;

    fclose(page);
    return body;
};

int WriteToPageBody(char* PageName, char* Content, size_t ContentLength){
    struct Body body = GetBodyPosition(PageName);

    if (body.body_start == 0 || body.body_end == 0 || body.body_start == body.body_end){
        printf("Body start or body end position is 0 (or equal)\n");
        return 1;
    }

    printf("Body start: %d | Body end: %d\n",body.body_start, body.body_end);

    FILE* page = fopen(PageName, "r+");

    fseek(page,body.body_start,SEEK_SET);

    fwrite(Content,sizeof(char),ContentLength,page);

    fclose(page);

    return 0;
}

struct Page ReadPage(char* PageName){

    struct Page ReturnedPage = {};
    FILE* page;

    printf("PageName: %s|\n",PageName);
    page = fopen(PageName,"r");

    if (page == NULL){
        printf("User tired acessing null page\n");
        return ReadPage("Pages/404Page.html");
    }

    ReturnedPage.Content = malloc(sizeof(char));
    ReturnedPage.ContentLength = 0;
    char buffer[1024] = { 0 };
    size_t CharacterCount = 0;

    while((CharacterCount = fread(buffer, sizeof(char), 1024, page)) != 0){
        if (CharacterCount == 0){continue;}
        ReturnedPage.Content = realloc(ReturnedPage.Content, sizeof(char) * (CharacterCount + ReturnedPage.ContentLength));
        memcpy(ReturnedPage.Content + ReturnedPage.ContentLength, buffer, CharacterCount);

        ReturnedPage.ContentLength += CharacterCount;
    }

    return ReturnedPage;
}

// Get request functions
// These functions are used when a user gets a certain page, they always look in the "Pages" folder
// --

char* GetPageAddress(char* UserInfo){
    char* PageAddress;
    char* ptr = UserInfo;

    ptr += 4;

    //If the address is null then they go to the main page
    if (*ptr == '/' && *(ptr + 1) == ' '){
        PageAddress = malloc(sizeof("Pages/MainPage.html"));

        memcpy(PageAddress,"Pages/MainPage.html",sizeof("Pages/MainPage.html"));
        return PageAddress;
    }
    else{
        
        //Non home page
        uint16_t AddressSize = 0;

        while(*(ptr + AddressSize) != ' '){
            AddressSize++;
        }

        //Memory allocates enough for "Pages" and the PageAdress
        PageAddress = malloc(sizeof("Pages") + AddressSize);
        memcpy(PageAddress,"Pages",6);
        strncat(PageAddress + sizeof("Pages") - 2,ptr,AddressSize);
        PageAddress[sizeof("Pages") + AddressSize] = '\0';

        return  PageAddress;;
    }

    PageAddress = malloc(sizeof("Pages/404Page.html"));

    memcpy(PageAddress,"Pages/404Page.html",sizeof("Pages/404Page.html"));
    return PageAddress;
}

int HandleGetRequest(int ClientFd ,char* UserInfo){
    char* PageAddress;
    char* ptr = UserInfo;

    PageAddress = GetPageAddress(UserInfo);

    struct Page Page = ReadPage(PageAddress);  
    
    printf("Crafting response\n");

    time_t CurrentTime;
    time(&CurrentTime); 
    char* string = ctime(&CurrentTime);

    
    struct Response Response = CraftHtmlOkResponse("DATE: ", "text/html", Page.Content, Page.ContentLength);

    printf("Sending response\n");
    SendResponse(ClientFd, Response.Content, Response.ResponseLength);

    free(Page.Content);
    free(Response.Content);
    free(PageAddress);

    return 0;
}

//End of get request handling
//
// --
/*
struct PostParamaters {
    size_t ParamaterAmount;
    char** Paramaters;
};*/

int FreePostParamaters(struct PostParamaters Paramaters){
    if (Paramaters.Paramaters == NULL){
        printf("Tried freeing null paramaters. Returning..\n");
        return 1;
    }

    for(int i = 0; i < Paramaters.ParamaterAmount; i++){
        
        if (*(Paramaters.Paramaters) == NULL){
            printf("Tried freeing a null paramater (string)\n");
            continue;
        }
        free(*(Paramaters.Paramaters + i));
    }

    free(Paramaters.Paramaters);

    return 0;
};

size_t CrlfStrlen(char* string){
    size_t length = 0; 
    char* ptr = string;
    
    while(*(ptr + length) != '\0'){
        if (*(ptr + length) == '\r'){
            length++;
            if (*(ptr + length) == '\n'){return length - 1;}
        }
        length++;
    }

    return length;
};

int CheckParamaterDistinction(char* string){
    char* ptr = string;

    if (*ptr != 13){
        return 1;
    }
    ptr++;
    if (*ptr != 10){
        return 1;
    }
    ptr++;
    if (*ptr != 13){
        return 1;
    }
    ptr++;
    if (*ptr != 10){
        return 1;
    }
    ptr++;

    return 0;
};

struct PostParamaters ExtractPostParamaters(char* FullRequest){
    char* ptr = FullRequest;

    char* ParamaterString;
    struct PostParamaters Paramaters = {};

    while(*ptr != '\0'){
        if (CheckParamaterDistinction(ptr) == 0){
            ptr += 4;
            printf("param destinction found\n");
            printf("Ptr: %s\n",ptr);
            break;
        }

        ptr++;
    }
    //ptr++;

    uint16_t ParamaterAmount = 0; 
    Paramaters.Paramaters = malloc(sizeof(char* ) * 10);
    Paramaters.ParamaterAmount = 10;

    printf("Started extracting paramaters\n");

    while(*ptr != '\0'){
        size_t AddressSize = 0;
        while(*(ptr + AddressSize) != '&' && *(ptr + AddressSize) != '\0'){
            if (*(ptr + AddressSize) == '\r' && *(ptr + AddressSize + 1) == '\n'){break;}
            AddressSize++;
        }
        printf("Address size: %ld\n",AddressSize);
        if (AddressSize == 0){continue;}

        ParamaterString = malloc(AddressSize + 1);
        memcpy(ParamaterString, ptr, AddressSize);
        ParamaterString[AddressSize] = '\0';

        if (ParamaterAmount >= Paramaters.ParamaterAmount){
            Paramaters.Paramaters = realloc(Paramaters.Paramaters,sizeof(char* ) * Paramaters.ParamaterAmount * 2);
        }
        *(Paramaters.Paramaters + ParamaterAmount) = ParamaterString; 

        ParamaterAmount++;
        ptr += AddressSize;
        if (*ptr == '\0'){break;}
        ptr++;
    } 
    
    if (ParamaterAmount == 0){
        Paramaters.ParamaterAmount = 0;
        return Paramaters;
    }

    Paramaters.ParamaterAmount = ParamaterAmount;
    Paramaters.Paramaters = realloc(Paramaters.Paramaters, sizeof(char* ) * ParamaterAmount);

    return Paramaters;
}

int HandlePostRequest(int ClientFd, char* PostRequest){

    struct PostReqHandler* handler = NULL;
    char* ptr = PostRequest;
    ptr += 5;
    
    size_t AddressSize = 0;
    while(*(ptr + AddressSize) != ' '){
        AddressSize++;
    }

    char* FunctionName = malloc(AddressSize + 1);
    memcpy(FunctionName, ptr, AddressSize);
    FunctionName[AddressSize] = '\0';

    

    for(int i = 0; i < PostRegisterElements; i++){
        if (strcmp(FunctionName,(PostRegister + i)->name) == 0){
            handler = (PostRegister + i);
        }
    }

    if (handler->handler != NULL){
        printf("Executing handler: %s\n", handler->name);
        struct PostParamaters Paramaters = ExtractPostParamaters(PostRequest);
        int ReturnValue = handler->handler(ClientFd,PostRequest, Paramaters);
        FreePostParamaters(Paramaters);
    }

    return 1;
} 

int HandlePageInfo(int ClientFd, char* UserInfo){

    char* ptr = UserInfo;
    char* Page;

    while(*ptr != '\0'){
        if (strncmp("GET ",ptr,4) == 0){
            return HandleGetRequest(ClientFd,ptr);

        }
        else if (strncmp("POST ", ptr, 5) == 0){            
            return HandlePostRequest(ClientFd,ptr);

        }

        while(*ptr != '\n'){
            if (*ptr == '\0'){
                return 1;
            }
            ptr++;
        }
        ptr++;
    }

    return 1;
}

int HandleClient(int ClientFd, char* UserInfo){
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;

    setsockopt(ClientFd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    printf("Handling request\n");
    return HandlePageInfo(ClientFd,UserInfo);
}

//Listens for incoming connections, if an AcceptedSocket is 0 then it's available
void Listener(){
    while(1){
        
        int new_socket;

        if ((new_socket = accept(SocketFd,(struct sockaddr* )&SocketAddress,&AddrLength)) < 0){
            //printf("Error on accepting connection\n");
        }
        else{
            for(int i = 0; i < MaxClients; i++){
                if (AcceptedSockets[i] == 0){

                    AcceptedSockets[i] = new_socket;
                    printf("Sucesfully connected user\n");

                    struct timeval timeout;
                    timeout.tv_sec = 2;
                    timeout.tv_usec = 0;
                    char buffer[1024] = { 0 };
                    setsockopt(new_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
                    recv(new_socket,buffer,sizeof(buffer) - 1, 0);
                    
                    printf("---------------------------------------------------------------\n");
                    printf("%s\n",buffer);
                    printf("---------------------------------------------------------------\n");

                    HandleClient(new_socket, buffer);
                    break;
                }
            }
            //printf("User failed connecting| max capacity\n");
        }
    }
}

int RegisterPost(int (*handler)(int, char*, struct PostParamaters), char* name){
    if (PostRegister == NULL){
        PostRegisterLength = 1;
        PostRegister = malloc(sizeof(struct PostReqHandler)); 
    }

    if (PostRegisterElements >= PostRegisterLength){
        PostRegisterLength *= 2;
        PostRegister = realloc(PostRegister ,sizeof(struct PostReqHandler ) * PostRegisterLength);
    }

    (PostRegister + PostRegisterElements)->handler = handler;
    (PostRegister + PostRegisterElements)->name = (char* )malloc(strlen(name) + 1); 
    memcpy((PostRegister + PostRegisterElements)->name, name, strlen(name) + 1);

    printf("registered: %s\n", (PostRegister + PostRegisterElements)->name);
    char* ptr = (PostRegister + PostRegisterElements)->name;

    PostRegisterElements++;

    return 0;
}

int StartServer(){
    printf("Redirecting stdout to OnionLog.md\n");

    time_t CurrentTime;
    time(&CurrentTime);
    printf("Started server at: %s",ctime(&CurrentTime));

    //freopen("OnionLog.md", "a+", stdout);

    printf("__________________________________________________________________\n");
    printf("Started server at: %s",ctime(&CurrentTime));

    printf("Starting server\n");
    if ((SocketFd = socket(AF_INET,SOCK_STREAM,0)) < 0){
        printf("Error creating socket\n");
    }
    signal(SIGINT,Cleanup);
    signal(SIGTERM,Cleanup);
    signal(SIGQUIT,Cleanup);
    signal(SIGSTOP,Cleanup);

    AcceptedSockets = malloc(sizeof(int) * MaxClients);
    memset(AcceptedSockets, 0, sizeof(int) * MaxClients);

    SocketAddress.sin_family = AF_INET;
    SocketAddress.sin_port = htons(80);
    SocketAddress.sin_addr.s_addr = INADDR_ANY;
    AddrLength = sizeof(SocketAddress);

    if (bind(SocketFd,(struct sockaddr* )&SocketAddress ,AddrLength) < 0){
        printf("Error binding socket, port 80 is privileged sudo may be required\n");
        exit(EXIT_FAILURE);
    }


    if (listen(SocketFd,MaxClients) < 0){
        printf("Error listening\n");
        exit(EXIT_FAILURE);
    }

    //Creating a child that constantly listens --
    pid_t pid = 0;
    if ((pid = fork()) < 0){
        printf("error forking\n");
        Cleanup(SIGTERM);
        exit(EXIT_FAILURE);
    }
    
    if (pid == 0){
        Listener();
    }

    printf("Server started succesfully. %s\n", ctime(&CurrentTime));

    while(1){

    }

    return 0;
}