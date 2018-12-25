// James William Flecher [github.com/mrbid] ~2018
// public release.
///////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////
/////////////////////////////
///////////////
////////
/*

IDFA - a unique user id

This will allow accurate and high performance for low cost single-server rtb
and dsp solutions, there is no set time for IDFA expirary as it is automatic,
based on your average daily win rate, the buffers MAX_SITES and MAX_IDFA_PER_SITE
will fill accordingly.

The idea here is if your winning around one million impressions a day, from roughly
250 different publisher/site id's then MAX_SITES 400 and MAX_IDFA_PER_SITE 4000
would seem a suitable memory requirement to ensure IDFA's where kept for around
24 hours.

The buffer write is cyclic/ring buffer so oldest IDFA's are the first to be forgotten
when the buffer does finally fill and loop back to it's beginning.

This creates am effect where smaller sites sending less traffic have their IDFA's
blocked for a longer period of time. Minimizing repeat traffic from lower supplying
traffic sources.

You will get notifications in the console which let you know when site buffers max
and trigger a loop, all are timestamped.

Execute the USTOR process and you will be able to use the php functions below
on the same local server.

USTOR opens a TCP port 6810 and then allows you to send two simple commands
as can be seen in the php code below, which allow adding an IDFA and checking
to see if an IDFA has been added.

The SITES / IDFA bucketing keeps array iterations low, and the ring buffer
write head with all the memory pre-allocated keeps writes O(1) while still
being advantageous to culling oldest IDFA's first.

For larger scale solutions I am currently using UDP and a bucketed Hash Map.

*/
///////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////
/////////////////////////////
///////////////
////////

//PHP Functions for Write and Check operations
/*
function add_ustor($source, $idfa)
{
    $fp = stream_socket_client("tcp://127.0.0.1:6810", $errno, $errstr, 1);
    if($fp)
    {
        fwrite($fp, "$" . sha1($source) . " " . sha1($idfa));
        fclose($fp);
    }
}

function check_ustor($source, $idfa)
{
    $fp = stream_socket_client("tcp://127.0.0.1:6810", $errno, $errstr, 1);
    if($fp)
    {
        $r = fwrite($fp, sha1($source) . " " . sha1($idfa));
        if($r == FALSE)
        {
            fclose($fp);
            return TRUE;
        }
        $r = fread($fp, 1);
        if($r != FALSE && $r[0] == 'y')
        {
            fclose($fp);
            return TRUE;
        }
        fclose($fp);
        return FALSE; //FALSE = It's not stored, you can bid :)
    }
    return TRUE;
}
*/

///////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////
/////////////////////////////
///////////////
////////

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h> //exit
#include <time.h> //time()
#include <signal.h> //SIGPIPE
#include <netdb.h> //hostent
#include <unistd.h> //Sleep
#include "sha1.h"


// [directly relfects memory usage]
#define MAX_SITES 2048              //Max number of sites possible to track
#define MAX_IDFA_PER_SITE 32768     //How many IDFA's we can track at any one time, per site

//Maximum recv length of incomming string
#define RECV_BUFF_SIZE 2048

///////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////
/////////////////////////////
///////////////
////////

//Max lenth of small strings
#define MIN_LEN 256
#define uint unsigned int

void timestamp()
{
    time_t ltime = time(NULL);
    fprintf(stderr, "\n%s", asctime(localtime(&ltime)));
}

///////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////
/////////////////////////////
///////////////
////////
///
//
//
//
/*
.   SAH1 hashing
*/

#define HASH_LEN 20 //all bytes no null term

//SHA1 Hash Digest/Result
struct sre
{
    char digest[HASH_LEN];
};
typedef struct sre sre;

//SHA1 Hash Func
void sre1(sre *r, const char* str)
{
    SHA1(r->digest, str, strlen(str));
}

//SHA1 Hash Comparison Func
int sre_com(const sre *d1, const sre *d2)
{
    if(memcmp(d1->digest, d2->digest, HASH_LEN) == 0)
        return 1; //Are the same hashes
    return 0; //Not the same hashes
}

