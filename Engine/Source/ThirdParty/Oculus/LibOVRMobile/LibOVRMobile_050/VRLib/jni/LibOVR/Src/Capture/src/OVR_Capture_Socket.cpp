/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_Capture_Socket.cpp
Content     :   Misc network communication functionality.
Created     :   January, 2015
Notes       : 

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "OVR_Capture_Socket.h"

#include <string.h> // memset

namespace OVR
{
namespace Capture
{


    SocketAddress SocketAddress::Any(UInt16 port)
    {
        SocketAddress addr;
        addr.m_addr.sin_family      = AF_INET;
        addr.m_addr.sin_addr.s_addr = INADDR_ANY;
        addr.m_addr.sin_port        = htons(port);
        return addr;
    }

    SocketAddress SocketAddress::Broadcast(UInt16 port)
    {
        SocketAddress addr;
        addr.m_addr.sin_family      = AF_INET;
        addr.m_addr.sin_addr.s_addr = INADDR_BROADCAST;
        addr.m_addr.sin_port        = htons(port);
        return addr;
    }

    SocketAddress::SocketAddress(void)
    {
        memset(&m_addr, 0, sizeof(m_addr));
    }


    Socket *Socket::Create(Type type)
    {
        Socket *newSocket = NULL;
    #if defined(OVR_CAPTURE_POSIX)
        int sdomain   = 0;
        int stype     = 0;
        int sprotocol = 0;
        switch(type)
        {
            case Type_Stream:
                sdomain   = AF_INET;
                stype     = SOCK_STREAM;
                sprotocol = IPPROTO_TCP;
                break;
            case Type_Datagram:
                sdomain   = AF_INET;
                stype     = SOCK_DGRAM;
                sprotocol = 0;
                break;
        }
        const int s = ::socket(sdomain, stype, sprotocol);
        OVR_CAPTURE_ASSERT(s != s_invalidSocket);
        if(s != s_invalidSocket)
        {
        #if defined(SO_NOSIGPIPE)
            // Disable SIGPIPE on close...
            const UInt32 value = 1;
            ::setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &value, sizeof(value));
        #endif
            // Create the Socket object...
            newSocket = new Socket();
            newSocket->m_socket = s;
        }
    #else
        #error Unknown Platform!
    #endif
        return newSocket;
    }

    void Socket::Release(void)
    {
        delete this;
    }

