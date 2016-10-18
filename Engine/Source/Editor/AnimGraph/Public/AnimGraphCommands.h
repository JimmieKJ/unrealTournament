// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/** Anim Graph Commands */
class ANIMGRAPH_API FAnimGraphCommands : public TCommands<FAnimGraphCommands>
{
public:
	FAnimGraphCommands()
		: TCommands<FAnimGraphCommands>(TEXT("AnimGraph"), NSLOCTEXT("Contexts", "AnimGraphCommands", "Anim Graph Commands"), NAME_None, FEditorStyle::GetStyleSetName())
	{
	}

	virtual void RegisterCommands() override;

public:

	// Toggle pose watching for a given node
	TSharedPtr<FUICommandInfo> TogglePoseWatch;
};
