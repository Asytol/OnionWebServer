#include "OnionServer.h"

int test(int ClientFd, char* name){

    printf("sucess\n");
    return 0;
};

int main(int argc, char** argv){

    RegisterPost(test, "/action_page.php");
    
    return StartServer();
}