    bool Socket::SetBroadcast(void)
    {
    #if defined(OVR_CAPTURE_POSIX)
        const UInt32 broadcast = 1;
        return ::setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) == 0;
    #else
        #error Unknown Platform!
    #endif
    }

    bool Socket::Bind(const SocketAddress &addr)
    {
    #if defined(OVR_CAPTURE_POSIX)
        return ::bind(m_socket, (const sockaddr*)&addr.m_addr, sizeof(addr.m_addr)) == 0;
    #else
        #error Unknown Platform!
    #endif
    }

    bool Socket::Listen(UInt32 maxConnections)
    {
    #if defined(OVR_CAPTURE_POSIX)
        return ::listen(m_socket, maxConnections) == 0;
    #else
        #error Unknown Platform!
    #endif
    }

    Socket *Socket::Accept(SocketAddress &addr)
    {
    #if defined(OVR_CAPTURE_POSIX)
        socklen_t addrlen = sizeof(addr.m_addr);

        // Wait for connection... or socket shutdown...
        fd_set readfd;
        FD_ZERO(&readfd);
        FD_SET(m_socket,   &readfd);
        FD_SET(m_recvPipe, &readfd);
        const int sret = ::select((m_socket>m_recvPipe?m_socket:m_recvPipe)+1, &readfd, NULL, NULL, NULL);

        // On error or timeout, abort!
        if(sret <= 0)
            return NULL;

        // If we have been signaled to abort... then don't even try to accept...
        if(FD_ISSET(m_recvPipe, &readfd))
            return NULL;

        // If the signal wasn't from the socket (WTF?)... then don't even try to accept...
        if(!FD_ISSET(m_socket, &readfd))
            return NULL;

        // Finally... accept the pending socket that we know for a fact is pending...
        Socket *newSocket = NULL;
        int s = ::accept(m_socket, (sockaddr*)&addr.m_addr, &addrlen);
        if(s != s_invalidSocket)
        {
        #if defined(SO_NOSIGPIPE)
            // Disable SIGPIPE on close...
            const UInt32 value = 1;
            ::setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &value, sizeof(value));
        #endif
            // Create the Socket object...
            newSocket = new Socket();
            newSocket->m_socket = s;
        }
    #else
        #error Unknown Platform!
    #endif
        return newSocket;
    }

    void Socket::Shutdown(void)
    {
    #if defined(OVR_CAPTURE_POSIX)
        ::shutdown(m_socket, SHUT_RDWR);
        // By writing into m_sendPipe and never reading from m_recvPipe, we are forcing all blocking select()
        // calls to return that m_recvPipe is ready for reading...
        if(m_sendPipe != -1)
        {
            const unsigned int dummy = 0;
            ::write(m_sendPipe, &dummy, sizeof(dummy));
        }
    #else
        #error Unknown Platform!
    #endif
    }

    bool Socket::Send(const void *buffer, UInt32 bufferSize)
    {
        OVR_CAPTURE_CPU_ZONE(Socket_Send);
    #if defined(OVR_CAPTURE_POSIX)
        const char  *bytesToSend    = (const char*)buffer;
        int          numBytesToSend = (int)bufferSize;
        while(numBytesToSend > 0)
        {
            int flags = 0;
        #if defined(MSG_NOSIGNAL)
            flags = MSG_NOSIGNAL;
        #endif
            const int numBytesSent = ::send(m_socket, bytesToSend, numBytesToSend, flags);
            if(numBytesSent >= 0)
            {
                bytesToSend    += numBytesSent;
                numBytesToSend -= numBytesSent;
            }
            else
            {
                // An error occured... just shutdown the socket because after this we are in an invalid state...
                Shutdown();
                return false;
            }    
        }
        return true;
    #else
        #error Unknown Platform!
    #endif
    }

    bool Socket::SendTo(const void *buffer, UInt32 size, const SocketAddress &addr)
    {
    #if defined(OVR_CAPTURE_POSIX)
        return ::sendto(m_socket, buffer, size, 0, (const sockaddr*)&addr.m_addr, sizeof(addr.m_addr)) != -1;
    #else
        #error Unknown Platform!
    #endif
    }

    UInt32 Socket::Receive(void *buffer, UInt32 size)
    {
    #if defined(OVR_CAPTURE_POSIX)
        const ssize_t r = ::recv(m_socket, buffer, (size_t)size, 0);
        return r>0 ? (UInt32)r : 0;
    #else
        #error Unknown Platform!
    #endif
    }

    Socket::Socket(void)
    {
    #if defined(OVR_CAPTURE_POSIX)
        m_socket   = s_invalidSocket;
        m_sendPipe = -1;
        m_recvPipe = -1;
        int p[2];
        if(::pipe(p) == 0)
        {
            m_recvPipe = p[0];
            m_sendPipe = p[1];
        }
    #else
        #error Unknown Platform!
    #endif
    }

    Socket::~Socket(void)
    {
    #if defined(OVR_CAPTURE_POSIX)
        if(m_socket != s_invalidSocket)
        {
            Shutdown();
            ::close(m_socket);
        }
        if(m_recvPipe != -1) ::close(m_recvPipe);
        if(m_sendPipe != -1) ::close(m_sendPipe);
    #else
        #error Unknown Platform!
    #endif
    }


    ZeroConfigHost *ZeroConfigHost::Create(UInt16 udpPort, UInt16 tcpPort, const char *packageName)
    {
        OVR_CAPTURE_ASSERT(udpPort > 0);
        OVR_CAPTURE_ASSERT(tcpPort > 0); 
        OVR_CAPTURE_ASSERT(udpPort != tcpPort);
        OVR_CAPTURE_ASSERT(packageName);
        Socket *broadcastSocket = Socket::Create(Socket::Type_Datagram);
        if(broadcastSocket && !broadcastSocket->SetBroadcast())
        {
            broadcastSocket->Release();
            broadcastSocket = NULL;
        }
        if(broadcastSocket)
        {
            return new ZeroConfigHost(broadcastSocket, udpPort, tcpPort, packageName);
        }
        return NULL;
    }

    void ZeroConfigHost::Release(void)
    {
        delete this;
    }

    ZeroConfigHost::ZeroConfigHost(Socket *broadcastSocket, UInt16 udpPort, UInt16 tcpPort, const char *packageName)
    {
        m_socket             = broadcastSocket;
        m_udpPort            = udpPort;
        m_packet.magicNumber = m_packet.s_magicNumber;
        m_packet.tcpPort     = tcpPort;
        strcpy(m_packet.packageName, packageName);
    }

    ZeroConfigHost::~ZeroConfigHost(void)
    {
        QuitAndWait();
        if(m_socket)
            m_socket->Release();
    }

    void ZeroConfigHost::OnThreadExecute(void)
    {
        const SocketAddress addr = SocketAddress::Broadcast(m_udpPort);
        while(!QuitSignaled())
        {
            if(!m_socket->SendTo(&m_packet, sizeof(m_packet), addr))
            {
                // If an error occurs, just abort the broadcast...
                break;
            }
            sleep(1); // sleep for 1 second before the next broadcast...
        }
        m_socket->Shutdown();
    }

} // namespace Capture
} // namespace OVR
