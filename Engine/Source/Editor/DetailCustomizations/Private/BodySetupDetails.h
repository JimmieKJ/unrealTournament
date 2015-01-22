// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FBodySetupDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

	FText OnGetBodyMass() const;
	bool IsBodyMassReadOnly() const;
	bool IsBodyMassEnabled() const { return !IsBodyMassReadOnly(); }
	EVisibility IsMassVisible(bool bOverrideMass) const;

private:
	TArray< TWeakObjectPtr<UObject> > ObjectsCustomized;
};

