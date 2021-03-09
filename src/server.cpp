#include <iostream>
#include <set>
#include <algorithm>

#include <cstdio>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

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

    // #4 Start conversation between server and clients
    while ( true )
    {
        fd_set fdSet;

        FD_ZERO( &fdSet );
        FD_SET( masterSock, &fdSet );
        
        for ( auto const & elem : slaveSockets )
            FD_SET( elem, &fdSet );

        int maxElem = std::max( masterSock,
            *std::max_element( slaveSockets.begin(), slaveSockets.end() )
        );

        select( maxElem + 1, &fdSet, 0, 0, 0 );

        for ( auto const & elem : slaveSockets )
        {
            if ( FD_ISSET( elem, &fdSet ) )
            {
                static char buffer[ 1024 ];

                int recvSize = recv( elem, buffer, 1024, MSG_NOSIGNAL );

                if ( !recvSize && ( errno != EAGAIN ) )
                {
                    shutdown( elem, SHUT_RDWR );
                    close( elem );
                    slaveSockets.erase( elem );
                }
                else if ( recvSize )
                {
                    send( elem, buffer, recvSize, MSG_NOSIGNAL );
                }
            }
        }

        if ( FD_ISSET( masterSock, &fdSet ) )
        {
            int slaveSocket = accept( masterSock, nullptr, 0 );
            setNonBlocking( slaveSocket );
            slaveSockets.insert( slaveSocket );
        }
    }

    return 0;
}