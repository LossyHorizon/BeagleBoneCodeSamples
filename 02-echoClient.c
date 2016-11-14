/*
*  Simple udp client
*  Created by JEP on Nov 10, 2016, using code from the URL below.  Various warnings cleaned up.
*  http://www.binarytides.com/programming-udp-sockets-c-linux/
*  
*  gcc -Wall  -o02Client 02-echoClient.c
*/
#include <stdio.h>       // printf(), getline()
#include <string.h>      // memset()
#include <stdlib.h>      // exit()
#include <unistd.h>      // close()
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER "127.0.0.1"
#define BUFLEN 512  //Max length of buffer
#define PORT 8888   //The port on which to send data
 
void die(char *s)
{
    perror(s);
    exit(1);
}
 
int main(void)
{
    struct sockaddr_in si_other;
    int sockHandle;
    size_t slen = sizeof si_other ;
    size_t recv_len;

    char buf[BUFLEN];
    char *message;
    size_t messageLen;
    char *s;
 
    if ( (sockHandle=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        die("socket");
    }
 
    message = (char *)malloc((BUFLEN+1) * sizeof(char));
    memset (message, 0, BUFLEN+1);

    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(PORT);
     
    if (inet_aton(SERVER , &si_other.sin_addr) == 0) 
    {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }
 
    while(1)
    {
        printf("Enter message : ");
        getline(&message, &messageLen, stdin);
        if ((s = strchr (message, '\n')) != NULL)
            *s = '\0';
         
        //send the message
        if (sendto(sockHandle, message, strlen(message) , 0 , (struct sockaddr *) &si_other, slen)==-1)
        {
            die("sendto()");
        }
         
        //receive a reply and print it
        //clear the buffer by filling null, it might have previously received data
        memset(buf,'\0', BUFLEN);
        //try to receive some data, this is a blocking call
        if (recvfrom(sockHandle, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &recv_len) == -1)
        {
            die("recvfrom()");
        }
         
        printf ("RX %d: [%s]\n", recv_len, buf);
    }
 
    close(sockHandle);
    return 0;
}
