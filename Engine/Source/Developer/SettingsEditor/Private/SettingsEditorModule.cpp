// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SettingsEditorPrivatePCH.h"
#include "ISettingsEditorModule.h"


/**
 * Implements the SettingsEditor module.
 */
class FSettingsEditorModule
	: public ISettingsEditorModule
{
public:

	// ISettingsEditorModule interface

	virtual TSharedRef<SWidget> CreateEditor( const TSharedRef<ISettingsEditorModel>& Model ) override
	{
		return SNew(SSettingsEditor, Model);
	}

	virtual ISettingsEditorModelRef CreateModel( const TSharedRef<ISettingsContainer>& SettingsContainer ) override
	{
		return MakeShareable(new FSettingsEditorModel(SettingsContainer));
	}
};


IMPLEMENT_MODULE(FSettingsEditorModule, SettingsEditor);
