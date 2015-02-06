// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "PrimitiveComponentDetails.h"
#include "ScopedTransaction.h"
#include "ObjectEditorUtils.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine//BodyInstance.h"
#include "Components/DestructibleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "IDocumentation.h"
#include "EditorCategoryUtils.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "ComponentMaterialCategory.h"

#define LOCTEXT_NAMESPACE "PrimitiveComponentDetails"

//////////////////////////////////////////////////////////////
// This class customizes collision setting in primitive component
//////////////////////////////////////////////////////////////

TSharedRef<IDetailCustomization> FPrimitiveComponentDetails::MakeInstance()
{
	return MakeShareable( new FPrimitiveComponentDetails );
}

bool FPrimitiveComponentDetails::IsSimulatePhysicsEditable() const
{
	// Check whether to enable editing of bSimulatePhysics - this will happen if all objects are UPrimitiveComponents & have collision geometry.
	bool bEnableSimulatePhysics = ObjectsCustomized.Num() > 0;
	for (TWeakObjectPtr<UObject> CustomizedObject : ObjectsCustomized)
	{
		if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(CustomizedObject.Get()))
		{
			if (!PrimitiveComponent->CanEditSimulatePhysics())
			{
				bEnableSimulatePhysics = false;
				break;
			}
		}
	}

	return bEnableSimulatePhysics;
}

bool FPrimitiveComponentDetails::IsUseAsyncEditable() const
{
	// Check whether to enable editing of bUseAsyncScene - this will happen if all objects are movable and the project uses an AsyncScene
	if (!UPhysicsSettings::Get()->bEnableAsyncScene)
	{
		return false;
	}
	
	bool bEnableUseAsyncScene = ObjectsCustomized.Num() > 0;
	for (auto ObjectIt = ObjectsCustomized.CreateConstIterator(); ObjectIt; ++ObjectIt)
	{
		if (ObjectIt->IsValid() && (*ObjectIt)->IsA(UPrimitiveComponent::StaticClass()))
		{
			TWeakObjectPtr<USceneComponent> SceneComponent = CastChecked<USceneComponent>(ObjectIt->Get());

			if (SceneComponent.IsValid() && SceneComponent->Mobility != EComponentMobility::Movable)
			{
				bEnableUseAsyncScene = false;
				break;
			}

			// Skeletal mesh uses a physics asset which will have multiple bodies - these bodies have their own bUseAsyncScene which is what we actually use - the flag on the skeletal mesh is not used
			TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent = Cast<USkeletalMeshComponent>(ObjectIt->Get());
			if (SkeletalMeshComponent.IsValid())
			{
				bEnableUseAsyncScene = false;
				break;
			}
		}
		else
		{
			bEnableUseAsyncScene = false;
			break;
		}
	}

	return bEnableUseAsyncScene;
}

