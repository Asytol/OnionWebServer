#include "OnionServer.h"
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

int RegisterWholesomeMessage(int ClientFd, char* name, struct PostParamaters Paramaters){

    for(int i = 0; i < Paramaters.ParamaterAmount; i++){
        printf("Paramater: %d = (%s)\n",i + 1,*(Paramaters.Paramaters + i));
    }

    WriteToPageBody("Pages/WholesomeMessages.html",*Paramaters.Paramaters , CrlfStrlen(*Paramaters.Paramaters));

    int Status = SendHtmlNoResponse(ClientFd);

    close(ClientFd);

    return 0;
};

int main(int argc, char** argv){

    RegisterPost(RegisterWholesomeMessage, "/action_page.php");
    
    return StartServer();
}