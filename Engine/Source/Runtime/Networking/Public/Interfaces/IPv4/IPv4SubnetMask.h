// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


namespace EIPv4SubnetClasses
{
	/**
	 * Enumerates IPv4 subnet classes.
	 */
	enum Type
	{
		/**
		 * Invalid subnet mask.
		 */
		Invalid,

		/**
		 * Class A subnet.
		 */
		ClassA,

		/**
		 * Class B subnet.
		 */
		ClassB,

		/**
		 * Class C subnet.
		 */
		ClassC
	};
}


/**
 * Implements an IPv4 subnet mask.
 */
class FIPv4SubnetMask
{
public:

	/**
	 * Default constructor.
	 */
	FIPv4SubnetMask( ) { }

	/**
	 * Creates and initializes a new IPv4 subnet mask with the specified components.
	 *
	 * The created subnet mask has the value A.B.C.D.
	 *
	 * @param A - The first component.
	 * @param B - The second component.
	 * @param C - The third component.
	 * @param D - The fourth component.
	 */
	FIPv4SubnetMask( uint8 A, uint8 B, uint8 C, uint8 D )
	{
		Bytes[0] = D;
		Bytes[1] = C;
		Bytes[2] = B;
		Bytes[3] = A;
	}
	
	/**
	 * Creates and initializes a new IPv4 subnet mask with the specified value.
	 *
	 * Note: The byte ordering of the passed in value is platform dependent.
	 *
	 * @param InValue - The address value.
	 */
	FIPv4SubnetMask( uint32 InValue )
		: Value(InValue)
	{ }


public:

	/**
	 * Compares this subnet mask with the given mask for equality.
	 *
	 * @param other - The subnet mask to compare with.
	 *
	 * @return true if the subnet masks are equal, false otherwise.
	 */
	bool operator==( const FIPv4SubnetMask& Other ) const
	{
		return (Value == Other.Value);
	}

	/**
	 * Compares this subnet mask with the given address for inequality.
	 *
	 * @param other - The subnet mask to compare with.
	 *
	 * @return true if the subnet masks are not equal, false otherwise.
	 */
	bool operator!=( const FIPv4SubnetMask& Other ) const
	{
		return (Value != Other.Value);
	}

	/**
	 * Returns an inverted subnet mask.
	 *
	 * @return Inverted subnet mask.
	 */
	FIPv4SubnetMask operator~( ) const
	{
		return FIPv4SubnetMask(~Value);
	}

	/**
	 * Serializes the given subnet mask from or into the specified archive.
	 *
	 * @param Ar - The archive to serialize from or into.
	 * @param SubnetMask - The subnet mask to serialize.
	 *
	 * @return The archive.
	 */
	friend FArchive& operator<<( FArchive& Ar, FIPv4SubnetMask& SubnetMask )
	{
		return Ar << SubnetMask.Value;
	}


public:

	/**
	 * Gets a byte component.
	 *
	 * @param Index - The index of the byte component to return.
	 *
	 * @return Byte component.
	 */
	uint8 GetByte( int32 Index ) const
	{
		return Bytes[Index];
	}

	/**
	 * Gets the subnet class that this mask specifies.
	 *
	 * @return Subnet class.
	 */
	EIPv4SubnetClasses::Type GetClass( ) const
	{
		if (Bytes[3] == 255)
		{
			if (Bytes[2] == 255)
			{
				if (Bytes[1] == 255)
				{
					return EIPv4SubnetClasses::ClassC;
				}

				return EIPv4SubnetClasses::ClassB;
			}

			return EIPv4SubnetClasses::ClassA;
		}

		return EIPv4SubnetClasses::Invalid;
	}

	/**
	 * Gets the address value.
	 *
	 * Note: The byte ordering of the returned value is platform dependent.
	 *
	 * @return Address value.
	 */
	uint32 GetValue( ) const
	{
		return Value;
	}

	/**
	 * Converts this IP address to its string representation.
	 *
	 * @return String representation.
	 */
	NETWORKING_API FText ToText( ) const;


public:

	/**
	 * Gets the hash for specified IPv4 subnet mask.
	 *
	 * @param SubnetMask - The subnet mask to get the hash for.
	 *
	 * @return Hash value.
	 */
	friend uint32 GetTypeHash( const FIPv4SubnetMask& SubnetMask )
	{
		return GetTypeHash(SubnetMask.Value);
	}


public:
	
	/**
	 * Converts a string to an IPv4 subnet mask.
	 *
	 * @param MaskString - The string to convert.
	 * @param OutAddress - Will contain the parsed subnet mask.
	 *
	 * @return true if the string was converted successfully, false otherwise.
	 */
	static NETWORKING_API bool Parse( const FString& MaskString, FIPv4SubnetMask& OutMask );


private:

	union
	{
		// Holds the address as an array of bytes.
		uint8 Bytes[4];

		// Holds the address as an integer.
		int32 Value;
	};
};