// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/ReflectionCapture.h"
#include "IDetailCustomization.h"

class FReflectionCaptureDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();
private:
	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;

	FReply OnUpdateReflectionCaptures();
private:
	/** The selected reflection capture */
	TWeakObjectPtr<AReflectionCapture> ReflectionCapture;
};
