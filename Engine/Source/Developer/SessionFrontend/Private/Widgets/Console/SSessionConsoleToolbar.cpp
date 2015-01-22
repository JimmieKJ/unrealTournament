// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SessionFrontendPrivatePCH.h"


/* SSessionConsoleToolbar interface
 *****************************************************************************/

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SSessionConsoleToolbar::Construct( const FArguments& InArgs, const TSharedRef<FUICommandList>& CommandList )
{
	FSessionConsoleCommands::Register();

	// create the toolbar
	FToolBarBuilder Toolbar(CommandList, FMultiBoxCustomization::None);
	{
		Toolbar.AddToolBarButton(FSessionConsoleCommands::Get().SessionCopy);
		Toolbar.AddSeparator();

		Toolbar.AddToolBarButton(FSessionConsoleCommands::Get().Clear);
		Toolbar.AddToolBarButton(FSessionConsoleCommands::Get().SessionSave);
	}

	ChildSlot
	[
		SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(0.0f)
			[
				Toolbar.MakeWidget()
			]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION
