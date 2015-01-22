// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class FInternationalizationManifest;


/**
 * Interface for Internationalization manifest serializers.
 */
class IInternationalizationManifestSerializer
{
public:

	/**
	 * Deserializes a Internationalization manifest from a string.
	 *
	 * @param InStr The string to serialize from.
	 * @param Manifest The populated Internationalization manifest.
	 * @return true if deserialization was successful, false otherwise.
	 */
	virtual bool DeserializeManifest( const FString& InStr, TSharedRef<FInternationalizationManifest> Manifest ) = 0;

	/**
	 * Serializes a Internationalization manifest to a string.
	 *
	 * @param Manifest The Internationalization manifest data to serialize.
	 * @param Str The string to serialize into.
	 * @return true if serialization was successful, false otherwise.
	 */
	virtual bool SerializeManifest( TSharedRef<const FInternationalizationManifest> Manifest, FString& Str ) = 0;


#if 0 // @todo Json: Serializing from FArchive is currently broken
	/**
	 * Deserializes a manifest from an archive.
	 *
	 * @param Archive The archive to serialize from.
	 * @param Manifest Will hold the content of the deserialized manifest.
	 * @return true if deserialization was successful, false otherwise.
	 */
	virtual bool DeserializeManifest( FArchive& Archive, TSharedRef<FInternationalizationManifest> Manifest ) = 0;

	/**
	 * Serializes a manifest into an archive.
	 *
	 * @param Manifest The manifest data to serialize.
	 * @param Archive The archive to serialize into.
	 * @return true if serialization was successful, false otherwise.
	 */
	virtual bool SerializeManifest( TSharedRef<const FInternationalizationManifest> Manifest, FArchive& Archive ) = 0;
#endif

public:

	/** Virtual destructor. */
	virtual ~IInternationalizationManifestSerializer() { }
};
