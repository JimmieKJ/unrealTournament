// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


class IMediaOptions;
class IMediaPlayer;


/**
 * Interface for media player factories.
 *
 * Media player factories are used to create instances of media player implementations.
 * Most media players will be implemented inside plug-ins, which will register their
 * factories on startup. The Media module will use the CanPlayUrl() method on this
 * interface to determine which media player to instantiate for a given media source.
 */
class IMediaPlayerFactory
{
public:

	/**
	 * Whether the player can play the specified source URL.
	 *
	 * @param Url The media source URL to check.
	 * @param Options Optional media player parameters.
	 * @param OutWarnings Will contain warning messages (optional).
	 * @param OutErrors will contain error messages (optional).
	 * @return true if the source can be played, false otherwise.
	 */
	virtual bool CanPlayUrl(const FString& Url, const IMediaOptions& Options, TArray<FText>* OutWarnings, TArray<FText>* OutErrors) const = 0;

	/**
	 * Creates a media player.
	 *
	 * @return A new media player, or nullptr if a player couldn't be created.
	 */
	virtual TSharedPtr<IMediaPlayer> CreatePlayer() = 0;

	/**
	 * Get the human readable name of the player.
	 *
	 * @return Display name text.
	 * @see GetName
	 */
	virtual FText GetDisplayName() const = 0;

	/**
	 * Get the unique name of the media player.
	 *
	 * @return Player name.
	 * @see GetDisplayName
	 */
	virtual FName GetName() const = 0;

	/**
	 * Get the names of platforms that the media player supports.
	 *
	 * The returned platforms names must match the ones returned by
	 * FPlatformProperties::IniPlatformName, i.e. "Windows", "Android", etc.
	 *
	 * @return Platform name collection.
	 * @see GetOptionalParameters, GetRequiredParameters, GetSupportedFileExtensions, GetSupportedUriSchemes
	 */
	virtual const TArray<FString>& GetSupportedPlatforms() const = 0;

public:

	/**
	 * Whether the player can play the specified source URL.
	 *
	 * @param Url The media source URL to check.
	 * @param Options Optional media player parameters.
	 * @return true if the source can be played, false otherwise.
	 */
	bool CanPlayUrl(const FString& Url, const IMediaOptions& Options) const
	{
		return CanPlayUrl(Url, Options, nullptr, nullptr);
	}

	/**
	 * Whether the player works on the given platform.
	 *
	 * @param PlatformName The name of the platform to check.
	 * @return true if the platform is supported, false otherwise.
	 * @see GetSupportedPlatforms
	 */
	bool SupportsPlatform(const FString& PlatformName) const
	{
		return GetSupportedPlatforms().Contains(PlatformName);
	}

public:

	/** Virtual destructor. */
	virtual ~IMediaPlayerFactory() { }
};
