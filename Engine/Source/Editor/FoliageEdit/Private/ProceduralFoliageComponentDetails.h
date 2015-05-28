// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FProceduralFoliageComponentDetails : public IDetailCustomization
{
public:
	virtual ~FProceduralFoliageComponentDetails(){};

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder );
private:
	FReply OnResimulateClicked();
	bool IsResimulateEnabled() const;
	FText GetResimulateTooltipText() const;
private:
	TArray< TWeakObjectPtr<class UProceduralFoliageComponent> > SelectedComponents;

};
