// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceEditorPCH.h"
#include "IDetailPropertyRow.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "LevelSequencePlayer.h"
#include "PropertyHandle.h"


#define LOCTEXT_NAMESPACE "LevelSequencePlaybackSettingsCustomization"


TSharedRef<IPropertyTypeCustomization> FLevelSequencePlaybackSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FLevelSequencePlaybackSettingsCustomization);
}


void FLevelSequencePlaybackSettingsCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// do nothing
}


void FLevelSequencePlaybackSettingsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	StructPropertyHandle = InPropertyHandle;
	ChildBuilder.AddChildProperty(StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLevelSequencePlaybackSettings, PlayRate)).ToSharedRef());
	LoopCountProperty = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLevelSequencePlaybackSettings, LoopCount));

	// Set up the initial environment
	{
		LoopModes.Add(MakeShareable( new FLoopMode{ LOCTEXT("DontLoop", "Don't Loop"), 0 } ));
		LoopModes.Add(MakeShareable( new FLoopMode{ LOCTEXT("Indefinitely", "Loop Indefinitely"), -1 }) );
		LoopModes.Add(MakeShareable( new FLoopMode{ LOCTEXT("Exactly", "Loop Exactly..."), 1 }) );

		int32 CurrentValue = -1;
		LoopCountProperty->GetValue(CurrentValue);

		if (auto* FoundMode = LoopModes.FindByPredicate([&](const TSharedPtr<FLoopMode>& In){ return In->Value == CurrentValue; }) )
		{
			CurrentMode = *FoundMode;
		}
		else
		{
			CurrentMode = LoopModes.Last();
		}
	}

	ChildBuilder.AddChildContent(LOCTEXT("LoopTitle", "Loop"))
		.NameContent()
		[
			LoopCountProperty->CreatePropertyNameWidget()
		]
		.ValueContent()
		.HAlign(HAlign_Fill)
		.MaxDesiredWidth(200)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SComboBox<TSharedPtr<FLoopMode>>)
						.OptionsSource(&LoopModes)
						.OnSelectionChanged_Lambda([&](TSharedPtr<FLoopMode> Mode, ESelectInfo::Type){
							CurrentMode = Mode;
							UpdateProperty();
						})
						.OnGenerateWidget_Lambda([&](TSharedPtr<FLoopMode> InMode){
							return SNew(STextBlock)
								.Font(IDetailLayoutBuilder::GetDetailFont())
								.Text(InMode->DisplayName);
						})
						.InitiallySelectedItem(CurrentMode)
						[
							SAssignNew(CurrentText, STextBlock)
								.Font(IDetailLayoutBuilder::GetDetailFont())
								.Text(CurrentMode->DisplayName)
						]
				]

			+ SHorizontalBox::Slot()
				.Padding(FMargin(4.f,0))
				.VAlign(VAlign_Center)
				[
					SAssignNew(LoopEntry, SHorizontalBox)
						.Visibility(CurrentMode == LoopModes.Last() ? EVisibility::Visible : EVisibility::Collapsed)

					+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.Padding(FMargin(0, 0, 4.f, 0))
						[
							LoopCountProperty->CreatePropertyValueWidget()
						]

					+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(STextBlock)
								.Font(IDetailLayoutBuilder::GetDetailFont())
								.Text_Raw(this, &FLevelSequencePlaybackSettingsCustomization::GetCustomSuffix)
						]
				]
		];
}


FText FLevelSequencePlaybackSettingsCustomization::GetCustomSuffix() const
{
	int32 NumLoops = 0;
	LoopCountProperty->GetValue(NumLoops);

	return NumLoops == 1
		? LOCTEXT("Time", "time")
		: LOCTEXT("Times", "times");
}


void FLevelSequencePlaybackSettingsCustomization::UpdateProperty()
{
	if (CurrentMode == LoopModes.Last())
	{
		// Show custom loop entry
		LoopEntry->SetVisibility(EVisibility::Visible);
	}
	else
	{
		// Hide custom loop entry
		LoopEntry->SetVisibility(EVisibility::Collapsed);
	}

	LoopCountProperty->SetValue(CurrentMode->Value);
	CurrentText->SetText(CurrentMode->DisplayName);
}


#undef LOCTEXT_NAMESPACE
