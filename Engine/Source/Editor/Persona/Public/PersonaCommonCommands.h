// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorStyleSet.h"
#include "Framework/Commands/Commands.h"

class PERSONA_API FPersonaCommonCommands : public TCommands<FPersonaCommonCommands>
{
public:
	FPersonaCommonCommands()
		: TCommands<FPersonaCommonCommands>(TEXT("PersonaCommon"), NSLOCTEXT("Contexts", "PersonaCommon", "Persona Common"), NAME_None, FEditorStyle::GetStyleSetName())
	{
	}

	virtual void RegisterCommands() override;

public:
	// Toggle playback
	TSharedPtr<FUICommandInfo> TogglePlay;
};
