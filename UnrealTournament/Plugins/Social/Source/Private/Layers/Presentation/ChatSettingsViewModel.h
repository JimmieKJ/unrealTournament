// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FChatSettingsViewModel
	: public TSharedFromThis<FChatSettingsViewModel>
{
public:
	virtual ~FChatSettingsViewModel() {}

	// Settings
	virtual uint8 GetNumberOfStatesForSetting(EChatSettingsType::Type OptionType) const = 0;
	virtual FText GetNameOfStateForSetting(EChatSettingsType::Type OptionType, uint8 Index) const = 0;

	// Check Box
	virtual void HandleCheckboxStateChanged(ECheckBoxState NewState, EChatSettingsType::Type OptionType) = 0;
	virtual ECheckBoxState GetOptionCheckState(EChatSettingsType::Type Option) const = 0;

	// Radio Box
	virtual void HandleRadioboxStateChanged(ECheckBoxState NewState, EChatSettingsType::Type OptionType, uint8 Index) = 0;
	virtual ECheckBoxState GetRadioOptionCheckState(EChatSettingsType::Type Option, uint8 Index) const = 0;

	// Slider
	virtual void HandleSliderValueChanged(float Value, EChatSettingsType::Type OptionType) = 0;
	virtual float GetSliderValue(EChatSettingsType::Type OptionType) const = 0;

};

/**
 * Creates the implementation for an FFriendsUserSettingsViewModel.
 *
 * @return the newly created FFriendsUserSettingsViewModel implementation.
 */
FACTORY(TSharedRef< FChatSettingsViewModel >, FChatSettingsViewModel, const TSharedRef<IChatSettingsService>& ChatSettingsService);