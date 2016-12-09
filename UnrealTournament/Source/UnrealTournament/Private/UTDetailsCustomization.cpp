// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "SlateBasics.h"
#include "UTDetailsCustomization.h"
#include "UTWeaponAttachment.h"
#include "Particles/ParticleSystemComponent.h"
#if WITH_EDITOR
#include "IDetailCustomNodeBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailPropertyRow.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "IDetailsView.h"
#include "IPropertyUtilities.h"
#include "PropertyCustomizationHelpers.h"

struct FMuzzleFlashChoice
{
	TWeakObjectPtr<UParticleSystemComponent> PSC;
	FName DisplayName;
	FMuzzleFlashChoice()
		: PSC(NULL), DisplayName(NAME_None)
	{}
	FMuzzleFlashChoice(const TWeakObjectPtr<UParticleSystemComponent>& InPSC, const FName& InName)
		: PSC(InPSC), DisplayName(InName)
	{}
	bool operator==(const FMuzzleFlashChoice& Other)
	{
		return PSC == Other.PSC;
	}
	bool operator!=(const FMuzzleFlashChoice& Other)
	{
		return PSC != Other.PSC;
	}
};

struct FMuzzleFlashItem : public TSharedFromThis<FMuzzleFlashItem>
{
	/** index in muzzle flash array */
	uint32 Index;
	/** object being modified */
	TWeakObjectPtr<UObject> Obj;
	/** choices */
	const TArray<TSharedPtr<FMuzzleFlashChoice>>& Choices;

	TSharedPtr<STextBlock> TextBlock;

	FMuzzleFlashItem(uint32 InIndex, TWeakObjectPtr<UObject> InObj, const TArray<TSharedPtr<FMuzzleFlashChoice>>& InChoices)
		: Index(InIndex), Obj(InObj), Choices(InChoices)
	{
	}

	TSharedRef<SComboBox<TSharedPtr<FMuzzleFlashChoice>>> CreateWidget()
	{
		FString CurrentText;
		{
			UParticleSystemComponent* CurrentValue = NULL;
			AUTWeapon* Weap = Cast<AUTWeapon>(Obj.Get());
			if (Weap != NULL)
			{
				CurrentValue = Weap->MuzzleFlash[Index];
			}
			else
			{
				AUTWeaponAttachment* Attachment = Cast<AUTWeaponAttachment>(Obj.Get());
				if (Attachment != NULL)
				{
					CurrentValue = Attachment->MuzzleFlash[Index];
				}
			}
			for (int32 i = 0; i < Choices.Num(); i++)
			{
				if (Choices[i]->PSC == CurrentValue)
				{
					CurrentText = Choices[i]->DisplayName.ToString();
				}
			}
		}

		return SNew(SComboBox<TSharedPtr<FMuzzleFlashChoice>>)
		.OptionsSource(&Choices)
		.OnGenerateWidget(this, &FMuzzleFlashItem::GenerateWidget)
		.OnSelectionChanged(this, &FMuzzleFlashItem::ComboChanged)
		.Content()
		[
			SAssignNew(TextBlock, STextBlock)
			.Text(FText::FromString(CurrentText))
		];
	}

	void ComboChanged(TSharedPtr<FMuzzleFlashChoice> NewSelection, ESelectInfo::Type SelectInfo)
	{
		if (Obj.IsValid())
		{
			UParticleSystemComponent* NewValue = NewSelection->PSC.Get();
			AUTWeapon* Weap = Cast<AUTWeapon>(Obj.Get());
			if (Weap != NULL)
			{
				if (Weap->MuzzleFlash.IsValidIndex(Index))
				{
					Weap->MuzzleFlash[Index] = NewValue;
					TextBlock->SetText(NewSelection->DisplayName.ToString());
				}
			}
			else
			{
				AUTWeaponAttachment* Attachment = Cast<AUTWeaponAttachment>(Obj.Get());
				if (Attachment != NULL && Attachment->MuzzleFlash.IsValidIndex(Index))
				{
					Attachment->MuzzleFlash[Index] = NewValue;
					TextBlock->SetText(NewSelection->DisplayName.ToString());
				}
			}
		}
	}

