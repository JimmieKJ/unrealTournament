// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * ICO implementation of the helper class
 */
class FIcoImageWrapper
	: public FImageWrapperBase
{
public:

	/**
	 * Default Constructor.
	 */
	FIcoImageWrapper();

public:

	// FImageWrapper Interface

	virtual void Compress( int32 Quality ) override;
	virtual void Uncompress( const ERGBFormat::Type InFormat, int32 InBitDepth ) override;
	virtual bool SetCompressed( const void* InCompressedData, int32 InCompressedSize ) override;
	virtual bool GetRaw( const ERGBFormat::Type InFormat, int32 InBitDepth, const TArray<uint8>*& OutRawData ) override;

private:

	/** 
	 * Load the header information.
	 *
	 * @return true if successful
	 */
	bool LoadICOHeader();

private:

	/** Sub-wrapper component, as icons that contain PNG or BMP data */
	TSharedPtr<FImageWrapperBase> SubImageWrapper;

	/** Offset into file that we use as image data */
	uint32 ImageOffset;

	/** Size of image data in file */
	uint32 ImageSize;

	/** Whether we should use PNG or BMP data */
	bool bIsPng;
};
