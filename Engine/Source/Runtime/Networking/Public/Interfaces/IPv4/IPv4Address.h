// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements an IPv4 address.
 */
class FIPv4Address
{
public:
	
	/**
	 * Default constructor.
	 */
	FIPv4Address( ) { }

	/**
	 * Creates and initializes a new IPv4 address with the specified components.
	 *
	 * The created IP address has the value A.B.C.D.
	 *
	 * @param A - The first component.
	 * @param B - The second component.
	 * @param C - The third component.
	 * @param D - The fourth component.
	 */
	FIPv4Address( uint8 A, uint8 B, uint8 C, uint8 D )
	{
		Bytes[0] = D;
		Bytes[1] = C;
		Bytes[2] = B;
		Bytes[3] = A;
	}
	
	/**
	 * Creates and initializes a new IPv4 address with the specified value.
	 *
	 * Note: The byte ordering of the passed in value is platform dependent.
	 *
	 * @param InValue - The address value.
	 */
	FIPv4Address( uint32 InValue )
		: Value(InValue)
	{ }


public:

	/**
	 * Compares this IPv4 address with the given address for equality.
	 *
	 * @param Other - The address to compare with.
	 *
	 * @return true if the addresses are equal, false otherwise.
	 */
	bool operator==( const FIPv4Address& Other ) const
	{
		return (Value == Other.Value);
	}

	/**
	 * Compares this IPv4 address with the given address for inequality.
	 *
	 * @param Other - The address to compare with.
	 *
	 * @return true if the addresses are not equal, false otherwise.
	 */
	bool operator!=( const FIPv4Address& Other ) const
	{
		return (Value != Other.Value);
	}

	/**
	 * OR operator for masking IP addresses with a subnet mask.
	 *
	 * @param SubnetMask - The subnet mask.
	 *
	 * @return The masked IP address.
	 */
	FIPv4Address operator|( const FIPv4SubnetMask& SubnetMask ) const
	{
		return FIPv4Address(Value | SubnetMask.GetValue());
	}

	/**
	 * AND operator for masking IP addresses with a subnet mask.
	 *
	 * @param SubnetMask - The subnet mask.
	 *
	 * @return The masked IP address.
	 */
	FIPv4Address operator&( const FIPv4SubnetMask& SubnetMask ) const
	{
		return FIPv4Address(Value & SubnetMask.GetValue());
	}

	/**
	 * Serializes the given IPv4 address from or into the specified archive.
	 *
	 * @param Ar - The archive to serialize from or into.
	 * @param IpAddress - The address to serialize.
	 *
	 * @return The archive.
	 */
	friend FArchive& operator<<( FArchive& Ar, FIPv4Address& IpAddress )
	{
		return Ar << IpAddress.Value;
	}

	
public:

	/**
	 * Gets a byte component.
	 *
	 * The IP address components have the following indices:
	 *		A = 3
	 *		B = 2
	 *		C = 1
	 *		D = 0
	 *
	 * @param Index - The index of the byte component to return.
	 *
	 * @return Byte component.
	 *
	 * @see GetValue
	 */
	uint8 GetByte( int32 Index ) const
	{
		check((Index >= 0) && (Index <= 3));

		return Bytes[Index];
	}

	/**
	 * Gets the address value.
	 *
	 * Note: The byte ordering of the returned value is platform dependent.
	 *
	 * @return Address value.
	 *
	 * @see GetByte
	 */
	uint32 GetValue( ) const
	{
		return Value;
	}

	/**
	 * Checks whether this IP address is a global multicast address.
	 *
	 * Global multicast addresses are in the range 224.0.1.0 to 238.255.255.255.
	 *
	 * @return true if it is a global multicast address, false otherwise.
	 *
	 * @see IsLinkLocalMulticast
	 * @see IsOrganizationLocalMulticast
	 * @see IsSiteLocalMulticast
	 */
	bool IsGlobalMulticast( ) const
	{
		return (((Bytes[3] >= 224) && Bytes[3] <= 238) &&
			!((Bytes[3] == 224) && (Bytes[2] == 0) && (Bytes[1] == 0)));
	}

	/**
	 * Checks whether this IP address is link local.
	 *
	 * Link local addresses are in the range 169.254.0.0/16
	 *
	 * @return true if it is link local, false otherwise.
	 */
	bool IsLinkLocal( ) const
	{
		return ((Bytes[3] == 169) && (Bytes[2] == 254));
	}

