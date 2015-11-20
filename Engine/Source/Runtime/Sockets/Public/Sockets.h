// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "IPAddress.h"
#include "SocketTypes.h"

/**
 * This is our abstract base class that hides the platform specific socket implementation
 */
class SOCKETS_API FSocket
{
protected:
	/** Indicates the type of socket this is */
	const ESocketType SocketType;

	/** Debug description of socket usage. */
	FString SocketDescription;

public:
	/** Default ctor */
	inline FSocket() :
		SocketType(SOCKTYPE_Unknown),
		SocketDescription(TEXT(""))
	{
	}

	/**
	 * Specifies the type of socket being created
	 *
	 * @param InSocketType the type of socket being created
	 * @param InSocketDescription the debug description of the socket
	 */
	inline FSocket(ESocketType InSocketType, const FString& InSocketDescription) :
		SocketType(InSocketType),
		SocketDescription(InSocketDescription)
	{
	}

	/**
	 * Virtual destructor
	 */
	virtual ~FSocket()
	{
	}

	/**
	 * Closes the socket
	 *
	 * @return true if it closes without errors, false otherwise
	 */
	virtual bool Close() = 0;

	/**
	 * Binds a socket to a network byte ordered address
	 *
	 * @param Addr the address to bind to
	 *
	 * @return true if successful, false otherwise
	 */
	virtual bool Bind(const FInternetAddr& Addr) = 0;

	/**
	 * Connects a socket to a network byte ordered address
	 *
	 * @param Addr the address to connect to
	 *
	 * @return true if successful, false otherwise
	 */
	virtual bool Connect(const FInternetAddr& Addr) = 0;

	/**
	 * Places the socket into a state to listen for incoming connections
	 *
	 * @param MaxBacklog the number of connections to queue before refusing them
	 *
	 * @return true if successful, false otherwise
	 */
	virtual bool Listen(int32 MaxBacklog) = 0;

	/**
	 * Queries the socket to determine if there is a pending connection
	 *
	 * @param bHasPendingConnection out parameter indicating whether a connection is pending or not
	 *
	 * @return true if successful, false otherwise
	 */
	virtual bool HasPendingConnection(bool& bHasPendingConnection) = 0;

	/**
	* Queries the socket to determine if there is pending data on the queue
	*
	* @param PendingDataSize out parameter indicating how much data is on the pipe for a single recv call
	*
	* @return true if the socket has data, false otherwise
	*/
	virtual bool HasPendingData(uint32& PendingDataSize) = 0;

	/**
	 * Accepts a connection that is pending
	 *
	 * @param		SocketDescription debug description of socket
	 * @return		The new (heap-allocated) socket, or NULL if unsuccessful.
	 */
	virtual class FSocket* Accept(const FString& InSocketDescription) = 0;

	/**
	 * Accepts a connection that is pending
	 *
	 * @param OutAddr the address of the connection
	 * @param		SocketDescription debug description of socket
	 *
	 * @return		The new (heap-allocated) socket, or NULL if unsuccessful.
	 */
	virtual class FSocket* Accept(FInternetAddr& OutAddr, const FString& InSocketDescription) = 0;

	/**
	 * Sends a buffer to a network byte ordered address
	 *
	 * @param Data the buffer to send
	 * @param Count the size of the data to send
	 * @param BytesSent out param indicating how much was sent
	 * @param Destination the network byte ordered address to send to
	 */
	virtual bool SendTo(const uint8* Data, int32 Count, int32& BytesSent, const FInternetAddr& Destination);

	/**
	 * Sends a buffer on a connected socket
	 *
	 * @param Data the buffer to send
	 * @param Count the size of the data to send
	 * @param BytesSent out param indicating how much was sent
	 */
	virtual bool Send(const uint8* Data, int32 Count, int32& BytesSent);

	/**
	 * Reads a chunk of data from the socket. Gathers the source address too
	 *
	 * @param Data the buffer to read into
	 * @param BufferSize the max size of the buffer
	 * @param BytesRead out param indicating how many bytes were read from the socket
	 * @param Source out param receiving the address of the sender of the data
	 * @param Flags the receive flags
	 */
	virtual bool RecvFrom(uint8* Data, int32 BufferSize, int32& BytesRead, FInternetAddr& Source, ESocketReceiveFlags::Type Flags = ESocketReceiveFlags::None);

	/**
	 * Reads a chunk of data from a connected socket
	 *
	 * @param Data the buffer to read into
	 * @param BufferSize the max size of the buffer
	 * @param BytesRead out param indicating how many bytes were read from the socket
	 * @param Flags the receive flags
	 */
	virtual bool Recv(uint8* Data, int32 BufferSize, int32& BytesRead, ESocketReceiveFlags::Type Flags = ESocketReceiveFlags::None);

	/**
	 * Blocks until the specified condition is met.
	 *
	 * @param Condition - The condition to wait for.
	 * @param WaitTime - The maximum time to wait.
	 *
	 * @return true if the condition was met, false if the time limit expired or an error occurred.
	 */
	virtual bool Wait (ESocketWaitConditions::Type Condition, FTimespan WaitTime) = 0;

	/**
	 * Determines the connection state of the socket
	 */
	virtual ESocketConnectionState GetConnectionState() = 0;

