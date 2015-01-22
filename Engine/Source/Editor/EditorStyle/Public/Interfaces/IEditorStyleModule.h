// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once


class IEditorStyleModule : public IModuleInterface
{
public:

	virtual TSharedRef< class FSlateStyleSet > CreateEditorStyleInstance() const = 0;
};
