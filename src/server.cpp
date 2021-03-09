#include <iostream>
#include <set>
#include <algorithm>

#include <cstdio>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#define POLL_SIZE 2048

int setNonBlocking ( int fd )
{
    int flags;
#if defined ( O_NONBLOCK )
    if ( -1 == ( flags = fcntl( fd, F_GETFL, 0 ) ) )
        flags = 0;
    return fcntl( fd, F_SETFL, flags | O_NONBLOCK );
#else
    flags = 1;
    return ioctl( fd, FIOBIO, &flags );
#endif
}

int main()
{
    // #1 Create socket
    int masterSock = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    std::set< int > slaveSockets;
    
    // Initialize socket
    struct sockaddr_in sockAddr;
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_port = htons( 1234 );
    sockAddr.sin_addr.s_addr = htonl( INADDR_ANY );
    
    // #2 Bind socket
    bind( masterSock
        ,   reinterpret_cast< struct sockaddr * >( &sockAddr )
        ,   sizeof( sockAddr )
    );

    setNonBlocking( masterSock );

    // #3 Listen socket
    listen( masterSock, SOMAXCONN );

    struct pollfd fdSet[ POLL_SIZE ];
    fdSet[ 0 ].fd = masterSock;
    fdSet[ 0 ].events = POLLIN;

    // #4 Start conversation between server and clients
    while ( true )
    {
        unsigned index = 1;

        for ( auto const & elem : slaveSockets )
        {
            fdSet[ index ].fd = elem;
            fdSet[ index ].events = POLLIN;
            ++index;
        }

        unsigned fdSetSize = 1 + slaveSockets.size();

        poll( fdSet, fdSetSize, -1 );

        for ( unsigned int i = 0; i < fdSetSize; ++i )
        {
            if ( fdSet[ i ].revents & POLLIN )
            {
                // slave sockets
                if ( i )
                {
                    static char buffer[ 1024 ];
                    int recvSize = recv( fdSet[ i ].fd,
                        buffer,
                        sizeof( buffer ),
                        MSG_NOSIGNAL
                    );
                    
                    if ( !recvSize && errno != EAGAIN )
                    {
                        shutdown( fdSet[ i ].fd, SHUT_RDWR );
                        close( fdSet[ i ].fd );
                        slaveSockets.erase( fdSet[ i ].fd );
                    }
                    else if ( recvSize > 0 )
                    {
                        send( fdSet[ i ].fd, buffer, recvSize, MSG_NOSIGNAL );
                    }
                }
                // master sockets
                else
                {
                    int slaveSocket = accept( masterSock, 0, 0 );
                    setNonBlocking( slaveSocket );
                    slaveSockets.insert( slaveSocket );
                }
            }
        }
    }

    return 0;
}