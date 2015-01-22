// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SLATE_API FGenericCommands : public TCommands<FGenericCommands>
{
public:
	
	FGenericCommands()
		: TCommands<FGenericCommands>( TEXT("GenericCommands"), NSLOCTEXT("GenericCommands", "Generic Commands", "Common Commands"), NAME_None, FCoreStyle::Get().GetStyleSetName() )
	{
	}

	virtual ~FGenericCommands()
	{
	}

	virtual void RegisterCommands() override;	

	TSharedPtr<FUICommandInfo> Cut;
	TSharedPtr<FUICommandInfo> Copy;
	TSharedPtr<FUICommandInfo> Paste;
	TSharedPtr<FUICommandInfo> Duplicate;
	TSharedPtr<FUICommandInfo> Undo;
	TSharedPtr<FUICommandInfo> Redo;
	TSharedPtr<FUICommandInfo> Delete;
	TSharedPtr<FUICommandInfo> Rename;
	TSharedPtr<FUICommandInfo> SelectAll;	
};
