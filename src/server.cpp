#include <iostream>
#include <set> // TODO: Remove
#include <algorithm>

#include <cstdio>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>

#define MAX_EVENTS 32 // TODO: std::size_t constexpr s_maxEventsSize = 1 << 5;

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
    int masterSock = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP ); // TODO: Rename to "masterSocket"
    std::set< int > slaveSockets; // TODO: Remove
    
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

    int ePoll = epoll_create1( 0 );

    struct epoll_event event;
    event.data.fd = masterSock;
    event.events = EPOLLIN;
    epoll_ctl( ePoll, EPOLL_CTL_ADD, masterSock, &event );

    // #4 Start conversation between server and clients
    while ( true )
    {
        struct epoll_event events[ MAX_EVENTS ];
        int N = epoll_wait( ePoll, events, MAX_EVENTS, -1 );

        for ( unsigned i = 0; i < N; ++i )
        {
            if ( events[ i ].data.fd == masterSock )
            {
                int slaveSocket = accept( masterSock, 0, 0 );
                setNonBlocking( slaveSocket );
                struct epoll_event inLoopEvent;
                inLoopEvent.data.fd = slaveSocket;
                inLoopEvent.events = EPOLLIN;
                epoll_ctl( ePoll, EPOLL_CTL_ADD, slaveSocket, &inLoopEvent );
            }
            else
            {
                static char buffer[ 1024 ];
                int recvSize = recv( events[ i ].data.fd,
                    buffer,
                    sizeof( buffer ),
                    MSG_NOSIGNAL
                );

                if ( !recvSize && errno != EAGAIN )
                {
                    shutdown( events[ i ].data.fd, SHUT_RDWR );
                    close( events[ i ].data.fd );
                }
                else if ( recvSize > 0 )
                {
                    std::cout << "Sending \"" << std::string( buffer, buffer + recvSize )
                              << "\" to " << events[ i ].data.fd << std::endl;
                    send( events[ i ].data.fd,
                        buffer,
                        recvSize,
                        MSG_NOSIGNAL
                    );
                }
            }
        }
    }

    return 0;
}