// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class IMediaPlayer;


/**
 * Type definition for media container file types.
 *
 * This type is a TMap that maps the file extensions of supported media file containers
 * formats to a human readable description text. The file extensions are stored without
 * a leading dot, i.e. {"avi", "Audio Video Interleave File"}.
 */
typedef TMap<FString, FText> FMediaFileTypes;


/**
 * Interface for media player factories.
 *
 * Media player factories are used to create instances of media player implementations.
 * Most media players will be implemented inside plug-ins, which will register their
 * factories on startup. The Media module will use the SupportsUrl() method on this
 * interface to determine which media player to instantiate for a given media URL, such
 * as a file path or a resource identifier on the internet.
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
	 * Gets a read-only collection of supported media file types.
	 *
	 * This function is used for file based media containers that reside on the users
	 * local hard drive, a shared network drive or a location on the internet. It does
	 * not take into account support for extension-less streaming media URI schemes,
	 * such as rtsp:// and udp://
	 *
	 * @return A collection of supported file types.
	 * @see SupportsUrl
	 */
	virtual const FMediaFileTypes& GetSupportedFileTypes() const = 0;

	/**
	 * Checks whether this factory can create media players that support the given URL.
	 *
	 * @param Url The URL to the media, i.e. a file path or URI.
	 * @return true if the URL supported, false otherwise.
	 * @see GetSupportedFileTypes
	 */
	virtual bool SupportsUrl(const FString& Url) const = 0;

public:

	/** Virtual destructor. */
	virtual ~IMediaPlayerFactory() { }
};
