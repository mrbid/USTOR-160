// James William Flecher [github.com/mrbid] ~2018
// public release.
///////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////
/////////////////////////////
///////////////
//////// 

#include <stdio.h>
#include <stdlib.h> //atoi()
#include <string.h>
#include <time.h> 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h> //SIGPIPE
#include <netdb.h> //hostent

///////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////
/////////////////////////////
///////////////
////////

void csend(const char* ip, const char* port, const char* send)
{
    struct addrinfo ai;
    struct addrinfo *result, *p;

    //Addr info structure
    memset(&ai, 0, sizeof(struct addrinfo));
    ai.ai_family = AF_UNSPEC;
    ai.ai_socktype = SOCK_STREAM;
    ai.ai_flags = AI_PASSIVE;
    ai.ai_protocol = 0;
    ai.ai_canonname = NULL;
    ai.ai_addr = NULL;
    ai.ai_next = NULL;

    //Get Addr Info
    int s0 = getaddrinfo(ip, port, &ai, &result);
    if(s0 != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s0));
        freeaddrinfo(result);
        return;
    }
    
    //Try to connect
    int client_socket = -1;
    for(p = result; p != NULL; p = p->ai_next)
    {
        client_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if(client_socket == -1)
            continue;

        if(connect(client_socket, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(client_socket);
            continue;
        }

        break;
    }
    
    //We're done with the ADDRINFO struct now that we have connected.
    freeaddrinfo(result);

    //Write and close
    write(client_socket, send, strlen(send));
    close(client_socket);
}

void *sendThread(void *arg)
{
    signal(SIGPIPE, SIG_IGN);

    while(1)
    {
        char rr[256];
        sprintf(rr, "%i IDFA", rand()%8000);
        csend("127.0.0.1", "6810", rr);

        char rw[256];
        sprintf(rw, "$%i IDFA", rand()%8000);
        csend("127.0.0.1", "6810", rw);
    }
}

int main(int argc , char *argv[])
{
    signal(SIGPIPE, SIG_IGN);

    //Launch some threads
    pthread_t tid;
    for(int i = 0; i < 4; i++)
    {
        if(pthread_create(&tid, NULL, sendThread, NULL) != 0)
        {
            perror("Failed to launch");
            break;
        }
    }

    //loop
    while(1)
    {
        //char rr[256];
        //sprintf(rr, "%i IDFA", rand());
        //csend("127.0.0.1", "6810", rr);

        char rw[256];
        sprintf(rw, "$%i IDFA", rand()%8000);
        csend("127.0.0.1", "6810", rw);
    }
}


