// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MeshUtilitiesPrivate.h"
#include "Allocator2D.h"

FAllocator2D::FAllocator2D( uint32 InWidth, uint32 InHeight )
	: Width( InWidth )
	, Height( InHeight )
{
	Pitch = ( Width + 63 ) / 64;
	// alloc +1 to avoid buffer overrun
	Bits = (uint64*)FMemory::Malloc( (Pitch * Height + 1) * sizeof(uint64) );
}

FAllocator2D::~FAllocator2D()
{
	FMemory::Free( Bits );
}

void FAllocator2D::Clear()
{
	FMemory::Memzero( Bits, (Pitch * Height + 1) * sizeof(uint64) );
}

bool FAllocator2D::Find( FRect& Rect )
{
	FRect TestRect = Rect;
	for( TestRect.X = 0; TestRect.X <= Width - TestRect.W; TestRect.X++ )
	{
		for( TestRect.Y = 0; TestRect.Y <= Height - TestRect.H; TestRect.Y++ )
		{
			if( Test( TestRect ) )
			{
				Rect = TestRect;
				return true;
			}
		}
	}

	return false;
}

bool FAllocator2D::Find( FRect& Rect, const FAllocator2D& Other )
{
	FRect TestRect = Rect;
	for( TestRect.X = 0; TestRect.X <= Width - TestRect.W; TestRect.X++ )
	{
		for( TestRect.Y = 0; TestRect.Y <= Height - TestRect.H; TestRect.Y++ )
		{
			if( Test( TestRect, Other ) )
			{
				Rect = TestRect;
				return true;
			}
		}
	}
	
	return false;
}

void FAllocator2D::Alloc( FRect Rect )
{
	for( uint32 y = Rect.Y; y < Rect.Y + Rect.H; y++ )
	{
		for( uint32 x = Rect.X; x < Rect.X + Rect.W; x++ )
		{
			SetBit( x, y );
		}
	}
}

void FAllocator2D::Alloc( FRect Rect, const FAllocator2D& Other )
{
	for( uint32 y = 0; y < Rect.H; y++ )
	{
		for( uint32 x = 0; x < Rect.W; x++ )
		{
			if( Other.GetBit( x, y ) )
			{
				SetBit( x + Rect.X, y + Rect.Y );
			}
		}
	}
}