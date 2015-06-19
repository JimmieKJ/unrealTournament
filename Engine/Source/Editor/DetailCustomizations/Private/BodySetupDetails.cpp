// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "BodySetupDetails.h"
#include "ScopedTransaction.h"
#include "ObjectEditorUtils.h"
#include "IDocumentation.h"
#include "PhysicsEngine/BodySetup.h"

#define LOCTEXT_NAMESPACE "BodySetupDetails"

TSharedRef<IDetailCustomization> FBodySetupDetails::MakeInstance()
{
	return MakeShareable( new FBodySetupDetails );
}

void FBodySetupDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	// Customize collision section
	{
		if ( DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBodySetup, DefaultInstance))->IsValidHandle() )
		{
			IDetailCategoryBuilder& PhysicsCategory = DetailBuilder.EditCategory("Physics");
			IDetailCategoryBuilder& CollisionCategory = DetailBuilder.EditCategory("Collision");

			TSharedPtr<IPropertyHandle> BodyInstanceHandler = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBodySetup, DefaultInstance));
			DetailBuilder.HideProperty(BodyInstanceHandler);

			TSharedPtr<IPropertyHandle> CollisionTraceHandler = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBodySetup, CollisionTraceFlag));
			DetailBuilder.HideProperty(CollisionTraceHandler);

			// add physics properties to physics category
			uint32 NumChildren = 0;
			BodyInstanceHandler->GetNumChildren(NumChildren);

			// Get the objects being customized so we can enable/disable editing of 'Simulate Physics'
			DetailBuilder.GetObjectsBeingCustomized(ObjectsCustomized);

			// add all properties of this now - after adding 
			for (uint32 ChildIndex=0; ChildIndex < NumChildren; ++ChildIndex)
			{
				TSharedPtr<IPropertyHandle> ChildProperty = BodyInstanceHandler->GetChildHandle(ChildIndex);
				FString Category = FObjectEditorUtils::GetCategory(ChildProperty->GetProperty());
				if (ChildProperty->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FBodyInstance, bSimulatePhysics)
					|| ChildProperty->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FBodyInstance, bAutoWeld)
					|| ChildProperty->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FBodyInstance, DOFMode)
					|| ChildProperty->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FBodyInstance, CustomDOFPlaneNormal)
					|| ChildProperty->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FBodyInstance, bLockTranslation)
					|| ChildProperty->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FBodyInstance, bLockXTranslation)
					|| ChildProperty->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FBodyInstance, bLockYTranslation)
					|| ChildProperty->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FBodyInstance, bLockZTranslation)
					|| ChildProperty->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FBodyInstance, bLockRotation)
					|| ChildProperty->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FBodyInstance, bLockXRotation)
					|| ChildProperty->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FBodyInstance, bLockYRotation)
					|| ChildProperty->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FBodyInstance, bLockZRotation))
				{
					// skip bSimulatePhysics
					// this is because we don't want bSimulatePhysics to show up 
					// phat editor 
					// staitc mesh already hides everything else not interested in
					// so phat editor just should not show this option
					//also hide bAutoWeld for phat
					continue;
				}
				else if (ChildProperty->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FBodyInstance, MassInKg))
				{
					PhysicsCategory.AddCustomRow(LOCTEXT("MassLabel", "Mass"), false)
						.IsEnabled(TAttribute<bool>(this, &FBodySetupDetails::IsBodyMassEnabled))
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
								.Text(this, &FBodySetupDetails::OnGetBodyMass)
								.IsReadOnly(this, &FBodySetupDetails::IsBodyMassReadOnly)
								.Font(IDetailLayoutBuilder::GetDetailFont())
								.Visibility(this, &FBodySetupDetails::IsMassVisible, false)
							]

							+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(SVerticalBox)
									.Visibility(this, &FBodySetupDetails::IsMassVisible, true)
									+ SVerticalBox::Slot()
									.AutoHeight()
									[
										ChildProperty->CreatePropertyValueWidget()
									]
								]

						];

					continue;
				}
				if (Category == TEXT("Physics"))
				{
					PhysicsCategory.AddProperty(ChildProperty);
				}
				else if (Category == TEXT("Collision"))
				{
					CollisionCategory.AddProperty(ChildProperty);
				}
			}
		}
	}
}

FText FBodySetupDetails::OnGetBodyMass() const
{
	UBodySetup* BS = NULL;

	float Mass = 0.0f;
	bool bMultipleValue = false;

	for (auto ObjectIt = ObjectsCustomized.CreateConstIterator(); ObjectIt; ++ObjectIt)
	{
		if(ObjectIt->IsValid() && (*ObjectIt)->IsA(UBodySetup::StaticClass()))
		{
			BS = Cast<UBodySetup>(ObjectIt->Get());
			
			float BSMass = BS->CalculateMass();
			if (Mass == 0.0f || FMath::Abs(Mass - BSMass) < 0.1f)
			{
				Mass = BSMass;
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

EVisibility FBodySetupDetails::IsMassVisible(bool bOverrideMass) const
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


bool FBodySetupDetails::IsBodyMassReadOnly() const
{
	for (auto ObjectIt = ObjectsCustomized.CreateConstIterator(); ObjectIt; ++ObjectIt)
	{
		if (ObjectIt->IsValid() && (*ObjectIt)->IsA(UBodySetup::StaticClass()))
		{
			UBodySetup* BS = Cast<UBodySetup>(ObjectIt->Get());
			if (BS->DefaultInstance.bOverrideMass == false)
			{
				return true;
			}
		}
	}

	return false;
}


#undef LOCTEXT_NAMESPACE

