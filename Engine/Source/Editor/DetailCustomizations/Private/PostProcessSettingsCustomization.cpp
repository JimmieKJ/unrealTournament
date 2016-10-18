// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "PostProcessSettingsCustomization.h"
#include "ObjectEditorUtils.h"
#include "SWidgetSwitcher.h"
#include "Classes/Engine/BlendableInterface.h"

#define LOCTEXT_NAMESPACE "PostProcessSettingsCustomization"

void FPostProcessSettingsCustomization::CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	TMap<FName, IDetailGroup*> CategoryNameToGroupMap;

	uint32 NumChildren = 0;
	FPropertyAccess::Result Result = StructPropertyHandle->GetNumChildren(NumChildren);

	UProperty* Prop = StructPropertyHandle->GetProperty();
	UStructProperty* StructProp = Cast<UStructProperty>(Prop);

	// a category with this name should be one level higher, should be "PostProcessSettings"
	FName ClassName = StructProp->Struct->GetFName();

	if( Result == FPropertyAccess::Success && NumChildren > 0)
	{
		for( uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
		{
			TSharedPtr<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle( ChildIndex );
			if( ChildHandle.IsValid() && ChildHandle->GetProperty() )
			{
				UProperty* Property = ChildHandle->GetProperty();
				FName Category = FObjectEditorUtils::GetCategoryFName( Property );

				if(Category == ClassName)
				{
					// Some elements should be outside of the categories, they need to specify the struct nama as category
					StructBuilder.AddChildProperty( ChildHandle.ToSharedRef() );
				}
				else
				{
					IDetailGroup* Group = CategoryNameToGroupMap.FindRef( Category );

					if( !Group )
					{
						Group = &StructBuilder.AddChildGroup( Category, FText::FromString( FName::NameToDisplayString( Category.ToString(), false ) ) );
						CategoryNameToGroupMap.Add( Category, Group );
					}
				
					Group->AddPropertyRow( ChildHandle.ToSharedRef() );
				}
			}
		}
	}
}



void FPostProcessSettingsCustomization::CustomizeHeader( TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	HeaderRow.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	];

	HeaderRow.ValueContent()
	[
		StructPropertyHandle->CreatePropertyValueWidget()
	];
}

void FWeightedBlendableCustomization::AddDirectAsset(TSharedRef<IPropertyHandle> StructPropertyHandle, UPackage* Package, TSharedPtr<IPropertyHandle> Weight, TSharedPtr<IPropertyHandle> Value, UClass* Class)
{
	Weight->SetValue(1.0f);

	{
		TArray<UObject*> Objects;
		StructPropertyHandle->GetOuterObjects(Objects);

		TArray<FString> Values;

		for(TArray<UObject*>::TConstIterator It = Objects.CreateConstIterator(); It; It++)
		{
			UObject* Obj = *It;

			const UObject* NewObj = NewObject<UObject>(Obj, Class);

			FString Str = NewObj->GetPathName();

			Values.Add(Str);
		}

		Value->SetPerObjectValues(Values);
	}
}

void FWeightedBlendableCustomization::AddIndirectAsset(TSharedPtr<IPropertyHandle> Weight)
{
	Weight->SetValue(1.0f);
}

EVisibility FWeightedBlendableCustomization::IsWeightVisible(TSharedPtr<IPropertyHandle> Weight) const
{
	float WeightValue = 1.0f;
	
	Weight->GetValue(WeightValue);

	return (WeightValue >= 0) ? EVisibility::Visible : EVisibility::Hidden;
}

FText FWeightedBlendableCustomization::GetDirectAssetName(TSharedPtr<IPropertyHandle> Value) const
{
	UObject* RefObject = 0;
	
	Value->GetValue(RefObject);

	check(RefObject);

	return FText::FromString(RefObject->GetFullName());
}

FReply FWeightedBlendableCustomization::JumpToDirectAsset(TSharedPtr<IPropertyHandle> Value)
{
	UObject* RefObject = 0;
	
	Value->GetValue(RefObject);

	FAssetEditorManager::Get().OpenEditorForAsset(RefObject);

	return FReply::Handled();
}

