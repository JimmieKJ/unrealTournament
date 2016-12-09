// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AttenuationSettingsCustomizations.h"
#include "Widgets/Text/STextBlock.h"
#include "Sound/SoundAttenuation.h"
#include "Audio.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"

TSharedRef<IPropertyTypeCustomization> FAttenuationSettingsCustomization::MakeInstance() 
{
	return MakeShareable( new FAttenuationSettingsCustomization );
}

void FAttenuationSettingsCustomization::CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	// We'll set up reset to default ourselves
	const bool bDisplayResetToDefault = false;
	const FText DisplayNameOverride = FText::GetEmpty();
	const FText DisplayToolTipOverride = FText::GetEmpty();

	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget( DisplayNameOverride, DisplayToolTipOverride, bDisplayResetToDefault )
		];
}

void FAttenuationSettingsCustomization::CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren( NumChildren );

	TMap<FName, TSharedPtr< IPropertyHandle > > PropertyHandles;

	for( uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle( ChildIndex ).ToSharedRef();
		const FName PropertyName = ChildHandle->GetProperty()->GetFName();

		PropertyHandles.Add(PropertyName, ChildHandle);
	}

	// We'll set up reset to default ourselves
	const bool bDisplayResetToDefault = false;
	const FString DisplayNameOverride = TEXT("");

	AttenuationShapeHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, AttenuationShape));
	DistanceAlgorithmHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, DistanceAlgorithm));
	SpatializationAlgorithmHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, SpatializationAlgorithm));

	TSharedRef<IPropertyHandle> AttenuationExtentsHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, AttenuationShapeExtents)).ToSharedRef();

	uint32 NumExtentChildren;
	AttenuationExtentsHandle->GetNumChildren( NumExtentChildren );

	TSharedPtr< IPropertyHandle > ExtentXHandle;
	TSharedPtr< IPropertyHandle > ExtentYHandle;
	TSharedPtr< IPropertyHandle > ExtentZHandle;

	for( uint32 ExtentChildIndex = 0; ExtentChildIndex < NumExtentChildren; ++ExtentChildIndex )
	{
		TSharedRef<IPropertyHandle> ChildHandle = AttenuationExtentsHandle->GetChildHandle( ExtentChildIndex ).ToSharedRef();
		const FName PropertyName = ChildHandle->GetProperty()->GetFName();

		if (PropertyName == GET_MEMBER_NAME_CHECKED(FVector, X))
		{
			ExtentXHandle = ChildHandle;
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(FVector, Y))
		{
			ExtentYHandle = ChildHandle;
		}
		else
		{
			check(PropertyName == GET_MEMBER_NAME_CHECKED(FVector, Z));
			ExtentZHandle = ChildHandle;
		}
	}

	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, bAttenuate)).ToSharedRef());
	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, bSpatialize)).ToSharedRef());

	// Check to see if a spatialization plugin is enabled
	if (IsAudioPluginEnabled(EAudioPlugin::SPATIALIZATION))
	{
		ChildBuilder.AddChildProperty(SpatializationAlgorithmHandle.ToSharedRef());
	}

	ChildBuilder.AddChildProperty(DistanceAlgorithmHandle.ToSharedRef() );

	IDetailPropertyRow& CustomCurveRow = ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, CustomAttenuationCurve)).ToSharedRef());
	CustomCurveRow.Visibility(TAttribute<EVisibility>(this, &FAttenuationSettingsCustomization::IsCustomCurveSelected));

	IDetailPropertyRow& dbAttenuationRow = ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, dBAttenuationAtMax)).ToSharedRef());
	dbAttenuationRow.Visibility(TAttribute<EVisibility>(this, &FAttenuationSettingsCustomization::IsNaturalSoundSelected));

	IDetailPropertyRow& AttenuationShapeRow = ChildBuilder.AddChildProperty( AttenuationShapeHandle.ToSharedRef() );
	
	ChildBuilder.AddChildProperty(AttenuationExtentsHandle)
		.Visibility(TAttribute<EVisibility>(this, &FAttenuationSettingsCustomization::IsBoxSelected))
		.DisplayName(NSLOCTEXT("AttenuationSettings", "BoxExtentsLabel", "Extents"))
		.ToolTip(NSLOCTEXT("AttenuationSettings", "BoxExtents", "The dimensions of the of the box."));

	ChildBuilder.AddChildContent(NSLOCTEXT("AttenuationSettings", "RadiusLabel", "Radius"))
		.NameContent()
		[
			SNew(STextBlock)
				.Text(NSLOCTEXT("AttenuationSettings", "RadiusLabel", "Radius"))
				.ToolTipText(NSLOCTEXT("AttenuationSettings", "RadiusToolTip", "The distance from the location of the sound at which falloff begins."))
				.Font(StructCustomizationUtils.GetRegularFont())
		]
		.ValueContent()
		[
			ExtentXHandle->CreatePropertyValueWidget()
		]
		.Visibility(TAttribute<EVisibility>(this, &FAttenuationSettingsCustomization::IsSphereSelected));

	ChildBuilder.AddChildContent(NSLOCTEXT("AttenuationSettings", "CapsuleHalfHeightLabel", "Capsule Half Height"))
			.NameContent()
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("AttenuationSettings", "CapsuleHalfHeightLabel", "Capsule Half Height"))
				.ToolTipText(NSLOCTEXT("AttenuationSettings", "CapsuleHalfHeightToolTip", "The attenuation capsule's half height."))
				.Font(StructCustomizationUtils.GetRegularFont())
			]
		.ValueContent()
			[
				ExtentXHandle->CreatePropertyValueWidget()
			]
		.Visibility(TAttribute<EVisibility>(this, &FAttenuationSettingsCustomization::IsCapsuleSelected));

	ChildBuilder.AddChildContent(NSLOCTEXT("AttenuationSettings", "CapsuleRadiusLabel", "Capsule Radius"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("AttenuationSettings", "CapsuleRadiusLabel", "Capsule Radius"))
			.ToolTipText(NSLOCTEXT("AttenuationSettings", "CapsuleRadiusToolTip", "The attenuation capsule's radius."))
			.Font(StructCustomizationUtils.GetRegularFont())
		]
	.ValueContent()
		[
			ExtentYHandle->CreatePropertyValueWidget()
		]
	.Visibility(TAttribute<EVisibility>(this, &FAttenuationSettingsCustomization::IsCapsuleSelected));

	ChildBuilder.AddChildContent(NSLOCTEXT("AttenuationSettings", "ConeRadiusLabel", "Cone Radius"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("AttenuationSettings", "ConeRadiusLabel", "Cone Radius"))
			.ToolTipText(NSLOCTEXT("AttenuationSettings", "ConeRadiusToolTip", "The attenuation cone's radius."))
			.Font(StructCustomizationUtils.GetRegularFont())
		]
	.ValueContent()
		[
			ExtentXHandle->CreatePropertyValueWidget()
		]
	.Visibility(TAttribute<EVisibility>(this, &FAttenuationSettingsCustomization::IsConeSelected));

	ChildBuilder.AddChildContent(NSLOCTEXT("AttenuationSettings", "ConeAngleLabel", "Cone Angle"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("AttenuationSettings", "ConeAngleLabel", "Cone Angle"))
			.ToolTipText(NSLOCTEXT("AttenuationSettings", "ConeAngleToolTip", "The angle of the inner edge of the attenuation cone's falloff. Inside this angle sounds will be at full volume."))
			.Font(StructCustomizationUtils.GetRegularFont())
		]
	.ValueContent()
		[
			ExtentYHandle->CreatePropertyValueWidget()
		]
	.Visibility(TAttribute<EVisibility>(this, &FAttenuationSettingsCustomization::IsConeSelected));

	ChildBuilder.AddChildContent(NSLOCTEXT("AttenuationSettings", "ConeFalloffAngleLabel", "Cone Falloff Angle"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("AttenuationSettings", "ConeFalloffAngleLabel", "Cone Falloff Angle"))
			.ToolTipText(NSLOCTEXT("AttenuationSettings", "ConeFalloffAngleToolTip", "The angle of the outer edge of the attenuation cone's falloff. Outside this angle sounds will be inaudible."))
			.Font(StructCustomizationUtils.GetRegularFont())
		]
	.ValueContent()
		[
			ExtentZHandle->CreatePropertyValueWidget()
		]
	.Visibility(TAttribute<EVisibility>(this, &FAttenuationSettingsCustomization::IsConeSelected));

	IDetailPropertyRow& ConeOffsetRow = ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, ConeOffset)).ToSharedRef());
	ConeOffsetRow.Visibility(TAttribute<EVisibility>(this, &FAttenuationSettingsCustomization::IsConeSelected));

	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, FalloffDistance)).ToSharedRef());
	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, OmniRadius)).ToSharedRef());
	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, StereoSpread)).ToSharedRef());
	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, bAttenuateWithLPF)).ToSharedRef());
	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, LPFRadiusMin)).ToSharedRef());
	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, LPFRadiusMax)).ToSharedRef());
	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, LPFFrequencyAtMin)).ToSharedRef());
	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, LPFFrequencyAtMax)).ToSharedRef());

	bIsSpatializedHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, bSpatialize)).ToSharedRef();
	bIsFocusedHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, bEnableListenerFocus)).ToSharedRef();
	bIsOcclussionEnabledHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, bEnableOcclusion)).ToSharedRef();

	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, bEnableListenerFocus)).ToSharedRef());

	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, FocusAzimuth)).ToSharedRef())
		.EditCondition(GetIsFocusEnabledAttribute(), nullptr);

	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, NonFocusAzimuth)).ToSharedRef())
		.EditCondition(GetIsFocusEnabledAttribute(), nullptr);

	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, FocusDistanceScale)).ToSharedRef())
		.EditCondition(GetIsFocusEnabledAttribute(), nullptr);

	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, NonFocusDistanceScale)).ToSharedRef())
		.EditCondition(GetIsFocusEnabledAttribute(), nullptr);

	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, FocusPriorityScale)).ToSharedRef())
		.EditCondition(GetIsFocusEnabledAttribute(), nullptr);

	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, NonFocusPriorityScale)).ToSharedRef())
		.EditCondition(GetIsFocusEnabledAttribute(), nullptr);

	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, FocusVolumeAttenuation)).ToSharedRef())
		.EditCondition(GetIsFocusEnabledAttribute(), nullptr);

	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, NonFocusVolumeAttenuation)).ToSharedRef())
		.EditCondition(GetIsFocusEnabledAttribute(), nullptr);

	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, bEnableOcclusion)).ToSharedRef());

	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, OcclusionTraceChannel)).ToSharedRef())
		.EditCondition(GetIsOcclusionEnabledAttribute(), nullptr);

	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, OcclusionLowPassFilterFrequency)).ToSharedRef())
		.EditCondition(GetIsOcclusionEnabledAttribute(), nullptr);

	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, OcclusionVolumeAttenuation)).ToSharedRef())
		.EditCondition(GetIsOcclusionEnabledAttribute(), nullptr);

	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, OcclusionInterpolationTime)).ToSharedRef())
		.EditCondition(GetIsOcclusionEnabledAttribute(), nullptr);

	ChildBuilder.AddChildProperty(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FAttenuationSettings, bUseComplexCollisionForOcclusion)).ToSharedRef())
		.EditCondition(GetIsOcclusionEnabledAttribute(), nullptr);

	if (PropertyHandles.Num() != 32)
	{
		FString PropertyList;
		for (auto It(PropertyHandles.CreateConstIterator()); It; ++It)
		{
			PropertyList += It.Key().ToString() + TEXT(", ");
		}
		ensureMsgf(false, TEXT("Unexpected property handle(s) customizing FAttenuationSettings: %s"), *PropertyList);
	}
}

