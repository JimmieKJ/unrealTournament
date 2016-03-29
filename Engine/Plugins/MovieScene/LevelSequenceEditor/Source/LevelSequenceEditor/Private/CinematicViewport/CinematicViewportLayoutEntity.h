// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LevelViewportLayout.h"
#include "SCinematicLevelViewport.h"

class FCinematicViewportLayoutEntity : public IViewportLayoutEntity
{
public:
	FCinematicViewportLayoutEntity(const FViewportConstructionArgs& Args)
		: Widget(
			SNew(SCinematicLevelViewport)
			.ParentLayout(Args.ParentLayout)
			.ParentLevelEditor(Args.ParentLevelEditor)
			.LayoutName(*Args.ConfigKey)
			)
	{}

	virtual TSharedRef<SWidget> AsWidget() const override { return Widget; }
	virtual TSharedPtr<SLevelViewport> AsLevelViewport() const { return Widget->GetLevelViewport(); }

	FName GetType() const
	{
		static FName CinematicName("Cinematic");
		return CinematicName;
	}

	FLevelEditorViewportClient& GetLevelViewportClient() const { return Widget->GetLevelViewport()->GetLevelViewportClient(); }
	bool IsPlayInEditorViewportActive() const { return false; }
	void RegisterGameViewportIfPIE(){}
	void SetKeyboardFocus(){ FSlateApplication::Get().SetKeyboardFocus(Widget); }
	void OnLayoutDestroyed(){}
	void SaveConfig(const FString&){}

protected:

	/** This entity's widget */
	TSharedRef<SCinematicLevelViewport> Widget;
};