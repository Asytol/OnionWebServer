#include "OnionServer.h"
#include <sys/socket.h>
#include <unistd.h>

int test(int ClientFd, char* name, struct PostParamaters Paramaters){

    for(int i = 0; i < Paramaters.ParamaterAmount; i++){
        printf("Paramater: %d = %s\n",i + 1,*(Paramaters.Paramaters + i));
    }

    printf("sucess\n");

    int Status = 200;

    struct Page Page = ReadPage("/");

    struct Response Response = CraftResponse("HTTP/1.1 200 OK", "Date: ", "text/html", Page.Content, Page.ContentLength);
    
    send(ClientFd,Response.Content,Response.ResponseLength,0);
    
    return 0;
};

int main(int argc, char** argv){

    RegisterPost(test, "/action_page.php");
    
    return StartServer();
}