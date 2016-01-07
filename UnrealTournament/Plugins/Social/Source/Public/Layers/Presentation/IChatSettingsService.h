// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
IChatDisplayService.h: Interface for Chat settings service.
The chat settings service is used to control the settings for the chat windows and tabs.
=============================================================================*/

#pragma once
#include "FriendsAndChatUserSettings.h"

class IChatSettingsService
{
public:

	/*
	 * Load the config
	 */
	virtual void LoadConfig() = 0;

	/*
	 * Save the config
	 */
	virtual void SaveConfig() = 0;

	/*
	 * Get an option that is stored as a bool
	 * @param Option The Option to get the value for
	 */
	virtual bool GetFlagOption(EChatSettingsType::Type Option) const = 0;

	/*
	 * Set an option that is stored as a bool
	 * @param Option The Option to set the value for
	 * @param CheckState What to set the option too
	 */
	virtual void SetFlagOption(EChatSettingsType::Type Option, bool CheckState) = 0;

	/*
	 * Get an option that is stored as an enumeration value
	 * @param Option The Option to get the value for
	 * @param Index The Index of the enumeration type
	 */
	virtual bool GetRadioOption(EChatSettingsType::Type Option, uint8 Index) const = 0;

	/*
	* Get an option that is stored as an enumeration value
	* @param Option The Option to get the value for
	*/
	virtual uint8 GetRadioOptionIndex(EChatSettingsType::Type Option) const = 0;

	/*
	 * Set an option that is stored as an enumeration value
	 * @param Option The Option to set the value for
	 * @param Index The Index of the enumeration type
	 */
	virtual void SetRadioOption(EChatSettingsType::Type Option, uint8 Index) = 0;

	/*
	 * Get an option that is stored as a float
	 * @param Option The Option to get the value for
	 */
	virtual float GetSliderValue(EChatSettingsType::Type OptionType) const = 0;

	/*
	 * Set an option that is stored as a float
	 * @param Option The Option to set the value for
	 * @param Value The value to set
	 */
	virtual void SetSliderValue(EChatSettingsType::Type OptionType, float Value) = 0;

	// Events
	DECLARE_EVENT_TwoParams(IChatSettingsService, FChatSettingsStateUpdated, EChatSettingsType::Type, bool);
	virtual FChatSettingsStateUpdated& OnChatSettingStateUpdated() = 0;

	DECLARE_EVENT_TwoParams(IChatSettingsService, FChatSettingsRadioStateUpdated, EChatSettingsType::Type, uint8);
	virtual FChatSettingsRadioStateUpdated& OnChatSettingRadioStateUpdated() = 0;

	DECLARE_EVENT_TwoParams(IChatSettingsService, FChatSettingsValueUpdated, EChatSettingsType::Type, float);
	virtual FChatSettingsValueUpdated& OnChatSettingValueUpdated() = 0;
};