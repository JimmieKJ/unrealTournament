// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "ChatSettingsViewModel.h"
#include "FriendsAndChatUserSettings.h"

// ToDO: Read and write these values from the respective calling app.

class FChatSettingsViewModelImpl
	: public FChatSettingsViewModel
{
public:

private:



	virtual uint8 GetNumberOfStatesForSetting(EChatSettingsType::Type OptionType) const override
	{
		uint8 StatesNumber = 1;
		switch (OptionType)
		{
		case EChatSettingsType::FontSize: StatesNumber = EChatFontType::InvalidOrMax; break;
		case EChatSettingsType::WindowHeight: StatesNumber = EChatWindowHeight::InvalidOrMax; break;
		};
		return StatesNumber;
	}

	virtual FText GetNameOfStateForSetting(EChatSettingsType::Type OptionType, uint8 Index) const override
	{
		if (OptionType == EChatSettingsType::FontSize && Index < EChatFontType::InvalidOrMax)
		{
			return EChatFontType::ToText(EChatFontType::Type(Index));
		}
		if (OptionType == EChatSettingsType::WindowHeight && Index < EChatWindowHeight::InvalidOrMax)
		{
			return EChatWindowHeight::ToText(EChatWindowHeight::Type(Index));
		}
		return FText::GetEmpty();
	}

	virtual void HandleCheckboxStateChanged(ECheckBoxState NewState, EChatSettingsType::Type OptionType) override
	{
		ChatSettingsService->SetFlagOption(OptionType, NewState == ECheckBoxState::Checked ? true : false);
	}

	virtual ECheckBoxState GetOptionCheckState(EChatSettingsType::Type Option) const override
	{
		return ChatSettingsService->GetFlagOption(Option) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	virtual void HandleRadioboxStateChanged(ECheckBoxState NewState, EChatSettingsType::Type OptionType, uint8 Index)  override
	{
		ChatSettingsService->SetRadioOption(OptionType, Index);
	}
	
	virtual ECheckBoxState GetRadioOptionCheckState(EChatSettingsType::Type Option, uint8 Index) const override
	{
		return ChatSettingsService->GetRadioOption(Option, Index) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	virtual void HandleSliderValueChanged(float Value, EChatSettingsType::Type OptionType) override
	{
		ChatSettingsService->SetSliderValue(OptionType, Value);
	}

	virtual float GetSliderValue(EChatSettingsType::Type OptionType) const override
	{
		return ChatSettingsService->GetSliderValue(OptionType);
	}

private:

	FChatSettingsViewModelImpl(const TSharedRef<IChatSettingsService>& InChatSettingsService)
		:ChatSettingsService(InChatSettingsService)
	{
	}
	
	TSharedRef<IChatSettingsService> ChatSettingsService;

	friend FChatSettingsViewModelFactory;
};

TSharedRef< FChatSettingsViewModel > FChatSettingsViewModelFactory::Create(const TSharedRef<IChatSettingsService>& ChatSettingsService)
{
	TSharedRef< FChatSettingsViewModelImpl > ViewModel(new FChatSettingsViewModelImpl(ChatSettingsService));
	return ViewModel;
}