	/**
	 * Reads the address the socket is bound to and returns it
	 * 
	 * @param OutAddr address the socket is bound to
	 */
	virtual void GetAddress(FInternetAddr& OutAddr) = 0;

	/**
	 * Reads the address of the peer the socket is connected to
	 *
	 * @param OutAddr address of the peer the socket is connected to
	 * @return return true if the address was retrieved correctly 
	 */
	virtual bool GetPeerAddress(FInternetAddr& OutAddr) = 0;

	/**
	 * Sets this socket into non-blocking mode
	 *
	 * @param bIsNonBlocking whether to enable blocking or not
	 *
	 * @return true if successful, false otherwise
	 */
	virtual bool SetNonBlocking(bool bIsNonBlocking = true) = 0;

	/**
	 * Sets a socket into broadcast mode (UDP only)
	 *
	 * @param bAllowBroadcast whether to enable broadcast or not
	 *
	 * @return true if successful, false otherwise
	 */
	virtual bool SetBroadcast(bool bAllowBroadcast = true) = 0;

	/**
	 * Joins this socket to the specified multicast group.
	 *
	 * The multicast group address must be in the range 224.0.0.0 to 239.255.255.255.
	 *
	 * @param GroupAddress - The IP address of the multicast group.
	 *
	 * @return true on success, false otherwise.
	 *
	 * @see LeaveMulticastGroup
	 * @see SetMulticastLoopback
	 * @see SetMulticastTtl
	 */
	virtual bool JoinMulticastGroup (const FInternetAddr& GroupAddress) = 0;

	/**
	 * Removes this UDP client from the specified multicast group.
	 *
	 * @param The multicast group address to leave.
	 *
	 * @return true on success, false otherwise.
	 *
	 * @see JoinMulticastGroup
	 * @see SetMulticastLoopback
	 * @see SetMulticastTtl
	 */
	virtual bool LeaveMulticastGroup (const FInternetAddr& GroupAddress) = 0;

	/**
	 * Enables or disables multicast loopback on the socket (UDP only).
	 *
	 * This setting determines whether multicast datagrams are looped
	 * back to the sending socket. By default, multicast loopback is
	 * enabled. It must be enabled if more than one listener is present
	 * on a host.
	 *
	 * @param bLoopback - Whether loopback should be enabled.
	 *
	 * @see LeaveMulticastGroup
	 * @see JoinMulticastGroup
	 * @see SetMulticastTtl
	 */
	virtual bool SetMulticastLoopback (bool bLoopback) = 0;

	/**
	 * Sets the time to live (TTL) for multicast datagrams.
	 *
	 * The default TTL for multicast datagrams is 1, which prevents them
	 * from being forwarded beyond the local subnet. Higher values will
	 * allow multicast datagrams to be sent into neighboring subnets, if
	 * multicast capable routers are present.
	 *
	 * @param TimeToLive - Number of hops the datagram can make.
	 *
	 * @see LeaveMulticastGroup
	 * @see JoinMulticastGroup
	 * @see SetMulticastLoopback
	 */
	virtual bool SetMulticastTtl (uint8 TimeToLive) = 0;

	/**
	 * Sets whether a socket can be bound to an address in use
	 *
	 * @param bAllowReuse whether to allow reuse or not
	 *
	 * @return true if the call succeeded, false otherwise
	 */
	virtual bool SetReuseAddr(bool bAllowReuse = true) = 0;

	/**
	 * Sets whether and how long a socket will linger after closing
	 *
	 * @param bShouldLinger whether to have the socket remain open for a time period after closing or not
	 * @param Timeout the amount of time to linger before closing
	 *
	 * @return true if the call succeeded, false otherwise
	 */
	virtual bool SetLinger(bool bShouldLinger = true, int32 Timeout = 0) = 0;

	/**
	 * Enables error queue support for the socket
	 *
	 * @param bUseErrorQueue whether to enable error queuing or not
	 *
	 * @return true if the call succeeded, false otherwise
	 */
	virtual bool SetRecvErr(bool bUseErrorQueue = true) = 0;

	/**
	 * Sets the size of the send buffer to use
	 *
	 * @param Size the size to change it to
	 * @param NewSize the out value returning the size that was set (in case OS can't set that)
	 *
	 * @return true if the call succeeded, false otherwise
	 */
	virtual bool SetSendBufferSize(int32 Size, int32& NewSize) = 0;

	/**
	 * Sets the size of the receive buffer to use
	 *
	 * @param Size the size to change it to
	 * @param NewSize the out value returning the size that was set (in case OS can't set that)
	 *
	 * @return true if the call succeeded, false otherwise
	 */
	virtual bool SetReceiveBufferSize(int32 Size, int32& NewSize) = 0;

	/**
	 * Reads the port this socket is bound to.
	 */ 
	virtual int32 GetPortNo() = 0;

	/**
	 * @return The type of protocol the socket is bound to
	 */
	FORCEINLINE ESocketType GetSocketType() const
	{
		return SocketType;
	}

	/**
	 * @return The debug description of the socket
	 */
	FORCEINLINE FString GetDescription() const
	{
		return SocketDescription;
	}
};