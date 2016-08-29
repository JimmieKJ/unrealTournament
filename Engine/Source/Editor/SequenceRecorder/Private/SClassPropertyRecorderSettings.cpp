// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequenceRecorderPrivatePCH.h"
#include "SClassPropertyRecorderSettings.h"
#include "EditorStyle.h"
#include "SequenceRecorderSettings.h"
#include "MovieSceneMultiPropertyRecorder.h"

#define LOCTEXT_NAMESPACE "SClassPropertyRecorderSettings"

void SClassPropertyRecorderSettings::Construct(const FArguments& Args, const TSharedRef<class IPropertyHandle>& InClassHandle, const TSharedRef<class IPropertyHandle>& InPropertiesHandle, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PropertiesHandle = InPropertiesHandle;
	ClassHandle = InClassHandle;

	ChildSlot
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(this, &SClassPropertyRecorderSettings::GetText)
			.Font(CustomizationUtils.GetRegularFont())
			.AutoWrapText(true)
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
			.ToolTipText(LOCTEXT("ChoosePropertiesButtonToolTip", "Choose properties to be recorded for this class"))
			.OnClicked(this, &SClassPropertyRecorderSettings::HandleChoosePropertiesButtonClicked)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("PropertyWindow.Button_Ellipsis"))
			]
		]
	];
}

FText SClassPropertyRecorderSettings::GetText() const
{
	if (PropertiesHandle.IsValid())
	{
		TArray<FName>& PropertiesToRecord = GetPropertyNameArray();
		if (PropertiesToRecord.Num() > 0)
		{
			FText Text = FText::Format(LOCTEXT("SinglePropertyToRecordFormat", "{0}"), FText::FromName(PropertiesToRecord[0]));
			for (int32 PropertyIndex = 1; PropertyIndex < PropertiesToRecord.Num(); ++PropertyIndex)
			{
				Text = FText::Format(LOCTEXT("PropertiesToRecordFormat", "{0}, {1}"), Text, FText::FromName(PropertiesToRecord[PropertyIndex]));
			}
			return Text;
		}
	}

	return LOCTEXT("NoPropertiesToRecord", "None");
}

FReply SClassPropertyRecorderSettings::HandleChoosePropertiesButtonClicked()
{
	if (ClassHandle.IsValid() && PropertiesHandle.IsValid())
	{
		FDetailsViewArgs DetailsViewArgs(false, false, true, FDetailsViewArgs::HideNameArea, true);
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

		DetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SClassPropertyRecorderSettings::ShouldShowProperty));
		DetailsView->SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly::CreateSP(this, &SClassPropertyRecorderSettings::IsPropertyReadOnly));
		DetailsView->SetExtensionHandler(SharedThis(this));
		DetailsView->SetDisableCustomDetailLayouts(true);

		TArray<void*> RawData;
		ClassHandle->AccessRawData(RawData);
		check(RawData.Num() > 0);
		TSubclassOf<UObject>& Class = *reinterpret_cast<TSubclassOf<UObject>*>(RawData[0]);
		DetailsView->SetObject(Class.GetDefaultObject());

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(LOCTEXT("PropertyPickerWindowTitle", "Choose Properties to Be Recorded"))
			.ClientSize(FVector2D(400, 550));

		Window->SetContent(
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush(TEXT("PropertyWindow.WindowBorder")))
			[
				DetailsView
			]
		);

		// Parent the window to the root window
		FSlateApplication::Get().AddModalWindow(Window, FGlobalTabmanager::Get()->GetRootWindow().ToSharedRef());
	}
	return FReply::Handled();
}

bool ShouldShowProperty_Recursive(const UProperty* InProperty)
{
	if (InProperty->IsA<UStructProperty>())
	{
		const UStructProperty* StructProperty = CastChecked<const UStructProperty>(InProperty);
		for (const UProperty* Property = StructProperty->Struct->PropertyLink; Property; Property = Property->PropertyLinkNext)
		{
			if (Property->HasAnyPropertyFlags(CPF_Interp) && FMovieSceneMultiPropertyRecorder::CanPropertyBeRecorded(*Property))
			{
				return true;
			}
			else if(ShouldShowProperty_Recursive(Property))
			{
				return true;
			}
		}
	}

	return false;
}

bool SClassPropertyRecorderSettings::ShouldShowProperty(const FPropertyAndParent& InPropertyAndParent) const
{
	bool bShow = InPropertyAndParent.Property.HasAnyPropertyFlags(CPF_Interp) && FMovieSceneMultiPropertyRecorder::CanPropertyBeRecorded(InPropertyAndParent.Property);

	// we also need to recurse into sub-properties to see if we should show them too
	return bShow || ShouldShowProperty_Recursive(&InPropertyAndParent.Property);
}

bool SClassPropertyRecorderSettings::IsPropertyReadOnly(const FPropertyAndParent& InPropertyAndParent) const
{
	return true;
}

bool SClassPropertyRecorderSettings::IsPropertyExtendable(const UClass* InObjectClass, const class IPropertyHandle& PropertyHandle) const
{
	return PropertyHandle.GetProperty()->HasAnyPropertyFlags(CPF_Interp) && FMovieSceneMultiPropertyRecorder::CanPropertyBeRecorded(*PropertyHandle.GetProperty());
}

TSharedRef<SWidget> SClassPropertyRecorderSettings::GenerateExtensionWidget(const UClass* InObjectClass, TSharedPtr<IPropertyHandle> PropertyHandle)
{
	ECheckBoxState InitialState = ECheckBoxState::Unchecked;
	FName PropertyName = *PropertyHandle->GeneratePathToProperty();

	TArray<FName>& PropertiesToRecord = GetPropertyNameArray();
	InitialState = PropertiesToRecord.Contains(PropertyName) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;

	return SNew(SCheckBox)
		.OnCheckStateChanged(this, &SClassPropertyRecorderSettings::HandlePropertyCheckStateChanged, PropertyHandle)
		.IsChecked(InitialState);
}

void SClassPropertyRecorderSettings::HandlePropertyCheckStateChanged(ECheckBoxState InState, TSharedPtr<IPropertyHandle> PropertyHandle)
{
	if (PropertyHandle.IsValid() && PropertiesHandle.IsValid())
	{
		TArray<FName>& PropertiesToRecord = GetPropertyNameArray();

		FName PropertyPath = *PropertyHandle->GeneratePathToProperty();
		if (InState == ECheckBoxState::Checked)
		{
			PropertiesToRecord.AddUnique(PropertyPath);
		}
		else
		{
			PropertiesToRecord.Remove(PropertyPath);
		}

		PropertiesHandle->NotifyPostChange();
	}
}

TArray<FName>& SClassPropertyRecorderSettings::GetPropertyNameArray() const
{
	check(PropertiesHandle.IsValid());

	TArray<void*> RawData;
	PropertiesHandle->AccessRawData(RawData);
	check(RawData.Num() > 0);
	return *reinterpret_cast<TArray<FName>*>(RawData[0]);
}

#undef LOCTEXT_NAMESPACE