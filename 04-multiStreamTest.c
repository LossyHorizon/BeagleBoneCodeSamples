/***************************************************************************************************
* Created by JEP on Nov 11, 2016 testing of Linux poll() call                                      *
* This version reads the Digitial inputs, so now we have both key board and physical inputs        *
* Key goal was to have poll() behave correctly so we did not have to constantly run to see inputs  *
*                                                                                                  *
* gcc -Wall  -oprog04  04-multiStreamTest.c                                                        *
*                                                                                                  *
*  gcc -std=c99 -Wall  -oprog04  04-multiStreamTest.c    breaks things                             *
*     inet_aton() in arpa/inet.h does not get defined                                              *
*     See /usr/include/features.h  for more details                                                *
*     _SVID_SOURCE, _BSD_SOURCE, and _POSIX_SOURCE set to one might fix                            *
*                                                                                                  *
* NOTES:                                                                                           *
*   Manual commands GPIO using bash shell                                                          *
*   echo 66 > /sys/class/gpio/export   # Enable I/O port 66 (pin  7)                               *
*   echo 67 > /sys/class/gpio/export   # Enable I/O port 66 (pin  8)                               *
*                                                                                                  *
*   Set the two pin's 'interrupt' state (none, both, rising, falling)                              *
*   echo none >/sys/class/gpio/gpio66/edge ; echo none >/sys/class/gpio/gpio67/edge                *
*   echo both >/sys/class/gpio/gpio66/edge ; echo both >/sys/class/gpio/gpio67/edge                *
*                                                                                                  *
*   echo out >/sys/class/gpio/gpio45/direction  # Set for output                                   *
*   echo in  >/sys/class/gpio/gpio45/direction  # Set for input                                    *
*                                                                                                  *
*   cat /sys/class/gpio/gpio67/value    # Shows current state                                      *
*                                                                                                  *
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
#include <sys/types.h>   // open(), O_RDONLY | O_DIRECT
#include <sys/stat.h>    // open(), O_RDONLY | O_DIRECT
#include <fcntl.h>       // open(), O_RDONLY | O_DIRECT

#include <unistd.h>      // close(), read(), write()

#include <errno.h>       // strerror()
#include <poll.h>        // poll(), ??
#include <signal.h>      // for Ctrl-C catch, signal(SIGINT, intHandler);

#include <stdbool.h>     // type bool, true, false

#define BUFLEN 512  //Max length of buffer
#ifdef LOCAL_ONLY
    #define SERVER "127.0.0.1"
    #define PORT 8888   //The port on which to send data

#else
    #define SERVER "45.55.156.55"
    #define PORT 3033 
#endif


/***************************************************************************************************
* Globals.  myFD_Ctrl[] and fds[] are parallel arrays.  poll() call requires array fds[], to not   *
* go mad/write garbage code we require myFD_Ctrl[].  During the main loop numActiveFDs is the      *
* number of entries in the two arrays.  fds[X] and myFD_Ctrl[X] MUST always be in sync.            *
*                                                                                                  *
*                                                                                                  *
***************************************************************************************************/
 
#define MAX_FDS  20
struct pollfd fds[MAX_FDS];
struct myFD_Ctrl {
    int handle;
    char RX_Buf[BUFLEN+1];
    char TX_Buf[BUFLEN+1];
//  size_t rxLen, txLen;
    int msgNum;

    struct sockaddr_in si_other;
    size_t slen;      // Is this really effectivly a constant?
    size_t recv_len;  // for TCP sockets I have to deal with partial data on reads.  I will need this if that is an issue for UDP sockets

    void (*processInput) (int entry, bool timeOut);

    bool closeRequired;
    char *name;
} myFD_Ctrl[MAX_FDS];

int timeOutMilliSecs;
static volatile bool keepGoing;
int numActiveFDs = 0;  // except when there are no active entries, always points one past last entry
int entrySTDIN;        // Channel/entry for stdin (keyboard is assumed)
int entrySOCKET;       // Channel/entry for the socket

typedef void (*PROCESS_INPUT) (int entry, bool timeOut);

