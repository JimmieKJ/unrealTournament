/************************************************************************************

Filename    :   ImageData.cpp
Content     :   Operations on byte arrays that don't interact with the GPU.
Created     :   July 9, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "ImageData.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_Alg.h"
#include "Android/LogUtils.h"

namespace OVR {


#pragma pack(1)
struct OVR_PVR_HEADER
{
    UInt32	Version;
    UInt32	Flags;
    UInt64  PixelFormat;
    UInt32  ColorSpace;
    UInt32  ChannelType;
    UInt32	Height;
    UInt32	Width;
    UInt32	Depth;
    UInt32  NumSurfaces;
    UInt32  NumFaces;
    UInt32  MipMapCount;
    UInt32  MetaDataSize;
};
#pragma pack()

void Write32BitPvrTexture( const char * fileName, const unsigned char * texture, int width, int height )
{
	FILE *f = fopen( fileName, "wb" );
	if ( !f )
	{
		WARN( "Failed to write %s", fileName );
		return;
	}

    OVR_PVR_HEADER header = {};
	header.Version = 0x03525650;				// 'PVR' + 0x3
    header.PixelFormat = 578721384203708274llu;	// 8888 RGBA
    header.Width = width;
    header.Height = height;
    header.Depth = 1;
    header.NumSurfaces = 1;
    header.NumFaces = 1;
    header.MipMapCount = 1;

	fwrite( &header, 1, sizeof( header ), f );
	fwrite( texture, 1, width * height * 4, f );

	fclose( f );
}

inline int AbsInt( const int x )
{
	const int mask = x >> ( sizeof( int )* 8 - 1 );
	return ( x + mask ) ^ mask;
}

inline int ClampInt( const int x, const int min, const int max )
{
	return min + ( ( AbsInt( x - min ) - AbsInt( x - min - max ) + max ) >> 1 );
}

inline float FracFloat( const float x )
{
	return x - floorf( x );
}

// "A Standard Default Color Space for the Internet - sRGB, Version 1.10"
// Michael Stokes, Matthew Anderson, Srinivasan Chandrasekar, Ricardo Motta
// November 5, 1996
static float SRGBToLinear( float c )
{
	const float a = 0.055f;
	if ( c <= 0.04045f )
	{
		return c * ( 1.0f / 12.92f );
	}
	else
	{
		return powf( ( ( c + a ) * ( 1.0f / ( 1.0f + a ) ) ), 2.4f );
	}
}

static float LinearToSRGB( float c )
{
	const float a = 0.055f;
	if ( c <= 0.0031308f )
	{
		return c * 12.92f;
	}
	else
	{
		return ( 1.0f + a ) * powf( c, ( 1.0f / 2.4f ) ) - a;
	}
}

unsigned char * QuarterImageSize( const unsigned char * src, const int width, const int height, const bool srgb )
{
	float table[256];
	if ( srgb )
	{
		for ( int i = 0; i < 256; i++ )
		{
			table[ i ] = SRGBToLinear( i * ( 1.0f / 255.0f ) );
		}
	}

	const int newWidth = OVR::Alg::Max( 1, width >> 1 );
	const int newHeight = OVR::Alg::Max( 1, height >> 1 );
	unsigned char * out = (unsigned char *)malloc( newWidth * newHeight * 4 );
	unsigned char * out_p = out;
	for ( int y = 0; y < newHeight; y++ )
	{
		const unsigned char * in_p = src + y * 2 * width * 4;
		for ( int x = 0; x < newWidth; x++ )
		{
			for ( int i = 0; i < 4; i++ )
			{
				if ( srgb )
				{
					const float linear = ( table[ in_p[ i ] ] +
						table[ in_p[ 4 + i ] ] +
						table[ in_p[ width * 4 + i ] ] +
						table[ in_p[ width * 4 + 4 + i ] ] ) * 0.25f;
					const float gamma = LinearToSRGB( linear );
					out_p[ i ] = ( unsigned char )ClampInt( ( int )( gamma * 255.0f + 0.5f ), 0, 255 );
				}
				else
				{
					out_p[ i ] = ( in_p[ i ] +
						in_p[ 4 + i ] +
						in_p[ width * 4 + i ] +
						in_p[ width * 4 + 4 + i ] ) >> 2;
				}
			}
			out_p += 4;
			in_p += 8;
		}
	}
	return out;
}

static const float BICUBIC_SHARPEN = 0.75f;	// same as default PhotoShop bicubic filter

static void FilterWeights( const float s, const int filter, float weights[ 4 ] )
{
	switch ( filter )
	{
	case IMAGE_FILTER_NEAREST:	
	{
				weights[ 0 ] = 1.0f;
				break;
	}
	case IMAGE_FILTER_LINEAR:	
	{
				weights[ 0 ] = 1.0f - s;
				weights[ 1 ] = s;
				break;
	}
	case IMAGE_FILTER_CUBIC: 
	{
				weights[ 0 ] = ( ( ( ( +0.0f - BICUBIC_SHARPEN ) * s + ( +0.0f + 2.0f * BICUBIC_SHARPEN ) ) * s + ( -BICUBIC_SHARPEN ) ) * s + ( 0.0f ) );
				weights[ 1 ] = ( ( ( ( +2.0f - BICUBIC_SHARPEN ) * s + ( -3.0f + 1.0f * BICUBIC_SHARPEN ) ) * s + ( 0.0f ) ) * s + ( 1.0f ) );
				weights[ 2 ] = ( ( ( ( -2.0f + BICUBIC_SHARPEN ) * s + ( +3.0f - 2.0f * BICUBIC_SHARPEN ) ) * s + ( BICUBIC_SHARPEN ) ) * s + ( 0.0f ) );
				weights[ 3 ] = ( ( ( ( +0.0f + BICUBIC_SHARPEN ) * s + ( +0.0f - 1.0f * BICUBIC_SHARPEN ) ) * s + ( 0.0f ) ) * s + ( 0.0f ) );
				break;
	}
	}
}

unsigned char * ScaleImageRGBA( const unsigned char * src, const int width, const int height, const int newWidth, const int newHeight, const ImageFilter filter )
{
	int footprintMin = 0;
	int footprintMax = 0;
	int offsetX = 0;
	int offsetY = 0;
	switch ( filter )
	{
	case IMAGE_FILTER_NEAREST:	
	{
				footprintMin = 0;
				footprintMax = 0;
				offsetX = width;
				offsetY = height;
				break;
	}
	case IMAGE_FILTER_LINEAR:	
	{
				footprintMin = 0;
				footprintMax = 1;
				offsetX = width - newWidth;
				offsetY = height - newHeight;
				break;
	}
	case IMAGE_FILTER_CUBIC:	
	{
				footprintMin = -1;
				footprintMax = 2;
				offsetX = width - newWidth;
				offsetY = height - newHeight;
				break;
	}
	}

	unsigned char * scaled = ( unsigned char * )malloc( newWidth * newHeight * 4 * sizeof( unsigned char ) );

	float * srcLinear = ( float * )malloc( width * height * 4 * sizeof( float ) );
	float * scaledLinear = ( float * )malloc( newWidth * newHeight * 4 * sizeof( float ) );

	float table[ 256 ];
	for ( int i = 0; i < 256; i++ )
	{
		table[ i ] = SRGBToLinear( i * ( 1.0f / 255.0f ) );
	}

	for ( int y = 0; y < height; y++ )
	{
		for ( int x = 0; x < width; x++ )
		{
			for ( int c = 0; c < 4; c++ )
			{
				srcLinear[ ( y * width + x ) * 4 + c ] = table[ src[ ( y * width + x ) * 4 + c ] ];
			}
		}
	}

	for ( int y = 0; y < newHeight; y++ )
	{
		const int srcY = ( y * height * 2 + offsetY ) / ( newHeight * 2 );
		const float fracY = FracFloat( ( ( float )y * height * 2.0f + offsetY ) / ( newHeight * 2.0f ) );

		float weightsY[ 4 ];
		FilterWeights( fracY, filter, weightsY );

		for ( int x = 0; x < newWidth; x++ )
		{
			const int srcX = ( x * width * 2 + offsetX ) / ( newWidth * 2 );
			const float fracX = FracFloat( ( ( float )x * width * 2.0f + offsetX ) / ( newWidth * 2.0f ) );

			float weightsX[ 4 ];
			FilterWeights( fracX, filter, weightsX );

			float fR = 0.0f;
			float fG = 0.0f;
			float fB = 0.0f;
			float fA = 0.0f;

			for ( int fpY = footprintMin; fpY <= footprintMax; fpY++ )
			{
				const float wY = weightsY[ fpY - footprintMin ];

				for ( int fpX = footprintMin; fpX <= footprintMax; fpX++ )
				{
					const float wX = weightsX[ fpX - footprintMin ];
					const float wXY = wX * wY;

					const int cx = ClampInt( srcX + fpX, 0, width - 1 );
					const int cy = ClampInt( srcY + fpY, 0, height - 1 );
					fR += srcLinear[ ( cy * width + cx ) * 4 + 0 ] * wXY;
					fG += srcLinear[ ( cy * width + cx ) * 4 + 1 ] * wXY;
					fB += srcLinear[ ( cy * width + cx ) * 4 + 2 ] * wXY;
					fA += srcLinear[ ( cy * width + cx ) * 4 + 3 ] * wXY;
				}
			}

			scaledLinear[ ( y * newWidth + x ) * 4 + 0 ] = fR;
			scaledLinear[ ( y * newWidth + x ) * 4 + 1 ] = fG;
			scaledLinear[ ( y * newWidth + x ) * 4 + 2 ] = fB;
			scaledLinear[ ( y * newWidth + x ) * 4 + 3 ] = fA;
		}
	}

	for ( int y = 0; y < newHeight; y++ )
	{
		for ( int x = 0; x < newWidth; x++ )
		{
			for ( int c = 0; c < 4; c++ )
			{
				const float gamma = LinearToSRGB( scaledLinear[ ( y * newWidth + x ) * 4 + c ] );
				scaled[ ( y * newWidth + x ) * 4 + c ] = ( unsigned char )ClampInt( ( int )( gamma * 255.0f + 0.5f ), 0, 255 );
			}
		}
	}

	free( scaledLinear );
	free( srcLinear );

	return scaled;
}


}	// namespace OVR
