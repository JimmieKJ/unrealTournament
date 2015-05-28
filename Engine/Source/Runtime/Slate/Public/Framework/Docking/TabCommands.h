// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SLATE_API FTabCommands : public TCommands < FTabCommands >
{
public:

	FTabCommands()
		: TCommands<FTabCommands>(TEXT("TabCommands"), NSLOCTEXT("TabCommands", "DockingTabCommands", "Docking Tab Commands"), NAME_None, FCoreStyle::Get().GetStyleSetName())
	{
	}

	virtual ~FTabCommands()
	{
	}

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> CloseMajorTab;
	TSharedPtr<FUICommandInfo> CloseMinorTab;
};
