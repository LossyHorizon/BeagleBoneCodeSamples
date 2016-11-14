/*
*  Simple udp server
*  Created by JEP on Nov 10, 2016, using code from the URL below.  Various warnings cleaned up.
*  http://www.binarytides.com/programming-udp-sockets-c-linux/
*  
*  gcc -Wall  -o01Server 01-echoServer.c
*  gcc -Wall  -oServer01 01-echoServer.c
*/
#include <stdio.h>       // printf()
#include <string.h>      // memset()
#include <stdlib.h>      // exit()
#include <unistd.h>      // close()
#include <arpa/inet.h>
#include <sys/socket.h>
 
#define BUFLEN 512  // Max length of buffer
#define PORT 8888   // The port on which to listen for incoming data
 
void die (char *s)
{
    perror(s);
    exit(1);
}
 
int main(void)
{
    struct sockaddr_in si_me, si_other;
     
    char buf[BUFLEN+1];
    char *s;
    int sockHandle, recv_len;
    size_t slen = sizeof si_other ;
     
    printf ("%s built on %s at %s\n", __FILE__, __DATE__, __TIME__);

    memset (buf, 0, sizeof buf);

    // create a UDP socket
    if ((sockHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        die("socket");
    }
     
    // zero out the structure
    memset((char *) &si_me, 0, sizeof(si_me));
     
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(PORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
     
    // bind socket to port
    if( bind(sockHandle , (struct sockaddr*)&si_me, sizeof(si_me) ) == -1)
    {
        die("bind");
    }
     
    // keep listening for data
    while (1)
    {
        printf("Waiting for data...");
        fflush(stdout);
         
        //try to receive some data, this is a blocking call
        if ((recv_len = recvfrom(sockHandle, buf, BUFLEN-1, 0, (struct sockaddr *) &si_other, &slen)) == -1)
        {
            die("recvfrom()");
        }
         
        buf[recv_len] = '\0';     // Make sure string is NULL terminated
        s = strchr (buf, '\n');   // Strip trailing line feed if present
        if (s) *s = '\0';

        //print details of the client/peer and the data received
        printf("Received packet from %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
        printf("Data: [%s]\n" , buf);
         
        //now reply the client with the same data
        if (sendto(sockHandle, buf, recv_len, 0, (struct sockaddr*) &si_other, slen) == -1)
        {
            die("sendto()");
        }
    }
 
    close(sockHandle);
    return 0;
}
