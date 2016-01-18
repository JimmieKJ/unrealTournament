// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "SChatChromeSettings.h"
#include "ChatChromeTabViewModel.h"
#include "ChatSettingsViewModel.h"
#include "FriendsFontStyleService.h"

#define LOCTEXT_NAMESPACE "SChatChromeSettingsImpl"

class SChatChromeSettingsImpl : public SChatChromeSettings
{
public:

	void Construct(const FArguments& InArgs, const TSharedPtr<class FChatSettingsViewModel> InChatSettingsViewModel, const TSharedRef<class FFriendsFontStyleService>& InFontService) override
	{
		FontService = InFontService;
		Settings = InArgs._Settings;
		FriendStyle = *InArgs._FriendStyle;
		Opacity = InArgs._Opacity;
		ChatSettingsViewModel = InChatSettingsViewModel;

		SAssignNew(OptionsContainer, SVerticalBox);

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			OptionsContainer.ToSharedRef()
		]);
		GenerateOptions();
	}

private:

	void GenerateOptions()
	{
		OptionsContainer->ClearChildren();

		for (EChatSettingsType::Type SettingType : Settings)
		{
			EChatSettingsOptionType::Type OptionType = EChatSettingsOptionType::GetOptionType(SettingType);
			if (OptionType == EChatSettingsOptionType::CheckBox)
			{
				OptionsContainer->AddSlot()
					.AutoHeight()
					[
						GetCheckBoxOption(SettingType).ToSharedRef()
					];
			}
			else if (OptionType == EChatSettingsOptionType::RadioBox)
			{
				OptionsContainer->AddSlot()
					.AutoHeight()
					[
						GetRadioBoxOption(SettingType).ToSharedRef()
					];
			}
			else if (OptionType == EChatSettingsOptionType::Slider)
			{
				OptionsContainer->AddSlot()
					.AutoHeight()
					[
						GetSliderOption(SettingType).ToSharedRef()
					];
			}
		}
	}

	TSharedPtr<SWidget> GetCheckBoxOption(EChatSettingsType::Type SettingType)
	{
		TSharedPtr<STextBlock> TextBlock = SNew(STextBlock)
			.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetNormalFont)
			.Text(EChatSettingsType::ToText(SettingType));

		TSharedPtr<SImage> CheckImage = SNew(SImage)
			.Image(this, &SChatChromeSettingsImpl::GetCheckOptionBrush, SettingType);

		TSharedPtr<SButton> Button;

		SAssignNew(Button, SButton)
			.ButtonStyle(&FriendStyle.FriendsListStyle.HeaderButtonStyle)
			.ButtonColorAndOpacity(this, &SChatChromeSettingsImpl::GetOpacityFadeColor)
			.ContentPadding(FriendStyle.FriendsListStyle.SubMenuSettingButtonMargin)
			.OnClicked(this, &SChatChromeSettingsImpl::HandleCheckButtonClicked, SettingType)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					TextBlock.ToSharedRef()
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					CheckImage.ToSharedRef()
				]
			];

		CheckImage->SetColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SChatChromeSettingsImpl::GetButtonContentColor, Button)));
		TextBlock->SetColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SChatChromeSettingsImpl::GetButtonContentColor, Button)));
		return Button;
	}

	TSharedPtr<SWidget> GetRadioBoxOption(EChatSettingsType::Type SettingType)
	{
		TSharedPtr<SVerticalBox> RadioBox = SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FriendStyle.FriendsListStyle.SubMenuListMargin)
			[
				GetRadioSelectionTitle(SettingType).ToSharedRef()
			];

		uint8 MaxStates = ChatSettingsViewModel->GetNumberOfStatesForSetting(SettingType);
		for (uint8 RadioIndex = 0; RadioIndex < MaxStates; ++RadioIndex)
		{
			TSharedPtr<STextBlock> TextBlock = SNew(STextBlock)
				.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetNormalFont)
				.Text(ChatSettingsViewModel->GetNameOfStateForSetting(SettingType, RadioIndex));

			TSharedPtr<SImage> RadioImage = SNew(SImage)
				.Image(this, &SChatChromeSettingsImpl::GetRadioOptionBrush, SettingType, RadioIndex);

			TSharedPtr<SButton> Button;

			RadioBox->AddSlot()
			[
				SAssignNew(Button, SButton)
				.ButtonStyle(&FriendStyle.FriendsListStyle.HeaderButtonStyle)
				.ButtonColorAndOpacity(this, &SChatChromeSettingsImpl::GetOpacityFadeColor)
				.ContentPadding(FriendStyle.FriendsListStyle.SubMenuSettingButtonMargin)
				.OnClicked(this, &SChatChromeSettingsImpl::HandleRadioButtonClicked, SettingType, RadioIndex)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						TextBlock.ToSharedRef()
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					[
						RadioImage.ToSharedRef()
					]
				]
			];

			RadioImage->SetColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SChatChromeSettingsImpl::GetButtonContentColor, Button)));
			TextBlock->SetColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SChatChromeSettingsImpl::GetButtonContentColor, Button)));
		}
		return RadioBox;
	}

	TSharedPtr<SWidget> GetSliderOption(EChatSettingsType::Type SettingType)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(FriendStyle.FriendsLargeFontStyle.FriendsFontNormal)
				.Text(EChatSettingsType::ToText(SettingType))
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSlider)
				.SliderHandleColor(this, &SChatChromeSettingsImpl::GetOpacityFadeColor)
				.SliderBarColor(this, &SChatChromeSettingsImpl::GetOpacityFadeColor)
				.Value(ChatSettingsViewModel.Get(), &FChatSettingsViewModel::GetSliderValue, SettingType)
				.OnValueChanged(ChatSettingsViewModel.Get(), &FChatSettingsViewModel::HandleSliderValueChanged, SettingType)
			];
	}

	FSlateColor GetOpacityFadeColor() const
	{
		return FLinearColor(1, 1, 1, Opacity.Get());
	}

	FSlateColor GetOpacityFadeColorDull() const
	{
		FLinearColor ButtonColor = FriendStyle.FriendsListStyle.ButtonContentColor.GetSpecifiedColor();
		ButtonColor.A = Opacity.Get();
		return ButtonColor;
	}

	FSlateColor GetButtonContentColor(TSharedPtr<SButton> Button) const
	{
		FLinearColor ButtonColor = FriendStyle.FriendsListStyle.ButtonContentColor.GetSpecifiedColor();
		if (Button.IsValid() && Button->IsHovered())
		{
			ButtonColor = FriendStyle.FriendsListStyle.ButtonHoverContentColor.GetSpecifiedColor();
		}
		ButtonColor.A = Opacity.Get();
		return ButtonColor;
	}

	TSharedPtr<SWidget> GetRadioSelectionTitle(EChatSettingsType::Type OptionType)
	{
		if (OptionType == EChatSettingsType::FontSize)
		{
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SSeparator)
					.SeparatorImage(&FriendStyle.FriendsListStyle.SeperatorBrush)
					.Thickness(FriendStyle.FriendsListStyle.SubMenuSeperatorThickness)
					.ColorAndOpacity(this, &SChatChromeSettingsImpl::GetOpacityFadeColor)
				]
				+ SVerticalBox::Slot()
				.Padding(FriendStyle.FriendsListStyle.RadioSettingTitleMargin)
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(FriendStyle.FriendsListStyle.SubMenuPageIconMargin)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SImage)
						.Image(&FriendStyle.FriendsListStyle.FontSizeBrush)
						.ColorAndOpacity(this, &SChatChromeSettingsImpl::GetOpacityFadeColor)
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("FontOption","FONT SIZE"))
						.ColorAndOpacity(this, &SChatChromeSettingsImpl::GetOpacityFadeColorDull)
						.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetNormalFont)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SSeparator)
					.SeparatorImage(&FriendStyle.FriendsListStyle.SeperatorBrush)
					.Thickness(FriendStyle.FriendsListStyle.SubMenuSeperatorThickness)
					.ColorAndOpacity(this, &SChatChromeSettingsImpl::GetOpacityFadeColor)
				];
		}

		return
			SNew(STextBlock)
			.Text(EChatSettingsType::ToText(OptionType))
			.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetNormalFont);
	}

	const FSlateBrush* GetRadioOptionBrush(EChatSettingsType::Type OptionType, uint8 Index) const
	{
		if (ChatSettingsViewModel.IsValid())
		{
			if (ChatSettingsViewModel->GetRadioOptionCheckState(OptionType, Index) == ECheckBoxState::Checked)
			{
				return &FriendStyle.RadioBoxStyle.CheckedImage;
			}
		}
		return &FriendStyle.RadioBoxStyle.UncheckedImage;
	}

	const FSlateBrush* GetCheckOptionBrush(EChatSettingsType::Type OptionType) const
	{
		if (ChatSettingsViewModel.IsValid())
		{
			if (ChatSettingsViewModel->GetOptionCheckState(OptionType) == ECheckBoxState::Checked)
			{
				return &FriendStyle.CheckBoxStyle.CheckedImage;
			}
		}
		return &FriendStyle.CheckBoxStyle.UncheckedImage;
	}

	FReply HandleRadioButtonClicked(EChatSettingsType::Type OptionType, uint8 Index)
	{
		if (ChatSettingsViewModel.IsValid())
		{
			ECheckBoxState NewState = ChatSettingsViewModel->GetRadioOptionCheckState(OptionType, Index) == ECheckBoxState::Checked ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
			ChatSettingsViewModel->HandleRadioboxStateChanged(NewState, OptionType, Index);
		}
		return FReply::Handled();
	}

	FReply HandleCheckButtonClicked(EChatSettingsType::Type OptionType)
	{
		if (ChatSettingsViewModel.IsValid())
		{
			ECheckBoxState NewState = ChatSettingsViewModel->GetOptionCheckState(OptionType) == ECheckBoxState::Checked ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
			ChatSettingsViewModel->HandleCheckboxStateChanged(NewState, OptionType);
		}
		return FReply::Handled();
	}


	TSharedPtr<FFriendsFontStyleService> FontService;
	TArray<EChatSettingsType::Type> Settings;
	TSharedPtr<SVerticalBox> OptionsContainer;
	FFriendsAndChatStyle FriendStyle;
	TSharedPtr<FChatSettingsViewModel> ChatSettingsViewModel;
	TAttribute<float> Opacity;
};

TSharedRef<SChatChromeSettings> SChatChromeSettings::New()
{
	return MakeShareable(new SChatChromeSettingsImpl());
}

#undef LOCTEXT_NAMESPACE
