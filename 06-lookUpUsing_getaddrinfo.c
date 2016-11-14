/***************************************************************************************************
* Convert host name to IP address using getaddrinfo()   Nov 13, 2016                               *
*                                                                                                  *
* Copied from: http://www.logix.cz/michal/devel/various/getaddrinfo.c.xp                           *
* On Nov 13, 2016 with various formatting changes. Original by                                     *
*  Michal Ludvig <michal@logix.cz> (c) 2002, 2003, http://www.logix.cz/michal/devel/  public domain*
*                                                                                                  *
*                                                                                                  *
* To comiile: gcc -Wall  -oresolv   06-lookUpUsing_getaddrinfo.c                                   *
*                                                                                                  *
* This: http://man7.org/linux/man-pages/man3/getaddrinfo.3.html                                    *
*      Not only shows getaddrinfo() useage, but it supports a UDP socket.                          *
*      Apparently the correct usage pattern is to not only look up the IP, but to try to connect   *
***************************************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

/***************************************************************************************************
*                                                                                                  *
* ATTENTION: this is a blocking func, should not be used in cooperative multitasking environment   *
***************************************************************************************************/
char *lookup_host (char *foundIP, size_t bufSize, const char *host)
{
    struct addrinfo hints, *res;
    int errcode;
    void *ptr;

    memset (&hints, 0, sizeof (hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;   // SOCK_DGRAM is also possible
    hints.ai_flags |= AI_CANONNAME;

    errcode = getaddrinfo (host, NULL, &hints, &res);
    if (errcode != 0) {
        printf ("%03d: ERROR: getaddrinfo() returned an error, %s \n", __LINE__, gai_strerror(errcode));
        return "";
    }

    printf ("%03d: Host: %s \n", __LINE__, host);
    while (res) {
        inet_ntop (res->ai_family, res->ai_addr->sa_data, foundIP, bufSize);

        switch (res->ai_family) {
          case AF_INET:
            ptr = &((struct sockaddr_in *) res->ai_addr)->sin_addr;
            break;

          case AF_INET6:
            ptr = &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr;
            break;
        }

        inet_ntop (res->ai_family, ptr, foundIP, bufSize);
        printf ("%03d: IPv%d address: %s (%s) \n", __LINE__, res->ai_family == PF_INET6 ? 6 : 4, foundIP, res->ai_canonname);
        res = res->ai_next;
    }

    freeaddrinfo (res);
    return foundIP;
}

/***************************************************************************************************
*                                                                                                  *
***************************************************************************************************/
int main (int argc, char *argv[])
{
    char addrstr[100];

    printf ("%s starting.  Built on %s at %s\n", __FILE__, __DATE__, __TIME__);
    if (argc < 2) {
        printf ("%03d: ERROR: you must give an address to lookup \n", __LINE__);
        exit (1);
    }

    if (strlen (argv[1]) < 3) {
        printf ("%03d: ERROR: you must give an address to lookup \n", __LINE__);
        exit (1);
    }

    printf ("%03d: Found IP address: (%s) \n", __LINE__, lookup_host (addrstr, sizeof(addrstr) - 1, argv[1]));

    return 0;
}

