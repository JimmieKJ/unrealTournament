// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Slate.h"
#include "SUWindowsDesktop.h"

#if !UE_SERVER

class SUWPanel : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUWPanel)
	{}

	SLATE_ARGUMENT(TWeakObjectPtr<UUTLocalPlayer>, PlayerOwner)

	SLATE_END_ARGS()

public:
	/** needed for every widget */
	void Construct(const FArguments& InArgs);

	virtual void BuildPage(FVector2D CurrentViewportSize);
	virtual void OnShowPanel();
	virtual void OnHidePanel();

	inline TWeakObjectPtr<class UUTLocalPlayer> GetPlayerOwner()
	{
		return PlayerOwner;
	}
	


protected:

	FName Tag;
	TWeakObjectPtr<UUTLocalPlayer> PlayerOwner;
};

#endif