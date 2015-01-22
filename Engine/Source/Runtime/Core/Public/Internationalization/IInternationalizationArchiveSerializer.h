// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class FInternationalizationArchive;


/**
 * Interface for Internationalization archive serializers.
 */
class IInternationalizationArchiveSerializer
{
public:

	/**
	 * Deserializes a Internationalization archive from a string.
	 *
	 * @param InStr The string to serialize from.
	 * @param InternationalizationArchive The populated internationalization archive.
	 * @return true if deserialization was successful, false otherwise.
	 */
	virtual bool DeserializeArchive( const FString& InStr, TSharedRef<FInternationalizationArchive> InternationalizationArchive ) = 0;

	/**
	 * Serializes a Internationalization archive to a string.
	 *
	 * @param InternationalizationArchive The Internationalization archive data to serialize.
	 * @param Str The string to serialize into.
	 * @return true if serialization was successful, false otherwise.
	 */
	virtual bool SerializeArchive( TSharedRef<const FInternationalizationArchive> InternationalizationArchive, FString& Str ) = 0;

#if 0 // @todo Json: Serializing from FArchive is currently broken
	/**
	 * Deserializes a internationalization archive from an archive.
	 *
	 * @param Archive The archive to serialize from.
	 * @param InternationalizationArchive Will hold the content of the deserialized internationalization archive.
	 * @return true if deserialization was successful, false otherwise.
	 */
	virtual bool DeserializeArchive( FArchive& Archive, TSharedRef<FInternationalizationArchive> InternationalizationArchive ) = 0;

	/**
	 * Serializes a internationalization archive into an archive.
	 *
	 * @param InternationalizationArchive The Internationalization archive data to serialize.
	 * @param Archive The archive to serialize into.
	 * @return true if serialization was successful, false otherwise.
	 */
	virtual bool SerializeArchive( TSharedRef<const FInternationalizationArchive> InternationalizationArchive, FArchive& Archive ) = 0;
#endif

public:

	/** Virtual destructor. */
	virtual ~IInternationalizationArchiveSerializer() { }
};
