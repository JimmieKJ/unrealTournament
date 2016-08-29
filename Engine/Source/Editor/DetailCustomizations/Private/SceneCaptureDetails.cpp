// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "Runtime/Engine/Public/ShowFlags.h"
#include "SceneCaptureDetails.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/SceneCaptureCube.h"
#include "Engine/PlanarReflection.h"
#include "Components/SceneCaptureComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneCaptureComponentCube.h"
#include "Components/PlanarReflectionComponent.h"

#define LOCTEXT_NAMESPACE "SceneCaptureDetails"

TSharedRef<IDetailCustomization> FSceneCaptureDetails::MakeInstance()
{
	return MakeShareable( new FSceneCaptureDetails );
}

// Used to sort the show flags inside of their categories in the order of the text that is actually displayed
inline static bool SortAlphabeticallyByLocalizedText(const FString& ip1, const FString& ip2)
{
	FText LocalizedText1;
	FEngineShowFlags::FindShowFlagDisplayName(ip1, LocalizedText1);

	FText LocalizedText2;
	FEngineShowFlags::FindShowFlagDisplayName(ip2, LocalizedText2);

	return LocalizedText1.ToString() < LocalizedText2.ToString();
}

void FSceneCaptureDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	// Add all the properties that are there by default
	// (These would get added by default anyway, but we want to add them first so what we add next comes later in the list)
	TArray<TSharedRef<IPropertyHandle>> SceneCaptureCategoryDefaultProperties;
	IDetailCategoryBuilder& SceneCaptureCategoryBuilder = DetailLayout.EditCategory("SceneCapture");
	SceneCaptureCategoryBuilder.GetDefaultProperties(SceneCaptureCategoryDefaultProperties);

	for (TSharedRef<IPropertyHandle> Handle : SceneCaptureCategoryDefaultProperties)
	{
		SceneCaptureCategoryBuilder.AddProperty(Handle);
	}

	const TArray< TWeakObjectPtr<UObject> >& SelectedObjects = DetailLayout.GetDetailsView().GetSelectedObjects();

	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		const TWeakObjectPtr<UObject>& CurrentObject = SelectedObjects[ObjectIndex];
		if ( CurrentObject.IsValid() )
		{
			ASceneCapture2D* CurrentCaptureActor2D = Cast<ASceneCapture2D>(CurrentObject.Get());
			if (CurrentCaptureActor2D != nullptr)
			{
				SceneCaptureComponent = Cast<USceneCaptureComponent>(CurrentCaptureActor2D->GetCaptureComponent2D());
				break;
			}
			ASceneCaptureCube* CurrentCaptureActorCube = Cast<ASceneCaptureCube>(CurrentObject.Get());
			if (CurrentCaptureActorCube != nullptr)
			{
				SceneCaptureComponent = Cast<USceneCaptureComponent>(CurrentCaptureActorCube->GetCaptureComponentCube());
				break;
			}
			APlanarReflection* CurrentPlanarReflection = Cast<APlanarReflection>(CurrentObject.Get());
			if (CurrentPlanarReflection != nullptr)
			{
				SceneCaptureComponent = Cast<USceneCaptureComponent>(CurrentPlanarReflection->GetPlanarReflectionComponent());
				break;
			}
		}
	}

	// Show flags that should be exposed for Scene Captures
	TArray<FEngineShowFlags::EShowFlag> ShowFlagsToAllowForCaptures;

	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_AtmosphericFog);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_BSP);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_Decals);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_Fog);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_Landscape);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_Particles);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_SkeletalMeshes);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_StaticMeshes);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_Translucency);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_DeferredLighting);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_InstancedStaticMeshes);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_InstancedFoliage);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_InstancedGrass);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_Paper2DSprites);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_TextRender);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_AmbientOcclusion);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_DynamicShadows);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_SkyLighting);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_AmbientCubemap);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_DistanceFieldAO);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_LightFunctions);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_LightShafts);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_ReflectionEnvironment);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_ScreenSpaceReflections);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_TexturedLightProfiles);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_AntiAliasing);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_TemporalAA);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_MotionBlur);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_Bloom);
	ShowFlagsToAllowForCaptures.Add(FEngineShowFlags::EShowFlag::SF_EyeAdaptation);
	
	// Create array of flag name strings for each group
	TArray< TArray<FString> > ShowFlagsByGroup;
	for (int32 GroupIndex = 0; GroupIndex < SFG_Max; ++GroupIndex)
	{
		ShowFlagsByGroup.Add(TArray<FString>());
	}

	// Add the show flags we want to expose to their group's array
	for (FEngineShowFlags::EShowFlag AllowedFlag : ShowFlagsToAllowForCaptures)
	{
		FString FlagName;
		FlagName = FEngineShowFlags::FindNameByIndex(AllowedFlag);
		if (!FlagName.IsEmpty())
		{
			EShowFlagGroup Group = FEngineShowFlags::FindShowFlagGroup(*FlagName);
			ShowFlagsByGroup[Group].Add(FlagName);
		}
	}

	// Sort the flags in their respective group alphabetically
	for (TArray<FString>& ShowFlagGroup : ShowFlagsByGroup)
	{
		ShowFlagGroup.Sort(SortAlphabeticallyByLocalizedText);
	}

	// Add each group
	for (int32 GroupIndex = 0; GroupIndex < SFG_Max; ++GroupIndex)
	{
		// Don't add a group if there are no flags allowed for it
		if (ShowFlagsByGroup[GroupIndex].Num() >= 1)
		{
			FText GroupName;
			FText GroupTooltip;
			switch (GroupIndex)
			{
			case SFG_Normal:
				GroupName = LOCTEXT("CommonShowFlagHeader", "General Show Flags");
				break;
			case SFG_Advanced:
				GroupName = LOCTEXT("AdvancedShowFlagsMenu", "Advanced Show Flags");
				break;
			case SFG_PostProcess:
				GroupName = LOCTEXT("PostProcessShowFlagsMenu", "Post Processing Show Flags");
				break;
			case SFG_Developer:
				GroupName = LOCTEXT("DeveloperShowFlagsMenu", "Developer Show Flags");
				break;
			case SFG_Visualize:
				GroupName = LOCTEXT("VisualizeShowFlagsMenu", "Visualize Show Flags");
				break;
			case SFG_LightTypes:
				GroupName = LOCTEXT("LightTypesShowFlagsMenu", "Light Types Show Flags");
				break;
			case SFG_LightingComponents:
				GroupName = LOCTEXT("LightingComponentsShowFlagsMenu", "Lighting Components Show Flags");
				break;
			case SFG_LightingFeatures:
				GroupName = LOCTEXT("LightingFeaturesShowFlagsMenu", "Lighting Features Show Flags");
				break;
			case SFG_CollisionModes:
				GroupName = LOCTEXT("CollisionModesShowFlagsMenu", "Collision Modes Show Flags");
				break;
			case SFG_Hidden:
				GroupName = LOCTEXT("HiddenShowFlagsMenu", "Hidden Show Flags");
				break;
			default:
				// Should not get here unless a new group is added without being updated here
				GroupName = LOCTEXT("MiscFlagsMenu", "Misc Show Flags");
				break;
			}

			FName GroupFName = FName(*(GroupName.ToString()));
			IDetailGroup& Group = SceneCaptureCategoryBuilder.AddGroup(GroupFName, GroupName, true);

			// Add each show flag for this group
			for (FString& FlagName : ShowFlagsByGroup[GroupIndex])
			{
				bool bFlagHidden = false;
				FText LocalizedText;
				FEngineShowFlags::FindShowFlagDisplayName(FlagName, LocalizedText);

				Group.AddWidgetRow()
					.IsEnabled(true)
					.NameContent()
					[
						SNew(STextBlock)
						.Text(LocalizedText)
					]
				.ValueContent()
					[
						SNew(SCheckBox)
						.OnCheckStateChanged(this, &FSceneCaptureDetails::OnShowFlagCheckStateChanged, FlagName)
						.IsChecked(this, &FSceneCaptureDetails::OnGetDisplayCheckState, FlagName)
					]
				.FilterString(LocalizedText);
			}
		}
	}
}

