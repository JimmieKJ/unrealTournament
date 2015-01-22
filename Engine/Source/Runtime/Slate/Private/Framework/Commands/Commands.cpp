// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"


#define LOC_DEFINE_REGION

void UI_COMMAND_Function( FBindingContext* This, TSharedPtr< FUICommandInfo >& OutCommand, const TCHAR* OutCommandName, const TCHAR* OutCommandNameUnderscoreTooltip, const ANSICHAR* DotOutCommandName, const TCHAR* FriendlyName, const TCHAR* InDescription, const EUserInterfaceActionType::Type CommandType, const FInputGesture& InDefaultGesture )
{
	static const FString UICommandsStr(TEXT("UICommands"));

	FUICommandInfo::MakeCommandInfo(
		This->AsShared(),
		OutCommand,
		OutCommandName,
		FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText( FriendlyName, *UICommandsStr, OutCommandName ),
		FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText( InDescription, *UICommandsStr, OutCommandNameUnderscoreTooltip ),
		FSlateIcon( This->GetStyleSetName(), ISlateStyle::Join( This->GetContextName(), DotOutCommandName ) ),
		CommandType,
		InDefaultGesture
	);
}

#undef LOC_DEFINE_REGION