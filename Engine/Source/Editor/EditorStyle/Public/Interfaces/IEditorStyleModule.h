// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class IEditorStyleModule : public IModuleInterface
{
public:

	virtual TSharedRef< class FSlateStyleSet > CreateEditorStyleInstance() const = 0;
};
