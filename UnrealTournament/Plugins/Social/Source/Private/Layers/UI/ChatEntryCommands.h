// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ChatEntryCommands.h: Declares commands that can be used in the chat text entry widget.
=============================================================================*/
#pragma once

#define LOCTEXT_NAMESPACE ""

/**
 * The device details commands
 */
class FChatEntryCommands
	: public TCommands<FChatEntryCommands>
{
public:

	/**
	 * Default constructor.
	 */
	FChatEntryCommands()
		: TCommands<FChatEntryCommands>(
			"ChatEntry",
			NSLOCTEXT("ChatEntryCommands", "Chat Commands", "Chat Commands"),
			NAME_None, FCoreStyle::Get().GetStyleSetName()
		)
	{ }

public:

	// TCommands interface

	virtual void RegisterCommands( ) override
	{
		UI_COMMAND( AutoFill, "Auto Fill", "Auto fill name", EUserInterfaceActionType::Button, FInputChord( EKeys::Tab ) )
	}

public:
	TSharedPtr<FUICommandInfo> AutoFill;
};

#undef LOCTEXT_NAMESPACE