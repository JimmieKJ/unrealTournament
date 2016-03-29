// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "ImportantToggleSettingCustomization.h"
#include "Engine/ImportantToggleSettingInterface.h"
#include "SHyperlink.h"

#define LOCTEXT_NAMESPACE "ImportantToggleSettingCustomization"

class SImportantToggleButton : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SImportantToggleButton)
		: _Text()
	{}

		SLATE_STYLE_ARGUMENT(FCheckBoxStyle, CheckBoxStyle)
		SLATE_ARGUMENT(FText, Text)
		SLATE_ARGUMENT(FText, ToolTipText)
		SLATE_ATTRIBUTE(bool, IsSet)
		SLATE_EVENT(FSimpleDelegate, OnToggled)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		OnToggled = InArgs._OnToggled;
		IsSetAttribute = InArgs._IsSet;

		FSlateFontInfo LargeDetailsFont = IDetailLayoutBuilder::GetDetailFontBold();
		LargeDetailsFont.Size += 4;

		ChildSlot
		[
			SNew(SCheckBox)
			.Style(InArgs._CheckBoxStyle)
			.IsChecked(this, &SImportantToggleButton::GetCheckedState)
			.OnCheckStateChanged(this, &SImportantToggleButton::OnClick)
			.ToolTipText(InArgs._ToolTipText)
			.Padding(FMargin(16.0f, 12.0f))
			.ForegroundColor(FSlateColor::UseForeground())
			.IsFocusable(true)
			[
				SNew(STextBlock)
				.Text(InArgs._Text)
				.Font(LargeDetailsFont)
			]
		];
	}

private:
	void OnClick(ECheckBoxState State)
	{
		OnToggled.ExecuteIfBound();
	}

	ECheckBoxState GetCheckedState() const
	{
		return IsSetAttribute.Get() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

private:
	TAttribute<bool> IsSetAttribute;
	FSimpleDelegate OnToggled;
};

//====================================================================================
// FImportantToggleSettingCustomization
//====================================================================================

TSharedRef<IDetailCustomization> FImportantToggleSettingCustomization::MakeInstance()
{
	return MakeShareable(new FImportantToggleSettingCustomization);
}

void FImportantToggleSettingCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	if (Objects.Num() == 1)
	{
		ToggleSettingObject = Objects[0];
		IImportantToggleSettingInterface* ToogleSettingInterface = Cast<IImportantToggleSettingInterface>(ToggleSettingObject.Get());

		if (ToogleSettingInterface != nullptr)
		{
			FName CategoryName;
			FName PropertyName;
			ToogleSettingInterface->GetToogleCategoryAndPropertyNames(CategoryName, PropertyName);
			IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(CategoryName);
			TogglePropertyHandle = DetailBuilder.GetProperty(PropertyName);

			FSlateFontInfo StateDescriptionFont = IDetailLayoutBuilder::GetDetailFont();
			StateDescriptionFont.Size += 4;

			// Customize collision section
			Category.InitiallyCollapsed(false)
			.AddProperty(TogglePropertyHandle)
			.ShouldAutoExpand(true)
			.CustomWidget()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 12.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SImportantToggleButton)
						.CheckBoxStyle(FEditorStyle::Get(), "Property.ToggleButton.Start")
						.Text(ToogleSettingInterface->GetFalseStateLabel())
						.ToolTipText(ToogleSettingInterface->GetFalseStateTooltip())
						.IsSet(this, &FImportantToggleSettingCustomization::IsToggleValue, false)
						.OnToggled(this, &FImportantToggleSettingCustomization::OnToggledTo, false)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SImportantToggleButton)
						.CheckBoxStyle(FEditorStyle::Get(), "Property.ToggleButton.End")
						.Text(ToogleSettingInterface->GetTrueStateLabel())
						.ToolTipText(ToogleSettingInterface->GetTrueStateTooltip())
						.IsSet(this, &FImportantToggleSettingCustomization::IsToggleValue, true)
						.OnToggled(this, &FImportantToggleSettingCustomization::OnToggledTo, true)
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.Padding(0.0f, 12.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.VAlign(VAlign_Center)
						[
							SNew(SHyperlink)
							.Text(ToogleSettingInterface->GetAdditionalInfoUrlLabel())
							.OnNavigate(this, &FImportantToggleSettingCustomization::OnNavigateHyperlink, ToogleSettingInterface->GetAdditionalInfoUrl())
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 12.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text(this, &FImportantToggleSettingCustomization::GetDescriptionText)
					.Font(StateDescriptionFont)
				]
			];			
		}
	}


}

bool FImportantToggleSettingCustomization::IsToggleValue(bool bValue) const
{
	bool bPropertyValue = false;
	TogglePropertyHandle->GetValue(bPropertyValue);
	return bPropertyValue == bValue;
}

void FImportantToggleSettingCustomization::OnToggledTo(bool bSetTo)
{
	TogglePropertyHandle->SetValue(bSetTo);
}

void FImportantToggleSettingCustomization::OnNavigateHyperlink(FString Url)
{
	FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
}

FText FImportantToggleSettingCustomization::GetDescriptionText() const
{
	IImportantToggleSettingInterface* ToogleSettingInterface = Cast<IImportantToggleSettingInterface>(ToggleSettingObject.Get());

	if (ToogleSettingInterface != nullptr)
	{
		bool bPropertyValue = false;
		TogglePropertyHandle->GetValue(bPropertyValue);

		if (bPropertyValue)
		{
			return ToogleSettingInterface->GetTrueStateDescription();
		}
		else
		{
			return ToogleSettingInterface->GetFalseStateDescription();
		}
	}
	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