bool FAttenuationSettingsCustomization::IsFocusedEnabled() const
{
	bool bIsFocusEnabled;
	bIsFocusedHandle->GetValue(bIsFocusEnabled);
	if (!bIsFocusEnabled)
	{
		return false;
	}

	bool bIsSpatialized;
	bIsSpatializedHandle->GetValue(bIsSpatialized);

	return bIsSpatialized;
}

TAttribute<bool> FAttenuationSettingsCustomization::GetIsFocusEnabledAttribute() const
{
	return TAttribute<bool>(this, &FAttenuationSettingsCustomization::IsFocusedEnabled);
}

bool FAttenuationSettingsCustomization::IsOcclusionEnabled() const
{
	bool bIsOcclussionEnabled;
	bIsOcclussionEnabledHandle->GetValue(bIsOcclussionEnabled);
	if (!bIsOcclussionEnabled)
	{
		return false;
	}

	bool bIsSpatialized;
	bIsSpatializedHandle->GetValue(bIsSpatialized);

	return bIsSpatialized;
}

TAttribute<bool> FAttenuationSettingsCustomization::GetIsOcclusionEnabledAttribute() const
{
	return TAttribute<bool>(this, &FAttenuationSettingsCustomization::IsOcclusionEnabled);
}

EVisibility FAttenuationSettingsCustomization::IsConeSelected() const
{
	uint8 AttenuationShapeValue;
	AttenuationShapeHandle->GetValue(AttenuationShapeValue);

	const EAttenuationShape::Type AttenuationShape = (EAttenuationShape::Type)AttenuationShapeValue;

	return (AttenuationShape == EAttenuationShape::Cone ? EVisibility::Visible : EVisibility::Hidden);
}

