#include "OnionServer.h"
#include <sys/socket.h>
#include <unistd.h>

int test(int ClientFd, char* name, struct PostParamaters Paramaters){

    for(int i = 0; i < Paramaters.ParamaterAmount; i++){
        printf("Paramater: %d = %s\n",i + 1,*(Paramaters.Paramaters + i));
    }

    printf("sucess\n");

    int Status = 200;

    struct Response Response = CraftHtmlOkResponse(HTTP_OK, "Sat May 6 21:05:11 PST 2023", "application/json", "{\"ok\":true}", 12);
    
    send(ClientFd,Response.Content,Response.ResponseLength,0);

    //close(ClientFd);

    return 0;
};

int main(int argc, char** argv){

    RegisterPost(test, "/action_page.php");
    
    return StartServer();
}