// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


// forward declarations
class IMediaPlayer;


/**
 * Type definition for media file container formats.
 *
 * This type is a TMap that maps the file extensions of supported media file containers
 * formats to a human readable description text. The file extensions are stored without
 * a leading dot, i.e. {"avi", "Audio Video Interleave File"}.
 */
typedef TMap<FString, FText> FMediaFormats;


/**
 * Interface for media player factories.
 *
 * Video player factories are used to create instances of media player implementations.
 * Most media players will be implemented inside plug-ins, which will register their
 * factories on startup. By associating a media format with a factory, the Video module
 * can provide available media players for a given format.
 */
class IMediaPlayerFactory
{
public:

	/**
	 * Creates a media player.
	 *
	 * @return A new media player, or nullptr if a player couldn't be created.
	 */
	virtual TSharedPtr<IMediaPlayer> CreatePlayer() = 0;

	/**
	 * Gets the collection of supported media formats.
	 *
	 * @return A collection of supported formats.
	 * @see SupportsFile
	 */
	virtual const FMediaFormats& GetSupportedFormats() const = 0;

public:

	/**
	 * Checks whether this factory can create media players that support the specified file format.
	 *
	 * @param Url The URL to the media file container.
	 * @see GetSupportedFormats
	 */
	bool SupportsFile( const FString& Url ) const
	{
		return GetSupportedFormats().Contains(FPaths::GetExtension(Url));
	}

public:

	/** Virtual destructor. */
	virtual ~IMediaPlayerFactory() { }
};