TSharedRef<SWidget> FWeightedBlendableCustomization::GenerateContentWidget(TSharedRef<IPropertyHandle> StructPropertyHandle, UPackage* Package, TSharedPtr<IPropertyHandle> Weight, TSharedPtr<IPropertyHandle> Value)
{
	bool bSeparatorIsNeeded = false; 

	FMenuBuilder MenuBuilder(true, NULL);
	{
		for(TObjectIterator<UClass> It; It; ++It)
		{
			if( It->IsChildOf(UFactory::StaticClass()))
			{
				UFactory* Factory = It->GetDefaultObject<UFactory>();

				check(Factory);

				UClass* SupportedClass = Factory->GetSupportedClass();

				if(SupportedClass)
				{
					if(SupportedClass->ImplementsInterface(UBlendableInterface::StaticClass()))
					{
						// At the moment we know about 3 Blendables: Material, UMaterialInstanceConstant, LightPropagationVolumeBlendable
						// The materials are not that useful to have here (hard to reference) so we suppress them here
						if(!(
							SupportedClass == UMaterial::StaticClass() ||
							SupportedClass == UMaterialInstanceConstant::StaticClass()
							))
						{
							FUIAction Direct2(FExecuteAction::CreateSP(this, &FWeightedBlendableCustomization::AddDirectAsset, StructPropertyHandle, Package, Weight, Value, SupportedClass));

							FName ClassName = SupportedClass->GetFName();
						
							MenuBuilder.AddMenuEntry(FText::FromString(ClassName.GetPlainNameString()),
								LOCTEXT("Blendable_DirectAsset2h", "Creates an asset that is owned by the containing object"), FSlateIcon(), Direct2);

							bSeparatorIsNeeded = true;
						}
					}
				}
			}
		}

		if(bSeparatorIsNeeded)
		{
			MenuBuilder.AddMenuSeparator();
		}

		FUIAction Indirect(FExecuteAction::CreateSP(this, &FWeightedBlendableCustomization::AddIndirectAsset, Weight));
		MenuBuilder.AddMenuEntry(LOCTEXT("Blendable_IndirectAsset", "Asset reference"), 
			LOCTEXT("Blendable_IndirectAsseth", "reference a Blendable asset (owned by a content package), e.g. material with Post Process domain"), FSlateIcon(), Indirect);
	}
	

	TSharedRef<SWidgetSwitcher> Switcher = SNew(SWidgetSwitcher)
			.WidgetIndex(this, &FWeightedBlendableCustomization::ComputeSwitcherIndex, StructPropertyHandle, Package, Weight, Value);

	Switcher->AddSlot()
		[
			SNew(SComboButton)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Blendable_ChooseElement", "Choose"))
			]
			.ContentPadding(FMargin(6.0, 2.0))
			.MenuContent()
			[
				MenuBuilder.MakeWidget()
			]
		];

	Switcher->AddSlot()
		[
			SNew(SButton)
			.ContentPadding(FMargin(0,0))
			.Text(this, &FWeightedBlendableCustomization::GetDirectAssetName, Value)
			.OnClicked(this, &FWeightedBlendableCustomization::JumpToDirectAsset, Value)
		];

	Switcher->AddSlot()
		[
			SNew(SObjectPropertyEntryBox)
			.PropertyHandle(Value)
		];

	return Switcher;
}


int32 FWeightedBlendableCustomization::ComputeSwitcherIndex(TSharedRef<IPropertyHandle> StructPropertyHandle, UPackage* Package, TSharedPtr<IPropertyHandle> Weight, TSharedPtr<IPropertyHandle> Value) const
{
	float WeightValue = 1.0f;
	UObject* RefObject = 0;
	
	Weight->GetValue(WeightValue);
	Value->GetValue(RefObject);

	if(RefObject)
	{
		UPackage* PropPackage = RefObject->GetOutermost();

		return (PropPackage == Package) ? 1 : 2;
	}
	else
	{
		return (WeightValue < 0.0f) ? 0 : 2;
	}
}

void FWeightedBlendableCustomization::CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	// we don't have children but this is a pure virtual so we need to override
}

void FWeightedBlendableCustomization::CustomizeHeader( TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	TSharedPtr<IPropertyHandle> SharedWeightProp;
	{
		TSharedPtr<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(FName(TEXT("Weight")));
		if (ChildHandle.IsValid() && ChildHandle->GetProperty())
		{
			SharedWeightProp = ChildHandle;
		}
	}
		
	TSharedPtr<IPropertyHandle> SharedValueProp;
	{
		TSharedPtr<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(FName(TEXT("Object")));
		if (ChildHandle.IsValid() && ChildHandle->GetProperty())
		{
			SharedValueProp = ChildHandle;
		}
	}

	float WeightValue = 1.0f;
	UObject* RefObject = 0;
	
	SharedWeightProp->GetValue(WeightValue);
	SharedValueProp->GetValue(RefObject);

	UPackage* StructPackage = 0;
	{
		const TSharedPtr<IPropertyHandle> ParentHandle = StructPropertyHandle->GetParentHandle();
		TArray<UObject*> Objects;
		StructPropertyHandle->GetOuterObjects(Objects);

		for (TArray<UObject*>::TConstIterator It = Objects.CreateConstIterator(); It; It++)
		{
			UObject* ref = *It;

			if(StructPackage)
			{
				// does this mean we have to deal with multiple levels? The code here is not ready for that
				check(StructPackage == ref->GetOutermost());
			}

			StructPackage = ref->GetOutermost();
		}
	}

	HeaderRow.NameContent()
	[
		SNew(SHorizontalBox)
		.Visibility(this, &FWeightedBlendableCustomization::IsWeightVisible, SharedWeightProp)
		+SHorizontalBox::Slot()
		[
			SNew(SBox)
			.MinDesiredWidth(60.0f)
			.MaxDesiredWidth(60.0f)		
			[
				SharedWeightProp->CreatePropertyValueWidget()
			]
		]
	];

	HeaderRow.ValueContent()
	.MaxDesiredWidth(0.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			GenerateContentWidget(StructPropertyHandle, StructPackage, SharedWeightProp, SharedValueProp)
		]
	];
}





#undef LOCTEXT_NAMESPACE
