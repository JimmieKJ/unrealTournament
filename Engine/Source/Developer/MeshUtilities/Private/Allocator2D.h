// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FAllocator2D
{
public:
	struct FRect
	{
		uint32 X;
		uint32 Y;
		uint32 W;
		uint32 H;
	};

public:
				FAllocator2D( uint32 Width, uint32 Height );
				~FAllocator2D();
	
	// Must clear before using
	void		Clear();

	bool		Find( FRect& Rect );
	bool		Test( FRect Rect );
	void		Alloc( FRect Rect );

	bool		Find( FRect& Rect, const FAllocator2D& Other );
	bool		Test( FRect Rect, const FAllocator2D& Other );
	void		Alloc( FRect Rect, const FAllocator2D& Other );
	
	uint64		GetBit( uint32 x, uint32 y ) const;
	void		SetBit( uint32 x, uint32 y );

private:
	uint64*		Bits;

	uint32		Width;
	uint32		Height;
	uint32		Pitch;
};

// Returns non-zero if set
FORCEINLINE uint64 FAllocator2D::GetBit( uint32 x, uint32 y ) const
{
	return Bits[ (x >> 6) + y * Pitch ] & ( 1ull << ( x & 63 ) );
}

FORCEINLINE void FAllocator2D::SetBit( uint32 x, uint32 y )
{
	Bits[ (x >> 6) + y * Pitch ] |= ( 1ull << ( x & 63 ) );
}

inline bool FAllocator2D::Test( FRect Rect )
{
	for( uint32 y = Rect.Y; y < Rect.Y + Rect.H; y++ )
	{
		for( uint32 x = Rect.X; x < Rect.X + Rect.W; x++ )
		{
			if( GetBit( x, y ) )
			{
				return false;
			}
		}
	}
	
	return true;
}

inline bool FAllocator2D::Test( FRect Rect, const FAllocator2D& Other )
{
	const uint32 LowShift = Rect.X & 63;
	const uint32 HighShift = 64 - LowShift;

	for( uint32 y = 0; y < Rect.H; y++ )
	{
#if 1
		uint32 ThisIndex = (Rect.X >> 6) + (y + Rect.Y) * Pitch;
		uint32 OtherIndex = y * Pitch;

		// Test a uint64 at a time
		for( uint32 x = 0; x < Rect.W; x += 64 )
		{
			// no need to zero out HighInt on wrap around because Other will always be zero outside Rect.
			uint64 LowInt  = Bits[ ThisIndex ];
			uint64 HighInt = Bits[ ThisIndex + 1 ];

			uint64 ThisInt = (HighInt << HighShift) | (LowInt >> LowShift);
			uint64 OtherInt = Other.Bits[ OtherIndex ];

			if( ThisInt & OtherInt )
			{
				return false;
			}

			ThisIndex++;
			OtherIndex++;
		}
#else
		for( uint32 x = 0; x < Rect.W; x++ )
		{
			if( Other.GetBit( x, y ) && GetBit( x + Rect.X, y + Rect.Y ) )
			{
				return false;
			}
		}
#endif
	}
	
	return true;
}