	TSharedRef<SWidget> GenerateWidget(TSharedPtr<FMuzzleFlashChoice> InItem)
	{
		return SNew(SBox)
			.Padding(5)
			[
				SNew(STextBlock)
				.Text(FText::FromName(InItem->DisplayName))
			];
	}
};

class UNREALTOURNAMENT_API FMFArrayBuilder : public FDetailArrayBuilder
{
public:
	FMFArrayBuilder(TWeakObjectPtr<UObject> InObj, TSharedRef<IPropertyHandle> InBaseProperty, TArray<TSharedPtr<FMuzzleFlashChoice>>& InChoices, bool InGenerateHeader = true)
		: FDetailArrayBuilder(InBaseProperty, InGenerateHeader), Obj(InObj), Choices(InChoices), MyArrayProperty(InBaseProperty->AsArray())
	{}

	TWeakObjectPtr<UObject> Obj;
	TArray<TSharedPtr<FMuzzleFlashChoice>> Choices;
	TArray<TSharedPtr<FMuzzleFlashItem>> MFEntries;
	TSharedPtr<IPropertyHandleArray> MyArrayProperty;

	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override
	{
		uint32 NumChildren = 0;
		MyArrayProperty->GetNumElements(NumChildren);

		MFEntries.SetNum(NumChildren);
		for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
		{
			TSharedPtr<FMuzzleFlashItem> NewEntry = MakeShareable(new FMuzzleFlashItem(ChildIndex, Obj, Choices));
			MFEntries[ChildIndex] = NewEntry;
			
			TSharedRef<IPropertyHandle> ElementHandle = MyArrayProperty->GetElement(ChildIndex);
			ChildrenBuilder.AddChildContent(FText::FromString(TEXT("MuzzleFlash")))
			.NameContent()
			[
				ElementHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			[
				NewEntry->CreateWidget()
			];
		}
	}
};

void FUTDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailLayout.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() == 1 && Objects[0].IsValid())
	{
		IDetailCategoryBuilder& WeaponCategory = DetailLayout.EditCategory("Weapon");

		TSharedRef<IPropertyHandle> MuzzleFlash = DetailLayout.GetProperty(TEXT("MuzzleFlash"));

		uint32 NumChildren = 0;
		MuzzleFlash->GetNumChildren(NumChildren);

		TArray<TSharedPtr<FMuzzleFlashChoice>> Choices;
		Choices.Add(MakeShareable(new FMuzzleFlashChoice(NULL, NAME_None)));
		{
			// the components editor uses names from the SCS instead so that's what we need to use
			TArray<USCS_Node*> ConstructionNodes;
			{
				TArray<const UBlueprintGeneratedClass*> ParentBPClassStack;
				UBlueprintGeneratedClass::GetGeneratedClassesHierarchy(Objects[0].Get()->GetClass(), ParentBPClassStack);
				for (int32 i = ParentBPClassStack.Num() - 1; i >= 0; i--)
				{
					const UBlueprintGeneratedClass* CurrentBPGClass = ParentBPClassStack[i];
					if (CurrentBPGClass->SimpleConstructionScript)
					{
						ConstructionNodes += CurrentBPGClass->SimpleConstructionScript->GetAllNodes();
					}
				}
			}
			TArray<UObject*> Children;
			for (UClass* TestClass = Objects[0].Get()->GetClass(); TestClass != NULL; TestClass = TestClass->GetSuperClass())
			{
				GetObjectsWithOuter(TestClass, Children, true, RF_NoFlags, EInternalObjectFlags::PendingKill);
			}
			for (int32 i = 0; i < Children.Num(); i++)
			{
				UParticleSystemComponent* PSC = Cast<UParticleSystemComponent>(Children[i]);
				if (PSC != NULL)
				{
					FName DisplayName = PSC->GetFName();
					for (int32 j = 0; j < ConstructionNodes.Num(); j++)
					{
						if (ConstructionNodes[j]->ComponentTemplate == PSC)
						{
							DisplayName = ConstructionNodes[j]->GetVariableName();
							break;
						}
					}
					Choices.Add(MakeShareable(new FMuzzleFlashChoice(PSC, DisplayName)));
				}
			}
		}

		WeaponCategory.AddCustomBuilder(MakeShareable(new FMFArrayBuilder(Objects[0], MuzzleFlash, Choices, true)), false);
	}
}
#endif