EVisibility FPrimitiveComponentDetails::IsCustomLockedAxisSelected() const
{
	bool bVisible = false;
	if (LockedAxisProperty.IsValid())
	{
		uint8 LockedAxis;
		if (LockedAxisProperty->GetValue(LockedAxis) == FPropertyAccess::Success)
		{
			bVisible = static_cast<ELockedAxis::Type>(LockedAxis) == ELockedAxis::Custom;
		}
	}

	return bVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FPrimitiveComponentDetails::IsLockAxisEnabled() const
{
	bool bVisible = false;
	if (LockedAxisProperty.IsValid())
	{
		uint8 LockedAxis;
		if (LockedAxisProperty->GetValue(LockedAxis) == FPropertyAccess::Success)
		{
			bVisible = static_cast<ELockedAxis::Type>(LockedAxis) != ELockedAxis::Default;
		}
	}

	return bVisible ? EVisibility::Visible : EVisibility::Collapsed;
}


bool FPrimitiveComponentDetails::IsAutoWeldEditable() const
{
	for (int32 i = 0; i < ObjectsCustomized.Num(); ++i)
	{
		if (UPrimitiveComponent * SceneComponent = Cast<UPrimitiveComponent>(ObjectsCustomized[i].Get()))
		{
			if (FBodyInstance* BI = SceneComponent->GetBodyInstance())
			{
				if (BI->IsInstanceSimulatingPhysics())
				{
					return false;
				}
			}
		}
	}

	return true;
}

EVisibility FPrimitiveComponentDetails::IsAutoWeldVisible() const
{
	for (int32 i = 0; i < ObjectsCustomized.Num(); ++i)
	{
		if (ObjectsCustomized[i].IsValid() && !ObjectsCustomized[i]->IsA(UStaticMeshComponent::StaticClass()))
		{
			return EVisibility::Collapsed;
		}
	}

	return EVisibility::Visible;
}

EVisibility FPrimitiveComponentDetails::IsMassVisible(bool bOverrideMass) const
{
	bool bIsMassReadOnly = IsBodyMassReadOnly();
	if (bOverrideMass)
	{
		return bIsMassReadOnly ? EVisibility::Collapsed : EVisibility::Visible;
	}
	else
	{
		return bIsMassReadOnly ? EVisibility::Visible : EVisibility::Collapsed;
	}
}

void FPrimitiveComponentDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	// Get the objects being customized so we can enable/disable editing of 'Simulate Physics'
	DetailBuilder.GetObjectsBeingCustomized(ObjectsCustomized);

	// See if we are hiding Physics category
	TArray<FString> HideCategories;
	FEditorCategoryUtils::GetClassHideCategories(DetailBuilder.GetDetailsView().GetBaseClass(), HideCategories);

	if(!HideCategories.Contains("Materials"))
	{
		AddMaterialCategory(DetailBuilder);
	}

	TSharedRef<IPropertyHandle> MobilityHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, Mobility), USceneComponent::StaticClass());
	MobilityHandle->SetToolTipText(LOCTEXT("PrimitiveMobilityTooltip", "Mobility for primitive components controls how they can be modified in game and therefore how they interact with lighting and physics.\n● A movable primitive component can be changed in game, but requires dynamic lighting and shadowing from lights which have a large performance cost.\n● A static primitive component can't be changed in game, but can have its lighting baked, which allows rendering to be very efficient."));

	if ( DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, BodyInstance))->IsValidHandle() )
	{
		TSharedPtr<IPropertyHandle> BodyInstanceHandler = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, BodyInstance));
		uint32 NumChildren = 0;
		BodyInstanceHandler->GetNumChildren(NumChildren);


		if (!HideCategories.Contains(TEXT("Physics")))
		{
			IDetailCategoryBuilder& PhysicsCategory = DetailBuilder.EditCategory("Physics");



			bool bDisplayMass = true;
			bool bDisplayMassOverride = true;

			for (int32 i = 0; i < ObjectsCustomized.Num(); ++i)
			{
				if (ObjectsCustomized[i].IsValid() && ObjectsCustomized[i]->IsA(UDestructibleComponent::StaticClass()))
				{
					bDisplayMass = false;
					bDisplayMassOverride = false;
				}

				if (ObjectsCustomized[i].IsValid() && ObjectsCustomized[i]->IsA(USkeletalMeshComponent::StaticClass()))
				{
					bDisplayMassOverride = false;
				}
			}

			// add all physics properties now - after adding mass
			for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
			{
				TSharedPtr<IPropertyHandle> ChildProperty = BodyInstanceHandler->GetChildHandle(ChildIndex);
				FString Category = FObjectEditorUtils::GetCategory(ChildProperty->GetProperty());
				FName PropName = ChildProperty->GetProperty()->GetFName();
				if (Category == TEXT("Physics"))
				{
					// Only permit modifying bSimulatePhysics when the body has some geometry.
					if (PropName == GET_MEMBER_NAME_CHECKED(FBodyInstance, bSimulatePhysics))
					{
						PhysicsCategory.AddProperty(ChildProperty).EditCondition(TAttribute<bool>(this, &FPrimitiveComponentDetails::IsSimulatePhysicsEditable), NULL);
					}
					else if (PropName == FName("bUseAsyncScene")) // Can't use GET_MEMBER_NAME_CHECKED because protected
					{
						//we only enable bUseAsyncScene if the project uses an AsyncScene
						PhysicsCategory.AddProperty(ChildProperty).EditCondition(TAttribute<bool>(this, &FPrimitiveComponentDetails::IsUseAsyncEditable), NULL);
					}
					else if (PropName == GET_MEMBER_NAME_CHECKED(FBodyInstance, LockedAxisMode))
					{
						LockedAxisProperty = ChildProperty;
						PhysicsCategory.AddProperty(ChildProperty);
					}
					else if (PropName == GET_MEMBER_NAME_CHECKED(FBodyInstance, CustomLockedAxis))
					{
						//we only enable bUseAsyncScene if the project uses an AsyncScene
						PhysicsCategory.AddProperty(ChildProperty).Visibility(TAttribute<EVisibility>(this, &FPrimitiveComponentDetails::IsCustomLockedAxisSelected));
					}
					else if (PropName == GET_MEMBER_NAME_CHECKED(FBodyInstance, bLockTranslation) || PropName == GET_MEMBER_NAME_CHECKED(FBodyInstance, bLockRotation))
					{
						// Only show the lock translation/rotation options if some lock axis is selected
						PhysicsCategory.AddProperty(ChildProperty).Visibility(TAttribute<EVisibility>(this, &FPrimitiveComponentDetails::IsLockAxisEnabled));
					}
					else if (PropName == GET_MEMBER_NAME_CHECKED(FBodyInstance, bAutoWeld))
					{
						PhysicsCategory.AddProperty(ChildProperty).Visibility(TAttribute<EVisibility>(this, &FPrimitiveComponentDetails::IsAutoWeldVisible))
																  .EditCondition(TAttribute<bool>(this, &FPrimitiveComponentDetails::IsAutoWeldEditable), NULL);
					}
					else if (PropName == GET_MEMBER_NAME_CHECKED(FBodyInstance, bOverrideMass))
					{
						if (bDisplayMassOverride)
						{
							PhysicsCategory.AddProperty(ChildProperty);
						}
					}
					else if (PropName == GET_MEMBER_NAME_CHECKED(FBodyInstance, MassInKg))
					{
						
						if (bDisplayMass)
						{
							PhysicsCategory.AddCustomRow(LOCTEXT("Mass", "Mass"), false)
								.IsEnabled(TAttribute<bool>(this, &FPrimitiveComponentDetails::IsBodyMassEnabled))
								.NameContent()
								[
									ChildProperty->CreatePropertyNameWidget()
								]
							.ValueContent()
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot()
									.AutoHeight()
									[
										SNew(SEditableTextBox)
										.Text(this, &FPrimitiveComponentDetails::OnGetBodyMass)
										.IsReadOnly(this, &FPrimitiveComponentDetails::IsBodyMassReadOnly)
										.Font(IDetailLayoutBuilder::GetDetailFont())
										.Visibility(this, &FPrimitiveComponentDetails::IsMassVisible, false)
									]

									+ SVerticalBox::Slot()
									.AutoHeight()
									[
										SNew(SVerticalBox)
										.Visibility(this, &FPrimitiveComponentDetails::IsMassVisible, true)
										+ SVerticalBox::Slot()
										.AutoHeight()
										[
											ChildProperty->CreatePropertyValueWidget()
										]
									]
									
								];
						}
					}
					else
					{
						PhysicsCategory.AddProperty(ChildProperty);
					}
				}
			}
		}

		// Collision
		{
			IDetailCategoryBuilder& CollisionCategory = DetailBuilder.EditCategory("Collision");

			// add all collision properties
			for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
			{
				TSharedPtr<IPropertyHandle> ChildProperty = BodyInstanceHandler->GetChildHandle(ChildIndex);
				FString Category = FObjectEditorUtils::GetCategory(ChildProperty->GetProperty());
				if (Category == TEXT("Collision"))
				{
					CollisionCategory.AddProperty(ChildProperty);
				}
			}
		}		
	}


	AddAdvancedSubCategory( DetailBuilder, "Rendering", "TextureStreaming" );
	AddAdvancedSubCategory( DetailBuilder, "Rendering", "LOD");
}

