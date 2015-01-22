// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * The abstract helper class for handling the different image formats
 */
class FImageWrapperBase
	: public IImageWrapper
{
public:

	/**
	 * Default Constructor.
	 */
	FImageWrapperBase( );

public:

	/**
	 * Gets the image's raw data.
	 *
	 * @return A read-only byte array containing the data.
	 */
	const TArray<uint8>& GetRawData() const
	{
		return RawData;
	}

public:

	/**
	 * Compresses the data.
	 *
	 * @param Quality The compression quality.
	 */
	virtual void Compress( int32 Quality ) = 0;

	/**
	 * Resets the local variables.
	 */
	virtual void Reset( );

	/**
	 * Sets last error message.
	 *
	 * @param ErrorMessage The error message to set.
	 */
	virtual void SetError( const TCHAR* ErrorMessage );

	/**  
	 * Function to uncompress our data 
	 *
	 * @param InFormat How we want to manipulate the RGB data
	 */
	virtual void Uncompress( const ERGBFormat::Type InFormat, int32 InBitDepth ) = 0;

public:

	// IImageWrapper interface

	virtual const TArray<uint8>& GetCompressed( int32 Quality = 0 ) override;

	virtual int32 GetBitDepth( ) const override
	{
		return BitDepth;
	}

	virtual ERGBFormat::Type GetFormat() const override
	{
		return Format;
	}

	virtual int32 GetHeight( ) const override
	{
		return Height;
	}

	virtual bool GetRaw( const ERGBFormat::Type InFormat, int32 InBitDepth, const TArray<uint8>*& OutRawData ) override;

	virtual int32 GetWidth( ) const override
	{
		return Width;
	}

	virtual bool SetCompressed( const void* InCompressedData, int32 InCompressedSize ) override;
	virtual bool SetRaw( const void* InRawData, int32 InRawSize, const int32 InWidth, const int32 InHeight, const ERGBFormat::Type InFormat, const int32 InBitDepth ) override;

protected:

	/** Arrays of compressed/raw data */
	TArray<uint8> RawData;
	TArray<uint8> CompressedData;

	/** Format of the raw data */
	TEnumAsByte<ERGBFormat::Type> RawFormat;
	int8 RawBitDepth;

	/** Format of the image */
	TEnumAsByte<ERGBFormat::Type> Format;

	/** Bit depth of the image */
	int8 BitDepth;

	/** Width/Height of the image data */
	int32 Width;
	int32 Height;

	/** Last Error Message. */
	FString LastError;
};