/***************************************************************************************************
*                                                                                                  *
***************************************************************************************************/
void intHandler (int dummy) {
    if (!keepGoing) {
        printf ("%03d: ERROR: intHandler() called twice, preforming HARD ABORT \n", __LINE__);

        /*
         * The program should be constantly running the main program loop, and the first time
         * we see Ctrl-C it should have seen it and quit.  But if a handler is stuck in an endless
         * loop then the program as a whole will just run forever.  In that situation when we get
         * a second Ctrl-C we abort from within this signal hander.  Other wise we can become an un-killable
         * process thanks to our trapping Ctrl-C.
         */
        int entry;
        for (entry = 0; entry < numActiveFDs; entry++) {
            if (myFD_Ctrl[entry].closeRequired)
                close(myFD_Ctrl[entry].handle);
        }

        exit (4);   
    }

    keepGoing = false;  // Tell main loop its time to shutdown
    return;
}


/***************************************************************************************************
* Meant to be used in decodeEventsMask() and no where else                                         *
*                                                                                                  *
***************************************************************************************************/
static void decodeEventsMask_Sub (char **dest, char *str) {
    while (*str) {
        *(*dest)++ = *str++;
    }

    *(*dest)++ = ',';
    *(*dest)++ = ' ';
    *(*dest)   = '\0';
}

#define EVT_TEST(buf, flags, mask)  if (flags & mask) { decodeEventsMask_Sub (buf, #mask); flags = flags & (~mask); }

/***************************************************************************************************
* Convert poll() event flag codes into human readable strings.  Intended for diagnostics only      *
*                                                                                                  *
***************************************************************************************************/
char *decodeEventsMask (unsigned int flags) {
    static char msgBuf[900];
    char *s;

    if (flags == 0) {
        strcpy (msgBuf, "none");
        return msgBuf;
    }

    s = msgBuf;

    EVT_TEST (&s, flags, POLLIN);
    EVT_TEST (&s, flags, POLLPRI);
    EVT_TEST (&s, flags, POLLOUT);
//  EVT_TEST (&s, flags, POLLRDHUP);
    EVT_TEST (&s, flags, POLLERR);
    EVT_TEST (&s, flags, POLLHUP);
    EVT_TEST (&s, flags, POLLNVAL);

    if (flags != 0) {
        sprintf (s, "unknown bits %04X, ", flags);
        s = s + strlen (s);
    }

    // Strip trailing space & comma
    *s-- = '\0';   // Silly, but cheaper than an if()
    *s-- = '\0';   // the trailing space
    *s-- = '\0';   // the trailing comma

    return msgBuf;
}


/***************************************************************************************************
*                                                                                                  *
***************************************************************************************************/
void sendMessageToSocket (char *msg, int senderEntryNum) {

    sprintf (myFD_Ctrl[entrySOCKET].TX_Buf, "=%02d@%s=", myFD_Ctrl[entrySOCKET].msgNum, msg);

    if (sendto (myFD_Ctrl[entrySOCKET].handle, myFD_Ctrl[entrySOCKET].TX_Buf, strlen(myFD_Ctrl[entrySOCKET].TX_Buf), 0 , (struct sockaddr *) &myFD_Ctrl[entrySOCKET].si_other, myFD_Ctrl[entrySOCKET].slen) == -1) {
        printf ("%3d: ERROR: sendto() failed, msg=%s \n", __LINE__, strerror (errno));
        printf ("%3d:        sendMessageToSocket() called for %s \n", __LINE__, myFD_Ctrl[senderEntryNum].name);
        keepGoing = false;

    } else {
        printf ("%3d: msg%d sent %s \n", __LINE__, myFD_Ctrl[entrySOCKET].msgNum, msg);
    }

    myFD_Ctrl[entrySOCKET].msgNum = myFD_Ctrl[entrySOCKET].msgNum < 99 ? myFD_Ctrl[entrySOCKET].msgNum + 1 : 1;
    return;
}