void FPrimitiveComponentDetails::AddMaterialCategory( IDetailLayoutBuilder& DetailBuilder )
{
	TArray<TWeakObjectPtr<USceneComponent> > Components;

	for( TWeakObjectPtr<UObject> Object : ObjectsCustomized )
	{
		USceneComponent*  Component = Cast<USceneComponent>(Object.Get());
		if( Component )
		{
			Components.Add( Component );
		}
	}

	MaterialCategory = MakeShareable(new FComponentMaterialCategory(Components));
	MaterialCategory->Create(DetailBuilder);
}
void FPrimitiveComponentDetails::AddAdvancedSubCategory( IDetailLayoutBuilder& DetailBuilder, FName MainCategoryName, FName SubCategoryName)
{
	TArray<TSharedRef<IPropertyHandle> > SubCategoryProperties;
	IDetailCategoryBuilder& SubCategory = DetailBuilder.EditCategory(SubCategoryName);

	const bool bSimpleProperties = false;
	const bool bAdvancedProperties = true;
	SubCategory.GetDefaultProperties( SubCategoryProperties, bSimpleProperties, bAdvancedProperties );

	if( SubCategoryProperties.Num() > 0 )
	{
		IDetailCategoryBuilder& MainCategory = DetailBuilder.EditCategory(MainCategoryName);

		const bool bForAdvanced = true;
		IDetailGroup& Group = MainCategory.AddGroup( SubCategoryName, FText::FromName(SubCategoryName), bForAdvanced );

		for( int32 PropertyIndex = 0; PropertyIndex < SubCategoryProperties.Num(); ++PropertyIndex )
		{
			TSharedRef<IPropertyHandle>& PropertyHandle = SubCategoryProperties[PropertyIndex];

			// Ignore customized properties
			if( !PropertyHandle->IsCustomized() )
			{
				Group.AddPropertyRow( SubCategoryProperties[PropertyIndex] );
			}
		}
	}

}

