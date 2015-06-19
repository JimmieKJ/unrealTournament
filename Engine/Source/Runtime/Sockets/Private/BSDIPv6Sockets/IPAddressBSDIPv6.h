// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#if PLATFORM_HAS_BSD_IPV6_SOCKETS

#include "Core.h"
#include "IPAddress.h"

/**
 * Represents an internet ip address, using the relatively standard sockaddr_in6 structure. All data is in network byte order
 */
class FInternetAddrBSDIPv6 : public FInternetAddr
{
	/** The internet ip address structure */
	sockaddr_in6 Addr;

	/** Horrible hack to catch hard coded multicasting on IPv4 **/
	static const uint32 IPv4MulitcastAddr = ((230 << 24) | (0 << 16) | (0 << 8) | (1 << 0));

public:
	/**
	 * Constructor. Sets address to default state
	 */
	FInternetAddrBSDIPv6()
	{
		FMemory::Memzero(&Addr,sizeof(Addr));
		Addr.sin6_family = AF_INET6;
	}

	/**
	 * Sets the ip address from a host byte order uint32, convert the IPv4 address supplied to an IPv6 address
	 *
	 * @param InAddr the new address to use (must convert to network byte order)
	 */
	virtual void SetIp(uint32 InAddr) override
	{
		if(InAddr == 0)
		{
			FMemory::Memzero(&Addr.sin6_addr,sizeof(Addr.sin6_addr));
		}
		else if(InAddr == IPv4MulitcastAddr)
		{
			// if it's the hardcoded IPv4 multicasting address then translate into an IPv6 multicast address
			bool isValid;
			SetIp(L"ff02::2", isValid);
			check(isValid);
		}
		else
		{
			in_addr InternetAddr;
			InternetAddr.s_addr= htonl(InAddr);

			SetIp(InternetAddr);
		}
	}

	/**
	 * Sets the ip address from a string ("[aaaa:bbbb:cccc:dddd:eeee:ffff:gggg:hhhh]:port" or "a.b.c.d:port")
	 *
	 * @param InAddr the string containing the new ip address to use
	 * @param bIsValid will be set to true if InAddr was a valid IPv6 or IPv4 address, false if not.
	 */
	virtual void SetIp(const TCHAR* InAddr, bool& bIsValid) override
	{

		// Count colons to determine IP address type
		int NumColons = 0;
		int LastColonIndex = 0;
		int i = 0;
		while(InAddr && InAddr[i] != '\0')
		{
			if(InAddr[i] == ':')
			{
				NumColons++;
				LastColonIndex = i;
			}
			i++;
		}

		// Break off port portion of string
		FString AddressString(InAddr);
		FString Port;
		if(NumColons == 1 || NumColons == 8)
		{
			Port = AddressString.RightChop(LastColonIndex+1);
			AddressString = AddressString.Left(LastColonIndex);
			SetPort(FCString::Atoi(*Port));
		}

		// IPv6 URLs are surrounded by square brackets, check for that.
		AddressString.RemoveFromStart(FString("["));
		AddressString.RemoveFromEnd(FString("]"));

		// check for valid IPv6 address
		auto InAddrAnsi = StringCast<ANSICHAR>(*AddressString);
		if (inet_pton(AF_INET6, InAddrAnsi.Get(), &Addr.sin6_addr))
		{
			bIsValid = true;
			return;
		}

		// Check if it's a valid IPv4 address, and if it is convert
		in_addr  IPv4Addr;
		if (inet_pton(AF_INET, InAddrAnsi.Get(), &IPv4Addr))
		{
			bIsValid = true;
			SetIp(IPv4Addr);
		}
		else
		{
			bIsValid = false;
		}
	}

	/**
	 * Sets the ip address using a network byte order ipv4 address
	 *
	 * @param IPv4Addr the new ip address to use
	 */	
	void SetIp(const in_addr& IPv4Addr)
	{
		FMemory::Memzero(&Addr.sin6_addr,sizeof(Addr.sin6_addr));

		// special mapping of ipv4 to ipv6 using a hybrid stack, won't work on a pure ipv6 implementation
		uint8	IPv4b1 = (static_cast<uint32>(IPv4Addr.s_addr) & 0xFF),
				IPv4b2 = ((static_cast<uint32>(IPv4Addr.s_addr) >> 8) & 0xFF),
				IPv4b3 = ((static_cast<uint32>(IPv4Addr.s_addr) >> 16) & 0xFF),
				IPv4b4 = ((static_cast<uint32>(IPv4Addr.s_addr) >> 24) & 0xFF);
		
		Addr.sin6_addr.s6_addr[10] = 0xff;
		Addr.sin6_addr.s6_addr[11] = 0xff;
		Addr.sin6_addr.s6_addr[12] = IPv4b1;
		Addr.sin6_addr.s6_addr[13] = IPv4b2;
		Addr.sin6_addr.s6_addr[14] = IPv4b3;
		Addr.sin6_addr.s6_addr[15] = IPv4b4;

		UE_LOG(LogSockets, Log, TEXT("Using IPv4 address: %d.%d.%d.%d  on an ipv6 socket"), IPv4b1, IPv4b2, IPv4b3, IPv4b4);
	}

