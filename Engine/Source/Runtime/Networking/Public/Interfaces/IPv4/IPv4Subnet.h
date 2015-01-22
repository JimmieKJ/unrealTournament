// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements a IPv4 subnet descriptor.
 *
 * @todo gmp: add IsValid() method
 */
class FIPv4Subnet
{
	/**
	 * Default constructor.
	 */
	FIPv4Subnet( ) { }

	/**
	 * Creates and initializes a new IPv4 subnet with the specified address and mask.
	 *
	 * @param InAddress - The subnet's address.
	 * @param InMask - The subnet's mask.
	 */
	FIPv4Subnet( const FIPv4Address& InAddress, const FIPv4SubnetMask& InMask )
		: Address(InAddress)
		, Mask(InMask)
	{ }


public:

	/**
	 * Compares this IPv4 subnet descriptor with the given subnet for equality.
	 *
	 * @param other - The subnet to compare with.
	 *
	 * @return true if the subnets are equal, false otherwise.
	 */
	bool operator==( const FIPv4Subnet& Other ) const
	{
		return ((Address == Other.Address) && (Mask == Other.Mask));
	}

	/**
	 * Compares this IPv4 subnet descriptor with the given subnet for inequality.
	 *
	 * @param other - The subnet to compare with.
	 *
	 * @return true if the subnets are not equal, false otherwise.
	 */
	bool operator!=( const FIPv4Subnet& Other ) const
	{
		return ((Address != Other.Address) || (Mask != Other.Mask));
	}

	/**
	 * Serializes the given subnet descriptor from or into the specified archive.
	 *
	 * @param Ar - The archive to serialize from or into.
	 * @param Subnet - The subnet descriptor to serialize.
	 *
	 * @return The archive.
	 */
	friend FArchive& operator<<( FArchive& Ar, FIPv4Subnet& Subnet )
	{
		return Ar << Subnet.Address << Subnet.Mask;
	}

	
public:

	/**
	 * Checks whether the subnet contains the specified IP address.
	 *
	 * @param TestAddress - The address to check.
	 *
	 * @return true if the subnet contains the address, false otherwise.
	 */
	bool ContainsAddress( const FIPv4Address& TestAddress )
	{
		return ((Address & Mask) == (TestAddress & Mask));
	}

	/**
	 * Gets the subnet's address.
	 *
	 * @return Subnet address.
	 */
	FIPv4Address GetAddress( ) const
	{
		return Address;
	}
	
	/**
	 * Returns the broadcast address for this subnet.
	 *
	 * @return The broadcast address.
	 */
	FIPv4Address GetBroadcastAddress( ) const
	{
		return (Address | ~Mask);
	}

	/**
	 * Gets the subnet's mask.
	 *
	 * @return Subnet mask.
	 */
	FIPv4SubnetMask GetMask( ) const
	{
		return Mask;
	}
	
	/**
	 * Converts this IP subnet to its string representation.
	 *
	 * @return String representation.
	 */
	NETWORKING_API FText ToText( ) const;


public:

	/**
	 * Gets the hash for specified IPv4 subnet.
	 *
	 * @param Subnet - The subnet to get the hash for.
	 *
	 * @return Hash value.
	 */
	friend uint32 GetTypeHash( const FIPv4Subnet& Subnet )
	{
		return GetTypeHash(Subnet.Address) + GetTypeHash(Subnet.Mask) * 23;
	}


public:

	/**
	 * Converts a string to an IPv4 subnet.
	 *
	 * @param SubnetString - The string to convert.
	 * @param OutSubnet - Will contain the parsed subnet.
	 *
	 * @return true if the string was converted successfully, false otherwise.
	 */
	static NETWORKING_API bool Parse( const FString& SubnetString, FIPv4Subnet& OutSubnet );


private:

	// Holds the subnet's address.
	FIPv4Address Address;
	
	// Holds the subnet's mask.
	FIPv4SubnetMask Mask;
};
