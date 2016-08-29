// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 *  Customize the actor recording object so we can expand external settings in-line
 */
class FActorRecordingDetailsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FActorRecordingDetailsCustomization);
	}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
