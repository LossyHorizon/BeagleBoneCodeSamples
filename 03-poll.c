/***************************************************************************************************
* Created by JEP on Nov 10, 2016 testing of Linux poll() call                                      *
*                                                                                                  *
*                                                                                                  *
* gcc -Wall  -oprog03  03-poll.c                                                                   *
*                                                                                                  *
*  gcc -std=c99 -Wall  -oprog03  03-poll.c   breaks things                                         *
*     inet_aton() in arpa/inet.h does not get defined                                              *
*     See /usr/include/features.h  for more details                                                *
*     _SVID_SOURCE, _BSD_SOURCE, and _POSIX_SOURCE set to one might fix                            *
*                                                                                                  *
*                                                                                                  *
***************************************************************************************************/
#include <stdio.h>       // printf(), getline()
#include <stdlib.h>      // exit()
#include <string.h>      // memset(), strerror()
#include <sys/time.h>

extern int errno;        // required for sterror() usage

//  #define _GNU_SOURCE 

#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>   // inet_aton()
#include <unistd.h>      // close(), read(), write()
#include <errno.h>
#include <poll.h>        // poll(), ??
#include <signal.h>      // for Ctrl-C catch, signal(SIGINT, intHandler);

#include <stdbool.h>     // type bool, true, false

#define SERVER "127.0.0.1"
#define BUFLEN 512  //Max length of buffer
#define PORT 8888   //The port on which to send data
 
/***************************************************************************************************
* Globals.  myFD_Ctrl[] and fds[] are parallel arrays.  poll() call requires array fds[], to not   *
* go mad/write garbage code we require myFD_Ctrl[].  During the main loop numActiveFDs is the      *
* number of entries in the two arrays.                                                             *
*                                                                                                  *
*                                                                                                  *
***************************************************************************************************/
 
#define myMAX_FDs  10
struct pollfd fds[myMAX_FDs];
struct myFD_Ctrl {
    int handle;
    char RX_Buf[BUFLEN+1];
    char TX_Buf[BUFLEN+1];
    size_t rxLen, txLen;

    struct sockaddr_in si_other;
    size_t slen;
    size_t recv_len;

    void (*processInput) (int entry, bool timeOut);

    bool sockCloseRequired;
    char *name;
} myFD_Ctrl[myMAX_FDs];

int timeOutMilliSecs;
static volatile bool keepGoing;
typedef void (*PROCESS_INPUT) (int entry, bool timeOut);
int numActiveFDs = 0;  // except when there are no active entries, always points one past last entry
int entrySTDIN;
int entrySOCKET;

/***************************************************************************************************
*                                                                                                  *
***************************************************************************************************/
void intHandler (int dummy) {
    keepGoing = false;
}