// -_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_
// -_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_
// -_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_
/***************************************************************************************************
*                                                                                                  *
***************************************************************************************************/
void setupSocketData (char *name, int entry, PROCESS_INPUT func) {
    myFD_Ctrl[entry].name = name;

    if ( (myFD_Ctrl[entry].handle = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
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

    myFD_Ctrl[entry].closeRequired = true;

    myFD_Ctrl[entry].slen = sizeof (struct sockaddr_in) ;
    myFD_Ctrl[entry].recv_len = 0;
    myFD_Ctrl[entry].msgNum = 1;

    myFD_Ctrl[entry].processInput = func;

    fds[entry].fd = myFD_Ctrl[entry].handle;
    fds[entry].events = POLLIN;

    return;
}


/***************************************************************************************************
*                                                                                                  *
***************************************************************************************************/
void readSocketData (int entry, bool timeOut) {
    ssize_t ret;
    char *s;

    if (timeOut) {
        return;
    }

    // printf ("%3d: readSocketData() running\n", __LINE__);

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



// -_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_
// -_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_
// -_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_
/***************************************************************************************************
*                                                                                                  *
***************************************************************************************************/
void setupStdIn (char *name, int entry, int fd, PROCESS_INPUT func) {
    myFD_Ctrl[entry].name = name;
    myFD_Ctrl[entry].handle = fd;
    myFD_Ctrl[entry].recv_len = 0;
    myFD_Ctrl[entry].closeRequired = false;

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

    // printf ("%3d: readStdIn() complete, about to send message \n", __LINE__);

    if (myFD_Ctrl[entry].RX_Buf[0] == 'q') {
        sendMessageToSocket ("quit command", entry);
        keepGoing = false;
        return;
    }

    sendMessageToSocket (myFD_Ctrl[entry].RX_Buf, entry);

    return;
}


// -_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_
// -_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_
// -_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_
/***************************************************************************************************
*                                                                                                  *
***************************************************************************************************/
void setupDigitialInput (char *name, int entry, PROCESS_INPUT func, int portNum) {
    int fd;
    char pathBuf[500];
    char cmd[500];
    int writeLen;
    struct stat sb;

    // ------------------ If /sys/* directory for this GPIO does not exist, tell the kernel to create it 
    sprintf (pathBuf, "/sys/class/gpio/gpio%d", portNum);
    if (stat(pathBuf, &sb) == 0 && S_ISDIR(sb.st_mode)) {
        printf ("%3d: %s gpio already enabled\n", __LINE__, name);

    } else {
        sprintf (pathBuf, "/sys/class/gpio/export");
        fd = open (pathBuf, O_WRONLY, 0666);
        if (fd == -1) {
            printf ("%3d: ERROR: open(%s) failed, msg=%s\n", __LINE__, pathBuf, strerror (errno));
            exit (6);
        }

        writeLen = sprintf (cmd, "%d", portNum);
        write (fd, cmd, (ssize_t)writeLen);
        close (fd);
    }

    // ------------------ Set this GPIO entry to detect both rising/falling edges
    sprintf (pathBuf, "/sys/class/gpio/gpio%d/edge", portNum);
    fd = open (pathBuf, O_WRONLY, 0666);
    if (fd == -1) {
        printf ("%3d: ERROR: open(%s) failed, msg=%s\n", __LINE__, pathBuf, strerror (errno));
        exit (6);
    }

    writeLen = sprintf (cmd, "both");
    write (fd, cmd, (ssize_t)writeLen);
    close (fd);

    // Input is the default, but here is where you could set the IO direction

    // ------------------ Open file that contains the actual GPIO state
    sprintf (pathBuf, "/sys/class/gpio/gpio%d/value", portNum);
    fd = open (pathBuf, O_RDONLY   /* | O_DIRECT */ );
    if (fd == -1) {
        printf ("%3d: ERROR: open(%s) failed, msg=%s\n", __LINE__, pathBuf, strerror (errno));
        exit (6);
    }
    
    printf ("%3d: setupDigitialInput() successfully opened [%s]\n", __LINE__, pathBuf);

    myFD_Ctrl[entry].name = name;
    myFD_Ctrl[entry].handle = fd;
    myFD_Ctrl[entry].recv_len = 0;
    myFD_Ctrl[entry].closeRequired = true;

    myFD_Ctrl[entry].processInput = func;

    fds[entry].fd = myFD_Ctrl[entry].handle;
    fds[entry].events = POLLPRI;

    return;
}


/***************************************************************************************************
*                                                                                                  *
***************************************************************************************************/
void readDigitalInput (int entry, bool timeOut) {
    ssize_t ret;
    char *s;

    if (timeOut) {
        return;
    }

    // printf ("%3d: readDigitalInput() running\n", __LINE__);

    lseek (myFD_Ctrl[entry].handle, 0, SEEK_SET);  // TODO: find out if I need this?
    ret = read (myFD_Ctrl[entry].handle, myFD_Ctrl[entry].RX_Buf, BUFLEN-1);
    if (ret > 0) {
        myFD_Ctrl[entry].RX_Buf[ret] = '\0';
        s = strchr (myFD_Ctrl[entry].RX_Buf, '\n');
        if (s) *s = '\0';

        printf ("%3d: %s input %d: [%s]\n",  __LINE__, myFD_Ctrl[entry].name, ret, myFD_Ctrl[entry].RX_Buf);

    } else if (ret == 0) {
        printf ("%03d: ERROR: on %s, no bytes, IGNORED\n",  __LINE__, myFD_Ctrl[entry].name);
        return;

    } else {
        printf ("%03d: ERROR: read error on %s, msg=%s, IGNORED\n",  __LINE__, myFD_Ctrl[entry].name, strerror (errno));
        return;
    }

    // Construct and send the message
    char msgBuf[500];
    int val;

    sscanf (myFD_Ctrl[entry].RX_Buf, "%d", &val);

    sprintf (msgBuf, "%s state %s", myFD_Ctrl[entry].name, val == 1 ? "on" : "off");

    sendMessageToSocket (msgBuf, entry);

    return;
}



// -_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_
// -_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_
// -_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_
// -_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_
// -_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_
// -_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_
/***************************************************************************************************
*                                                                                                  *
***************************************************************************************************/
int main(void)
{
    printf ("%s starting.  Built on %s at %s\n", __FILE__, __DATE__, __TIME__);

    // Strictly speaking we don't need these two (global variables are already zero), but 
    // someday if we malloc() these two arrays then this would be necessary
    memset (myFD_Ctrl, 0, sizeof myFD_Ctrl);
    memset (fds, 0 , sizeof fds );
    numActiveFDs = 0;

    /*
     * Setup the input channels/entries that we will be monitoring.  We do no error checking
     * to stop our selves from running past end of the arrays.  That's not necessaryly a great idea.
     * However a fancy program might want to simply reallocate the arrays instead of crashing.  Test
     * program does not need such fancy stuff.
     */
    entrySTDIN = numActiveFDs;
    setupStdIn ("stdin", numActiveFDs++, 0, readStdIn);   // Standard input access

    entrySOCKET = numActiveFDs;
    setupSocketData ("socket", numActiveFDs++, readSocketData);  // Socket setup

    setupDigitialInput ("Digitial66", numActiveFDs++, readDigitalInput, 66);
    setupDigitialInput ("Digitial67", numActiveFDs++, readDigitalInput, 67);

    // Core loop setup, then begin.  Loop runs forever until its time to shutdown
    int rc;
    int entry;
 
    // Set to 1000+ if debuging the loop
    // If no background tasks exist/wanted then set to a large number and make the code strictly responsive to input
    // Small values allow background tasks to run frequently.  Really fancy code will be constantly changing this.
    timeOutMilliSecs = 6000;

    keepGoing = true;   // May be set to false by intHandler() when it time to shutdown, possibly others as well
    signal (SIGINT, intHandler);

    while (keepGoing) {
        rc = poll (fds, numActiveFDs, timeOutMilliSecs);
        
        if (rc < 0) {
          printf ("%03d: ERROR: poll error, msg=%s, aborting\n",  __LINE__, strerror (errno));
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
                continue;  // not for this slot/entry

            if (fds[entry].revents & POLLIN) {
                // STDIN and sockets produce these events when they have input for us to process
                // printf ("%03d: About to call input handler for %s\n",  __LINE__, myFD_Ctrl[entry].name);
                myFD_Ctrl[entry].processInput (entry, false);
                continue;
            }

            if (fds[entry].revents & POLLPRI) {
                // The GPIO driver produces these when the input changes, I have no idea why
                // Some docs treat this behaviour as 'interrupt' driven I/O.  That would not be my personal phrasing
                // printf ("%03d: About to call input handler for %s\n",  __LINE__, myFD_Ctrl[entry].name);
                myFD_Ctrl[entry].processInput (entry, false);
                continue;
            }

            printf ("%03d: ERROR: revents = %s\n",  __LINE__, decodeEventsMask (fds[entry].revents) );
            if (fds[entry].revents & POLLERR) {
                // Generally when we have been here its pathalogical, so aborting makes sense
                printf ("%03d: ERROR: poll error %s, msg=%s, aborting\n",  __LINE__, myFD_Ctrl[entry].name, strerror (errno));
                exit (4);
            }
        }
    }

    // When we drop out of the main loop we are shutting down.  Clean up a few things.
    for (entry = 0; entry < numActiveFDs; entry++) {
        if (myFD_Ctrl[entry].closeRequired)
            close(myFD_Ctrl[entry].handle);
    }

    return 0;
}
