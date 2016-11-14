# BeagleBoneCodeSamples
Some Test programs I have created for the BeagleBoneBlack embeded computer.  
I have rather limited experience with contributing to Github, please be patient with me.

**noBlue.sh** : I needed a way to turn off the blue LED's on the board off when I was sleeping, back on during the day, this script does that.

**01-echoServer.c** : A simple UDP server I found on a web site given in the source code.  I added a header, made a few small changes.  Don't plan to update, it does the little bit that I needed.

**02-echoClient.c** : A simple UDB client, found on the same site.  Again just header changes and minor syntax work.

**03-poll.c** : First successful pass at using the poll() call, completely standalone. Works with STDIN and the UDP socket alone.  However once I tried to add GPIO input there were problems.

**04-multiStreamTest.c** : Now I can read the GPIO's, and show them, send messages to the server (coded as a client).  554 lines.

**07-multiStreamWithNameLookup.c** : Some bug fixes to 04-multiStreamTest.c, send of messages moved to a function, and most importantly the server can be specified as a name, not just bare IP address.  My compliance with IP6 is un-clear to me at the moment.  If you see anything you want fixed for this, email/send a pull request.  This will become the basis of the next step, however it will be split into multile files, 600+ lines is getting too big.

**05-lookUpUsing_gethostbyname.c** : First attempt to lookup a host name.  While trying to clean up the error message handling I found out that gethostbyname() is consdered obsolete.  This works, but has no future.

**06-lookUpUsing_getaddrinfo.c** : Second attempt to lookup a host name.  Using more modern getaddrinfo().  Routine in here went into 07-multiStreamWithNameLookup.c.  Simple name lookup, nothing very fancy.