EVisibility FAttenuationSettingsCustomization::IsSphereSelected() const
{
	uint8 AttenuationShapeValue;
	AttenuationShapeHandle->GetValue(AttenuationShapeValue);

	const EAttenuationShape::Type AttenuationShape = (EAttenuationShape::Type)AttenuationShapeValue;

	return (AttenuationShape == EAttenuationShape::Sphere ? EVisibility::Visible : EVisibility::Hidden);
}

EVisibility FAttenuationSettingsCustomization::IsBoxSelected() const
{
	uint8 AttenuationShapeValue;
	AttenuationShapeHandle->GetValue(AttenuationShapeValue);

	const EAttenuationShape::Type AttenuationShape = (EAttenuationShape::Type)AttenuationShapeValue;

	return (AttenuationShape == EAttenuationShape::Box ? EVisibility::Visible : EVisibility::Hidden);
}

EVisibility FAttenuationSettingsCustomization::IsCapsuleSelected() const
{
	uint8 AttenuationShapeValue;
	AttenuationShapeHandle->GetValue(AttenuationShapeValue);

	const EAttenuationShape::Type AttenuationShape = (EAttenuationShape::Type)AttenuationShapeValue;

	return (AttenuationShape == EAttenuationShape::Capsule ? EVisibility::Visible : EVisibility::Hidden);
}

EVisibility FAttenuationSettingsCustomization::IsNaturalSoundSelected() const
{
	uint8 DistanceAlgorithmValue;
	DistanceAlgorithmHandle->GetValue(DistanceAlgorithmValue);

	const ESoundDistanceModel DistanceAlgorithm = (ESoundDistanceModel)DistanceAlgorithmValue;

	return (DistanceAlgorithm == ATTENUATION_NaturalSound ? EVisibility::Visible : EVisibility::Hidden);
}

EVisibility FAttenuationSettingsCustomization::IsCustomCurveSelected() const
{
	uint8 DistanceAlgorithmValue;
	DistanceAlgorithmHandle->GetValue(DistanceAlgorithmValue);

	const ESoundDistanceModel DistanceAlgorithm = (ESoundDistanceModel)DistanceAlgorithmValue;

	return (DistanceAlgorithm == ATTENUATION_Custom ? EVisibility::Visible : EVisibility::Hidden);
}
