// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Holds the UI commands for the MediaPlayerEditorToolkit widget.
 */
class FMediaPlayerEditorCommands
	: public TCommands<FMediaPlayerEditorCommands>
{
public:

	/**
	 * Default constructor.
	 */
	FMediaPlayerEditorCommands() 
		: TCommands<FMediaPlayerEditorCommands>("MediaPlayerEditor", NSLOCTEXT("Contexts", "MediaPlayerEditor", "MediaPlayer Editor"), NAME_None, "MediaPlayerEditorStyle")
	{ }

public:

	// TCommands interface

	virtual void RegisterCommands() override;
	
public:

	/** Fast forwards media playback. */
	TSharedPtr<FUICommandInfo> ForwardMedia;

	/** Pauses media playback. */
	TSharedPtr<FUICommandInfo> PauseMedia;

	/** Starts media playback. */
	TSharedPtr<FUICommandInfo> PlayMedia;

	/** Reverses media playback. */
	TSharedPtr<FUICommandInfo> ReverseMedia;

	/** Rewinds the media to the beginning. */
	TSharedPtr<FUICommandInfo> RewindMedia;
};