	/**
	 * Checks whether this IP address is a link local multicast address.
	 *
	 * Link local multicast addresses have the form 224.0.0.x.
	 *
	 * @return true if it is a link local multicast address, false otherwise.
	 *
	 * @see IsGlobalMulticast
	 * @see IsOrganizationLocalMulticast
	 * @see IsSiteLocalMulticast
	 */
	bool IsLinkLocalMulticast( ) const
	{
		return ((Bytes[3] >= 224) && (Bytes[2] == 0) && (Bytes[1] == 0));
	}

	/**
	 * Checks whether this IP address is a loopback address.
	 *
	 * Loopback addresses have the form 127.x.x.x
	 *
	 * @return true if it is a loopback address, false otherwise.
	 *
	 * @see IsMulticastAddress
	 */
	bool IsLoopbackAddress( ) const
	{
		return (Bytes[3] == 127);
	}

	/**
	 * Checks whether this IP address is a multicast address.
	 *
	 * Multicast addresses are in the range 224.0.0.0 to 239.255.255.255.
	 *
	 * @return true if it is a multicast address, false otherwise.
	 *
	 * @see IsLoopbackAddress
	 */
	bool IsMulticastAddress( ) const
	{
		return ((Bytes[3] >= 224) && (Bytes[3] <= 239));
	}

	/**
	 * Checks whether this IP address is a organization local multicast address.
	 *
	 * Organization local multicast addresses are in the range 239.192.x.x to 239.195.x.x.
	 *
	 * @return true if it is a site local multicast address, false otherwise.
	 *
	 * @see IsGlobalMulticast
	 * @see IsLinkLocalMulticast
	 * @see IsSiteLocalMulticast
	 */
	bool IsOrganizationLocalMulticast( ) const
	{
		return ((Bytes[3] == 239) && (Bytes[2] >= 192) && (Bytes[2] <= 195));
	}

	/**
	 * Checks whether this IP address is a site local address.
	 *
	 * Site local addresses have one of the following forms:
	 *		10.x.x.x
	 *		172.16.x.x
	 *		192.168.x.x
	 *
	 * @return true if it is a site local address, false otherwise.
	 */
	bool IsSiteLocalAddress( ) const
	{
		return ((Bytes[3] == 10) ||
				((Bytes[3] == 172) && (Bytes[2] == 16)) ||
				((Bytes[3] == 192) && (Bytes[2] == 168)));
	}

	/**
	 * Checks whether this IP address is a site local multicast address.
	 *
	 * Site local multicast addresses have the form 239.255.0.x.
	 *
	 * @return true if it is a site local multicast address, false otherwise.
	 *
	 * @see IsGlobalMulticast
	 * @see IsLinkLocalMulticast
	 * @see IsOrganizationLocalMulticast
	 */
	bool IsSiteLocalMulticast( ) const
	{
		return ((Bytes[3] == 239) && (Bytes[2] == 255));
	}

	/**
	 * Gets the string representation for this address.
	 *
	 * @return String representation.
	 */
	NETWORKING_API FText ToText( ) const;


public:

	/**
	 * Gets the hash for specified IPv4 address.
	 *
	 * @param Address - The address to get the hash for.
	 *
	 * @return Hash value.
	 */
	friend uint32 GetTypeHash( const FIPv4Address& Address )
	{
		return Address.Value;
	}


public:
	
	/**
	 * Converts a string to an IPv4 address.
	 *
	 * @param AddressString - The string to convert.
	 * @param OutAddress - Will contain the parsed address.
	 *
	 * @return true if the string was converted successfully, false otherwise.
	 */
	static NETWORKING_API bool Parse( const FString& AddressString, FIPv4Address& OutAddress );

	
public:

	/**
	 * Defines the wild card address, which is 0.0.0.0
	 */
	static NETWORKING_API const FIPv4Address Any;

	/**
	 * Defines the internal loopback address, which is 127.0.0.1
	 */
	static NETWORKING_API const FIPv4Address InternalLoopback;

	/**
	 * Defines the broadcast address for the 'zero network' (i.e. LAN), which is 255.255.255.255.
	 */
	static NETWORKING_API const FIPv4Address LanBroadcast;


private:

	union
	{
		// Holds the address as an array of bytes.
		uint8 Bytes[4];

		// Holds the address as an integer.
		uint32 Value;
	};
};
