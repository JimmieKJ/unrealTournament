// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FAttenuationSettingsCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization instance */
	virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;

protected:

	EVisibility IsSphereSelected() const;
	EVisibility IsBoxSelected() const;
	EVisibility IsCapsuleSelected() const;
	EVisibility IsConeSelected() const;
	EVisibility IsNaturalSoundSelected() const;
	EVisibility IsCustomCurveSelected() const;

	TSharedPtr< IPropertyHandle > AttenuationShapeHandle;
	TSharedPtr< IPropertyHandle > DistanceAlgorithmHandle;
	TSharedPtr< IPropertyHandle > SpatializationAlgorithmHandle;
};