	/**
	 * Sets the ip address using a network byte order ipv6 address
	 *
	 * @param IpAddr the new ip address to use
	 */
	void SetIp(const in6_addr& IpAddr)
 	{
 		Addr.sin6_addr = IpAddr;
 	}

	/**
	 * Sets the ip address using a generic sockaddr_storage
	 *
	 * @param IpAddr the new ip address to use
	 */
	void SetIp(const sockaddr_storage& IpAddr)
	{
		if (IpAddr.ss_family == AF_INET)
		{
			const sockaddr_in* SockAddr = (const sockaddr_in*)&IpAddr;
			SetIp(SockAddr->sin_addr);
		}
		else if (IpAddr.ss_family == AF_INET6)
		{
			const sockaddr_in6* SockAddr = (const sockaddr_in6*)&IpAddr;
			SetIp(SockAddr->sin6_addr);
		}
	}

	/**
	 * Copies the network byte order ip address to a host byte order dword, doesn't exist with IPv6
	 *
	 * @param OutAddr the out param receiving the ip address
	 */
	virtual void GetIp(uint32& OutAddr) const override
	{
		// grab the last 32-bits of the IPv6 address as this will correspond to the IPv4 address
		// in a dual stack system.
		// This function doesn't really make sense in IPv6, but too much other code relies on it
		// existing to not have this here.

		//OutAddr = (Addr.sin6_addr.u.Word[6] << 16) & (Addr.sin6_addr.u.Word[7]);
		// FIXME [RCL]: original code had & here (see commented line above), which looks like a bug. 
		OutAddr = (Addr.sin6_addr.s6_addr[15] << 24) | (Addr.sin6_addr.s6_addr[14] << 16) | (Addr.sin6_addr.s6_addr[13] << 8) | (Addr.sin6_addr.s6_addr[12]);
	}
#if 0
	/**
	 * Copies the network byte order ip address 
	 *
	 * @param OutAddr the out param receiving the ip address
	 */
	void GetIp(in6_addr& OutAddr) const
 	{
 		OutAddr = Addr.sin6_addr;
 	}
#endif
	/**
	 * Sets the port number from a host byte order int
	 *
	 * @param InPort the new port to use (must convert to network byte order)
	 */
	virtual void SetPort(int32 InPort) override
	{
		Addr.sin6_port = htons(InPort);
	}

	/**
	 * Copies the port number from this address and places it into a host byte order int
	 *
	 * @param OutPort the host byte order int that receives the port
	 */
	virtual void GetPort(int32& OutPort) const override
	{
		OutPort = ntohs(Addr.sin6_port);
	}

	/** Returns the port number from this address in host byte order */
	virtual int32 GetPort() const override
	{
		return ntohs(Addr.sin6_port);
	}

	/** Sets the address to be any address */
	virtual void SetAnyAddress() override
	{
		SetIp(in6addr_any);
		SetPort(0);
	}

	/** Sets the address to broadcast */
	virtual void SetBroadcastAddress() override
	{
		// broadcast means something different in IPv6, but this is a rough equivalent
#ifndef in6addr_allnodesonlink
		// see RFC 4291, link-local multicast address http://tools.ietf.org/html/rfc4291
		static in6_addr in6addr_allnodesonlink =
		{
			{ { 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 } }
		};
#endif // in6addr_allnodesonlink

		SetIp(in6addr_allnodesonlink);
		SetPort(0);
	}

	/**
	 * Converts this internet ip address to string form. String will be enclosed in square braces.
	 *
	 * @param bAppendPort whether to append the port information or not
	 */
	virtual FString ToString(bool bAppendPort) const override
	{
		char IPStr[INET6_ADDRSTRLEN];

		inet_ntop(AF_INET6, (void*)&Addr.sin6_addr, IPStr, INET6_ADDRSTRLEN);

		FString Result("[");
		Result += IPStr;
		Result += "]";
		
		if (bAppendPort)
		{
			Result += ":";
			Result += FString::Printf(TEXT("%d"), GetPort());
		}

		return Result;
	}

	/**
	 * Compares two internet ip addresses for equality
	 *
	 * @param Other the address to compare against
	 */
	virtual bool operator==(const FInternetAddr& Other) const override
	{
		FInternetAddrBSDIPv6& OtherBSD = (FInternetAddrBSDIPv6&)Other;
		return memcmp(&Addr.sin6_addr,&OtherBSD.Addr.sin6_addr,sizeof(in6_addr)) == 0 &&
			Addr.sin6_port == OtherBSD.Addr.sin6_port &&
			Addr.sin6_family == OtherBSD.Addr.sin6_family;
	}

	/**
	 * Is this a well formed internet address, the only criteria being non-zero
	 *
	 * @return true if a valid IP, false otherwise
	 */
	virtual bool IsValid() const override
	{
		FInternetAddrBSDIPv6 Temp;

		return memcmp(&Addr.sin6_addr, &Temp.Addr.sin6_addr, sizeof(in6_addr)) != 0;
	}

 	operator sockaddr*(void)
 	{
 		return (sockaddr*)&Addr;
 	}

 	operator const sockaddr*(void) const
 	{
 		return (const sockaddr*)&Addr;
 	}
};

#endif
