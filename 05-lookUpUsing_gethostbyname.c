/***************************************************************************************************
* Convert host name to IP address using gethostbyname()     Nov 13, 2016                           *
*                                                                                                  *
* Copied from: http://www.binarytides.com/hostname-to-ip-address-c-sockets-linux/                  *
* On Nov 13, 2016 with various formatting changes                                                  *
* Turns out gethostbyname() is obsolete, we should use getaddrinfo()                               *
*                                                                                                  *
* To comiile: gcc -Wall  -oresolv   05-lookUpUsing_gethostbyname.c                                 *
*                                                                                                  *
***************************************************************************************************/
#include<stdio.h>        // printf
#include<string.h>       // memset
#include<stdlib.h>       // exit(0);
#include<sys/socket.h>
#include<errno.h>        // For errno - the error number
#include<netdb.h>        // hostent
#include<arpa/inet.h>
 
int hostname_to_ip(char *  , char *);
 
/***************************************************************************************************
*                                                                                                  *
***************************************************************************************************/
int main (int argc , char *argv[])
{
    printf ("%s starting.  Built on %s at %s\n", __FILE__, __DATE__, __TIME__);

    if (argc < 2)
    {
        printf ("%03d: ERROR: hostname to resolve  must be given. \n", __LINE__);
        exit(1);
    }
     
    char *hostname = argv[1];
    char ip[100];
     
    hostname_to_ip(hostname , ip);
    printf("%s resolved to %s" , hostname , ip);
     
    printf("\n");
     
    return 0;
}

 
/***************************************************************************************************
* Get ip from domain name                                                                          *
* WHOOPS: gethostbyname() is obsolete.  Should be using getaddrinfo()                              *
*                                                                                                  *
* Copied from: http://www.binarytides.com/hostname-to-ip-address-c-sockets-linux/                  *
*                                                                                                  *
* ATTENTION: this is a blocking func, should not be used in cooperative multitasking environment   *
***************************************************************************************************/
int hostname_to_ip(char * hostname , char* ip)
{
    struct hostent *he;
    struct in_addr **addr_list;
    int i;
         
    if ( (he = gethostbyname( hostname ) ) == NULL) {
        // get the host info
        printf ("%03d: ERROR:  gethostbyname() returned an error \n", __LINE__);
        herror("gethostbyname");   // hsterror() exists
        return 5;
    }
 
    addr_list = (struct in_addr **) he->h_addr_list;
     
    for (i = 0; addr_list[i] != NULL; i++) {
        // Return the first one;
        strcpy (ip , inet_ntoa(*addr_list[i]) );
        return 0;
    }
     
    printf ("%03d: ERROR: Unrecognized error in lookup\n", __LINE__);
    return 9;
}

