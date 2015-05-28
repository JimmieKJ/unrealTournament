// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FSceneComponentDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;
	
private:
	void MakeTransformDetails( IDetailLayoutBuilder& DetailBuilder );

	FSlateColor GetMobilityTextColor(EComponentMobility::Type InMobility) const;

	ECheckBoxState IsMobilityActive(EComponentMobility::Type InMobility) const;

	void OnMobilityChanged(ECheckBoxState InCheckedState, EComponentMobility::Type InMobility);

	FText GetMobilityToolTip() const;
private:
	TSharedPtr<IPropertyHandle> MobilityHandle;
};