ECheckBoxState FSceneCaptureDetails::OnGetDisplayCheckState(FString ShowFlagName) const
{
	bool IsChecked = false;
	FEngineShowFlagsSetting* FlagSetting;
	if (SceneCaptureComponent->GetSettingForShowFlag(ShowFlagName, &FlagSetting))
	{
		IsChecked = FlagSetting->Enabled;
	}
	else
	{
		int32 SettingIndex = SceneCaptureComponent->ShowFlags.FindIndexByName(*(ShowFlagName));
		if (SettingIndex != INDEX_NONE)
		{
			IsChecked = SceneCaptureComponent->ShowFlags.GetSingleFlag(SettingIndex);
		}
	}

	return IsChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FSceneCaptureDetails::OnShowFlagCheckStateChanged(ECheckBoxState InNewRadioState, FString FlagName)
{
	SceneCaptureComponent->Modify();
	FEngineShowFlagsSetting* FlagSetting;
	bool bNewCheckState = (InNewRadioState == ECheckBoxState::Checked);

	// If setting exists, update it
	if (SceneCaptureComponent->GetSettingForShowFlag(FlagName, &FlagSetting))
	{
		FlagSetting->Enabled = bNewCheckState;
	}
	// Otherwise create a new setting
	else
	{
		FEngineShowFlagsSetting NewFlagSetting;
		NewFlagSetting.ShowFlagName = FlagName;
		NewFlagSetting.Enabled = bNewCheckState;
		SceneCaptureComponent->ShowFlagSettings.Add(NewFlagSetting);
	}

	// Ensure PostEditChangeProperty is called, which in turn calls update
	SceneCaptureComponent->PostEditChange();
}

#undef LOCTEXT_NAMESPACE