//SHA1 Hash Copy Func
void sre_copy(sre *d, const sre *src)
{
    memcpy(d->digest, src->digest, HASH_LEN);
}

//SHA1 Hash Null Func
void sre_null(sre *d)
{
    memset(d->digest, 0, HASH_LEN);
}

//SHA1 Hash IsNull Func
int sre_isnull(sre *d)
{
    const static sre snull = {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
    if(memcmp(d->digest, &snull, HASH_LEN) == 0)
        return 1; //Are the same hashes
    return 0; //Not the same hashes
}

///////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////
/////////////////////////////
///////////////
////////
///
//
//
//
/*
.
.
.
.
. ~ Unique Capping Code,
.   an array of impressions per site,
.   fixed allocated memory size.
*/

//Output stats
unsigned long long int block = 0, allow = 0, rej = 0, reset = 0, site_count = 0, err = 0;

//Impressions are split into site domain buckets
struct site
{
    sre site;
    sre idfa_array[MAX_IDFA_PER_SITE];
    int idfa_head;
};

//Our site buckets
struct site sites[MAX_SITES];

//Init sites array
void init_sites() //Pub
{
    for(int i = 0; i < MAX_SITES; ++i)
    {
        memset(sites[i].idfa_array, 0, sizeof(sre)*MAX_IDFA_PER_SITE);
        sites[i].idfa_head = 0;
        sre_null(&sites[i].site);
    }
    block = 0;
    allow = 0;
    rej = 0;
    reset = 0;
    site_count = 0;
}

int get_site(const sre *site) //Priv
{
    //Look for site
    for(int i = 0; i < MAX_SITES; ++i)
    {  
        //No site found, but space for new site
        if(sre_isnull(&sites[i].site) == 1)
        {
            //write new site
            sre_copy(&sites[i].site, site);
            site_count++;
            return i;
        }

        //Found site
        if(sre_com(&sites[i].site, site) == 1)
            return i;
    }

    //Alert num sites maxed
    timestamp();
    fprintf(stderr, "Unique Max Num Sites out of memory, (s)warn.\n");
    init_sites(); //reset memory so we can re-buffer sites list
    fprintf(stderr, " ^.. Memory Reset (Flushed), (!)notice.\n");
    return -1;
}

//Check against all idfa in memory for a match, if it's not there, add it,
int has_idfa(const sre *site, const sre *idfa) //Pub
{
    //Find site index
    int site_index = get_site(site);
    if(site_index == -1)
        return 0; //No such site cached

    //For each impression
    for(int i = 0; i < MAX_IDFA_PER_SITE; ++i)
    {
        //if no idfa set here, bail on search (pre-op) 
        if(sre_isnull(&sites[site_index].idfa_array[i]) == 1)
            break;

        //if IDFA = IDFA 
        if(sre_com(&sites[site_index].idfa_array[i], idfa) == 1)
            return 1; //Got it
    }

    //add_idfa(site_index, idfa); //No idfa? Add it (Auto insta-block)
    return 0; //NO IFDA :(
}

//Set's ifa,
void add_idfa(const sre *site, const sre *idfa) //Pub
{
    //Find site index
    int site_index = get_site(site);
    if(site_index == -1)
        return; //No such site cached

    //Get write head
    int i = sites[site_index].idfa_head;

    //Set write head
    sre_copy(&sites[site_index].idfa_array[i], idfa);

    //Increment write head
    sites[site_index].idfa_head++;
    if(sites[site_index].idfa_head >= MAX_IDFA_PER_SITE)
    {
        sites[site_index].idfa_head = 0;
        reset++;
        timestamp();
        fprintf(stderr, "%i: Site Buffer Loop Reached.\n", site_index);
    }

    return;
}

///////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////
/////////////////////////////
///////////////
////////
///
//
//
//
/* ~ Socket I/O interface to the keystore
*/

void parseMsg(const char* str, char* site, char* idfa)
{
    char *wh = site;
    int i = 0;
    if(str[0] == '$')
        i = 1;
    const int len = strlen(str);
    for(; i < len; i++)
    {
        if(str[i] == ' ')
        {
            *wh = 0x00;
            wh = idfa;
            continue;
        }

        *wh = str[i];
        wh++;
    }
    *wh = 0x00;
}

int main(int argc , char *argv[])
{
    //Init memory for unique filtering
    init_sites();
    
    //Vars
    int socket_desc, client_sock, c, read_size;
    struct sockaddr_in server, client;

    //Prevent process termination from broken pipe signal
    signal(SIGPIPE, SIG_IGN);

    //Security Log
    timestamp();
    printf("USTOR - James William Fletcher\n");
    printf("https://github.com/mrbid\n");
    printf("US-160\n");
     
    //Create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_desc == -1)
    {
        perror("Failed to create socket");
        return 1;
    }
    puts("Socket created...");

    //Allow the port to be reused
    int reuse = 1; //mpromonet [stack overflow]
    if(setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");
    if(setsockopt(socket_desc, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse)) < 0) 
        perror("setsockopt(SO_REUSEPORT) failed");

    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(6810); //port
    
    //Bind port to socket
    uint tt = time(0);
    while(bind(socket_desc, (struct sockaddr*)&server, sizeof(server)) < 0)
    {
        //print the error message
        perror("bind error");
        printf("time taken: %lds\n", time(0)-tt);
        sleep(3);
    }
    printf("Bind took: %.2f minutes ...\n", (float)(time(0)-tt) / 60.0f);
    puts("Waiting for connections...");
    
    //Listen for connections
    listen(socket_desc, 512);

    //Never allow process to end
    uint reqs = 0;
    uint st = time(0);
    tt = time(0);
    char client_message[RECV_BUFF_SIZE];
    char psite[MIN_LEN];
    char pidfa[MIN_LEN];
    while(1)
    {  
        //Accept and incoming connection
        c = sizeof(struct sockaddr_in);
        client_sock = accept(socket_desc, (struct sockaddr *) &client, (socklen_t*) &c);
        if(client_sock < 0)
        {
            perror("accept failed");
            continue;
        }

        //Client Command
        memset(client_message, '\0', sizeof(client_message));
        read_size = recv(client_sock, client_message, RECV_BUFF_SIZE-1, 0);

        //Log Requests per Second
        if(st < time(0))
        {
            if(reqs != 0)
                printf("Req/s: %ld - Blocked: %llu - Allowed: %llu - Rejected: %llu - Buff Loops: %llu - Sites: %llu - Error: %llu\n", reqs / (time(0)-tt), block, allow, rej, reset, site_count, err);
            tt = time(0);
            reqs = 0;
            st = time(0)+180; //3 Min Display Interval
        }

        //Nothing from the client?
        if(read_size <= 0)
        {
            err++; //Transfer Error
            close(client_sock);
            continue;
        }
        reqs++;

        //Parse the message
        psite[0] = 0x00;
        pidfa[0] = 0x00;
        parseMsg(client_message, psite, pidfa);
        if(psite[0] == 0x00 || pidfa == 0x00)
        {
            err++; //Transfer error
            close(client_sock);
            continue;
        }
        //printf("%s %s\n", psite, pidfa);

        //Get site and idfa
        sre site, idfa;
        sre1(&site, psite);
        sre1(&idfa, pidfa);

        //Function switch
        if(client_message[0] != '$')
        {
            //Check if we have this idfa
            if(has_idfa(&site, &idfa) == 1)
            {
                if(write(client_sock, "y", 1) < 0) //Only respond if we have something
                    err++; //Connection Error
                else
                    rej++;
            }
            else
            {
                allow++;
            }
        }
        else
        {
            add_idfa(&site, &idfa);
            block++;
        }

        //Done
        if(close(client_sock) < 0)
            err++; //Connection Error
    }
    
    //Daemon
    return 0;
}