/***************************************************************************************************
*                                                                                                  *
***************************************************************************************************/
void setupSocketEntry (char *name, int entry, PROCESS_INPUT func) {
    myFD_Ctrl[entry].name = name;

    if ( (myFD_Ctrl[entry].handle = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        printf ("%03d: ERROR: %s, socket() setup call failed, msg=%s \n", __LINE__, myFD_Ctrl[entry].name, strerror (errno));
        exit (5);
    }

    memset((char *) &myFD_Ctrl[entry].si_other, 0, sizeof (struct sockaddr_in));
    myFD_Ctrl[entry].si_other.sin_family = AF_INET;
    myFD_Ctrl[entry].si_other.sin_port = htons(PORT);
     
    if (inet_aton(SERVER , &myFD_Ctrl[entry].si_other.sin_addr) == 0) {
        printf ("%03d: ERROR: %s, inet_aton() failed, msg=%s \n", __LINE__, myFD_Ctrl[entry].name, strerror (errno));
        exit(1);
    }

    myFD_Ctrl[entry].sockCloseRequired = true;

    myFD_Ctrl[entry].slen = sizeof (struct sockaddr_in) ;
    myFD_Ctrl[entry].recv_len = 0;

    myFD_Ctrl[entry].processInput = func;

    fds[entry].fd = myFD_Ctrl[entry].handle;
    fds[entry].events = POLLIN;

    return;
}

/***************************************************************************************************
*                                                                                                  *
***************************************************************************************************/
void setupStreamEntry (char *name, int entry, int fd, PROCESS_INPUT func) {
    myFD_Ctrl[entry].name = name;
    myFD_Ctrl[entry].handle = fd;
    myFD_Ctrl[entry].recv_len = 0;
    myFD_Ctrl[entry].sockCloseRequired = false;

    myFD_Ctrl[entry].processInput = func;

    fds[entry].fd = myFD_Ctrl[entry].handle;
    fds[entry].events = POLLIN;

    return;
}


/***************************************************************************************************
*                                                                                                  *
***************************************************************************************************/
void readStdIn (int entry, bool timeOut) {
    ssize_t ret;
    char *s;

    if (timeOut) {
        // printf ("%03d: multitasking would go here\n", __LINE__);
        return;
    }

    // printf ("%3d: readStdIn() running", __LINE__);

    ret = read (myFD_Ctrl[entry].handle, myFD_Ctrl[entry].RX_Buf, BUFLEN-1);
    if (ret > 0) {
        myFD_Ctrl[entry].RX_Buf[ret] = '\0';
        s = strchr (myFD_Ctrl[entry].RX_Buf, '\n');
        if (s) *s = '\0';

        printf ("%3d: %s input %d: [%s]\n",  __LINE__, myFD_Ctrl[entry].name, ret, myFD_Ctrl[entry].RX_Buf);

    } else {
        printf ("%03d: ERROR: read error on %s\n",  __LINE__, myFD_Ctrl[entry].name);
        keepGoing = false;
        return;
    }

    printf ("%3d: readStdIn() complete, about to send message \n", __LINE__);

    //send the message
    if (sendto (myFD_Ctrl[entrySOCKET].handle, myFD_Ctrl[entrySTDIN].RX_Buf, strlen(myFD_Ctrl[entrySTDIN].RX_Buf), 0 , (struct sockaddr *) &myFD_Ctrl[entrySOCKET].si_other, myFD_Ctrl[entrySOCKET].slen) == -1) {
        printf ("%3d: ERROR: sendto() failed, msg=%s \n", __LINE__, strerror (errno));
        keepGoing = false;
        return;
    }

    return;
}

/***************************************************************************************************
*                                                                                                  *
***************************************************************************************************/
void readStreamIn (int entry, bool timeOut) {
    ssize_t ret;
    char *s;

    if (timeOut) {
        return;
    }

    // printf ("%3d: readStreamIn() running\n", __LINE__);

    ret = read (myFD_Ctrl[entry].handle, myFD_Ctrl[entry].RX_Buf, BUFLEN-1);
    if (ret > 0) {
        myFD_Ctrl[entry].RX_Buf[ret] = '\0';
        s = strchr (myFD_Ctrl[entry].RX_Buf, '\n');
        if (s) *s = '\0';

        printf ("%3d: %s input %d: [%s]\n",  __LINE__, myFD_Ctrl[entry].name, ret, myFD_Ctrl[entry].RX_Buf);

    } else if (ret == 0) {
        printf ("%03d: ERROR: no bytes\n",  __LINE__);

    } else {
        printf ("%03d: ERROR: read error on %s, msg=%s\n",  __LINE__, myFD_Ctrl[entry].name, strerror (errno));
        keepGoing = false;
    }

    return;
}



/***************************************************************************************************
*                                                                                                  *
***************************************************************************************************/
int main(void)
{
    int rc;
    int entry;
 
    printf ("%s starting.  Built on %s at %s\n", __FILE__, __DATE__, __TIME__);

    memset (myFD_Ctrl, 0, sizeof myFD_Ctrl);
    memset (fds, 0 , sizeof fds );
    numActiveFDs = 0;

    entrySTDIN = numActiveFDs;
    setupStreamEntry ("stdin", numActiveFDs++, 0, readStdIn);   // Standard input access

    entrySOCKET = numActiveFDs;
    setupSocketEntry ("socket", numActiveFDs++, readStreamIn);  // Socket setup

    timeOutMilliSecs = 100;

    keepGoing = true;
    signal(SIGINT, intHandler);
    while (keepGoing) {
        rc = poll(fds, numActiveFDs, timeOutMilliSecs);
        
        if (rc < 0) {
          perror("  poll() failed");
          return 88;
        }
    
        for (entry = 0; entry < numActiveFDs; entry++) {
            if (myFD_Ctrl[entry].processInput == NULL) {
                // This is a programmer error, abort to encourage them to fix it
                printf ("%03d: ERROR: processInput func was not defined for entry %d", __LINE__, entry);
                exit (5);
            }

            if (rc == 0) {
              myFD_Ctrl[entry].processInput (entry, true);  // poll timed out, let handler decide if they care
              continue;
            }

            if (fds[entry].revents == 0)
                continue;  // Not for us

            if (fds[entry].revents == POLLIN) {
                // printf ("%03d: About to call input handler for %s\n",  __LINE__, myFD_Ctrl[entry].name);
                myFD_Ctrl[entry].processInput (entry, false);
                continue;
            }

            printf ("%03d: ERROR: revents = %d\n",  __LINE__, fds[entry].revents);
        }
    }

    for (entry = 0; entry < numActiveFDs; entry++) {
        if (myFD_Ctrl[entry].sockCloseRequired)
            close(myFD_Ctrl[entry].handle);
    }

    return 0;
}