FText FPrimitiveComponentDetails::OnGetBodyMass() const
{
	UPrimitiveComponent* Comp = NULL;

	float Mass = 0.0f;
	bool bMultipleValue = false;

	for (auto ObjectIt = ObjectsCustomized.CreateConstIterator(); ObjectIt; ++ObjectIt)
	{
		if(ObjectIt->IsValid() && (*ObjectIt)->IsA(UPrimitiveComponent::StaticClass()))
		{
			Comp = Cast<UPrimitiveComponent>(ObjectIt->Get());

			float CompMass = Comp->CalculateMass();
			if (Mass == 0.0f || FMath::Abs(Mass - CompMass) < 0.1f)
			{
				Mass = CompMass;
			}
			else
			{
				bMultipleValue = true;
				break;
			}
		}
	}

	if (bMultipleValue)
	{
		return LOCTEXT("MultipleValues", "Multiple Values");
	}

	return FText::AsNumber(Mass);
}

bool FPrimitiveComponentDetails::IsBodyMassReadOnly() const
{
	for (auto ObjectIt = ObjectsCustomized.CreateConstIterator(); ObjectIt; ++ObjectIt)
	{
		if (ObjectIt->IsValid() && (*ObjectIt)->IsA(UPrimitiveComponent::StaticClass()))
		{
			if (UPrimitiveComponent* Comp = Cast<UPrimitiveComponent>(ObjectIt->Get()))
			{
				if (Comp->BodyInstance.bOverrideMass == false) { return true; }
			}
		}
	}

	return false;
}


#undef LOCTEXT_NAMESPACE

