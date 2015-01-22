// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/SkyLight.h"
#include "IDetailCustomization.h"

class FSkyLightComponentDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();
private:
	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;

	FReply OnUpdateSkyCapture();
private:
	/** The selected sky light */
	TWeakObjectPtr<ASkyLight> SkyLight;
};
