// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


namespace EImageFormat
{
	/**
	 * Enumerates the types of image formats this class can handle
	 */
	enum Type
	{
		/** Invalid or unrecognized format */
		Invalid = -1,

		/** Portable Network Graphics */
		PNG = 0,

		/** Joint Photographic Experts Group */
		JPEG,

		/** Single channel jpeg */
		GrayscaleJPEG,	

		/** Windows Bitmap */
		BMP,

		/** Windows Icon resource */
		ICO,

		/** OpenEXR (HDR) image file format */
		EXR,

		/** Mac icon */
		ICNS
	};
};


namespace ERGBFormat
{
	/**
	 * Enumerates the types of RGB formats this class can handle
	 */
	enum Type
	{
		Invalid = -1,
		RGBA    =  0,
		BGRA    =  1,
		Gray    =  2,
	};
};


/** Type definition for shared pointers to instances of IImageWrapper. */
typedef TSharedPtr<class IImageWrapper> IImageWrapperPtr;

/** Type definition for shared references to instances of IImageWrapper. */
typedef TSharedRef<class IImageWrapper> IImageWrapperRef;


/**
 * Interface for image wrappers.
 */
class IImageWrapper
{
public:

	/**  
	 * Sets the compressed data.
	 *
	 * @param InCompressedData - The memory address of the start of the compressed data
	 * @param InCompressedSize - The size of the compressed data parsed
	 * @return	true if data was the expected format
	 */
	virtual bool SetCompressed( const void* InCompressedData, int32 InCompressedSize ) = 0;

	/**  
	 * Sets the compressed data.
	 *
	 * @param InRawData - The memory address of the start of the raw data
	 * @param InRawSize - The size of the compressed data parsed
	 * @param InWidth - The width of the image data
	 * @param InHeight - the height of the image data
	 * @param InFormat - the format the raw data is in, normally RGBA
	 * @param InBitDepth - the bit-depth per channel, normally 8
	 * @return true if data was the expected format
	 */
	virtual bool SetRaw( const void* InRawData, int32 InRawSize, const int32 InWidth, const int32 InHeight, const ERGBFormat::Type InFormat, const int32 InBitDepth ) = 0;

	/**
	 * Gets the compressed data.
	 *
	 * @return Array of the compressed data
	 */
	virtual const TArray<uint8>& GetCompressed(int32 Quality = 0) = 0;

	/**  
	 * Gets the raw data.
	 *
	 * @param InFormat -  How we want to manipulate the RGB data
	 * @param InBitDepth - The output bit-depth per channel, normally 8
	 * @param OutRawData - Pointer to the uncompressed raw data.
	 * @return Array of the raw data
	 */
	virtual bool GetRaw( const ERGBFormat::Type InFormat, int32 InBitDepth, const TArray<uint8>*& OutRawData ) = 0;

	/** 
	 * Gets the width of the image.
	 *
	 * @return Image width.
	 */
	virtual int32 GetWidth() const = 0;

	/** 
	 * Gets the height of the image.
	 *
	 * @return Image height.
	 */
	virtual int32 GetHeight() const = 0;

	/** 
	 * Gets the bit depth of the image 
	 *
	 * @return The bit depth per-channel of the image
	 */
	virtual int32 GetBitDepth() const = 0;

	/** 
	 * Gets the format of the image.
	 * Theoretically, this is the format it would be best to call GetRaw() with, if you support it.
	 *
	 * @return The format the image data is in
	 */
	virtual ERGBFormat::Type GetFormat() const = 0;

public:

	/**
	 * Virtual destructor.
	 */
	virtual ~IImageWrapper( ) { }
};
