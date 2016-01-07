// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "ChatSettingsService.h"
#include "FriendsAndChatUserSettings.h"

class FChatSettingsServiceImpl
	: public FChatSettingsService
{
public:

private:
	FChatSettingsServiceImpl()
	{
		ChatSettings.FontSize = EChatFontType::Small;
		ChatSettings.WindowHeight = EChatWindowHeight::Standard;
		ChatSettings.WindowOpacity = 0.5f;
		ChatSettings.bHideOffline = false;
		ChatSettings.bHideOutgoing = false;
		ChatSettings.bHideSuggestions = false;
		ChatSettings.bHideRecent = false;
		ChatSettings.bHideBlocked = true;
		LoadConfig();
	}

	virtual void LoadConfig() override
	{
		GConfig->GetBool(TEXT("ChatSettings"), TEXT("ShowNotifications"), ChatSettings.bShowNotifications, GGameIni);
		GConfig->GetBool(TEXT("ChatSettings"), TEXT("NeverFadeMessages"), ChatSettings.bNeverFadeMessages, GGameIni);
		GConfig->GetBool(TEXT("ChatSettings"), TEXT("ShowTimeStamps"), ChatSettings.bShowTimeStamps, GGameIni);
		GConfig->GetBool(TEXT("ChatSettings"), TEXT("ShowWhispersInAllTabs"), ChatSettings.bShowWhispersInAllTabs, GGameIni);
		GConfig->GetBool(TEXT("ChatSettings"), TEXT("ShowCustomTab"), ChatSettings.bShowCustomTab, GGameIni);
		GConfig->GetBool(TEXT("ChatSettings"), TEXT("ShowWhispersTab"), ChatSettings.bShowWhispersTab, GGameIni);
		GConfig->GetBool(TEXT("ChatSettings"), TEXT("ShowCombatAndEventsTab"), ChatSettings.bShowCombatAndEventsTab, GGameIni);
		GConfig->GetBool(TEXT("ChatSettings"), TEXT("ShowClanTabs"), ChatSettings.bShowClanTabs, GGameIni);
		GConfig->GetBool(TEXT("ChatSettings"), TEXT("HideOffline"), ChatSettings.bHideOffline, GGameIni);
		GConfig->GetBool(TEXT("ChatSettings"), TEXT("HideOutgoing"), ChatSettings.bHideOutgoing, GGameIni);
		GConfig->GetBool(TEXT("ChatSettings"), TEXT("HideSuggestions"), ChatSettings.bHideSuggestions, GGameIni);
		GConfig->GetBool(TEXT("ChatSettings"), TEXT("HideRecent"), ChatSettings.bHideRecent, GGameIni);
		GConfig->GetBool(TEXT("ChatSettings"), TEXT("HideBlocked"), ChatSettings.bHideBlocked, GGameIni);
		
		int32 FontSize = ChatSettings.FontSize;
		GConfig->GetInt(TEXT("ChatSettings"), TEXT("FontSize"), FontSize, GGameIni);
		ChatSettings.FontSize = EChatFontType::Type(FontSize);

		int32 WindowHeight = ChatSettings.WindowHeight;
		GConfig->GetInt(TEXT("ChatSettings"), TEXT("WindowHeight"), WindowHeight, GGameIni);
		ChatSettings.WindowHeight = EChatWindowHeight::Type(WindowHeight);

		GConfig->GetFloat(TEXT("ChatSettings"), TEXT("WindowOpacity"), ChatSettings.WindowOpacity, GGameIni);
	}

	virtual void SaveConfig() override
	{
		GConfig->SetBool(TEXT("ChatSettings"), TEXT("ShowNotifications"), ChatSettings.bShowNotifications, GGameIni);
		GConfig->SetBool(TEXT("ChatSettings"), TEXT("NeverFadeMessages"), ChatSettings.bNeverFadeMessages, GGameIni);
		GConfig->SetBool(TEXT("ChatSettings"), TEXT("ShowTimeStamps"), ChatSettings.bShowTimeStamps, GGameIni);
		GConfig->SetBool(TEXT("ChatSettings"), TEXT("ShowWhispersInAllTabs"), ChatSettings.bShowWhispersInAllTabs, GGameIni);
		GConfig->SetBool(TEXT("ChatSettings"), TEXT("ShowCustomTab"), ChatSettings.bShowCustomTab, GGameIni);
		GConfig->SetBool(TEXT("ChatSettings"), TEXT("ShowWhispersTab"), ChatSettings.bShowWhispersTab, GGameIni);
		GConfig->SetBool(TEXT("ChatSettings"), TEXT("ShowCombatAndEventsTab"), ChatSettings.bShowCombatAndEventsTab, GGameIni);
		GConfig->SetBool(TEXT("ChatSettings"), TEXT("ShowClanTabs"), ChatSettings.bShowClanTabs, GGameIni);
		GConfig->SetBool(TEXT("ChatSettings"), TEXT("HideOffline"), ChatSettings.bHideOffline, GGameIni);
		GConfig->SetBool(TEXT("ChatSettings"), TEXT("HideOutgoing"), ChatSettings.bHideOutgoing, GGameIni);
		GConfig->SetBool(TEXT("ChatSettings"), TEXT("HideSuggestions"), ChatSettings.bHideSuggestions, GGameIni);
		GConfig->SetBool(TEXT("ChatSettings"), TEXT("HideRecent"), ChatSettings.bHideRecent, GGameIni);
		GConfig->SetBool(TEXT("ChatSettings"), TEXT("HideBlocked"), ChatSettings.bHideBlocked, GGameIni);

		GConfig->SetInt(TEXT("ChatSettings"), TEXT("FontSize"), ChatSettings.FontSize, GGameIni);
		GConfig->SetInt(TEXT("ChatSettings"), TEXT("WindowHeight"), ChatSettings.WindowHeight, GGameIni);

		GConfig->SetFloat(TEXT("ChatSettings"), TEXT("WindowOpacity"), ChatSettings.WindowOpacity, GGameIni);
	}

	virtual bool GetFlagOption(EChatSettingsType::Type Option) const override
	{
		check(EChatSettingsOptionType::GetOptionType(Option) == EChatSettingsOptionType::CheckBox);
		switch(Option)
		{
		case EChatSettingsType::ShowNotifications:
		{
			return ChatSettings.bShowNotifications; 
			break;
		}
		case EChatSettingsType::NeverFadeMessages:
		{
			return ChatSettings.bNeverFadeMessages; 
			break;
		}
		case EChatSettingsType::ShowTimeStamps:
		{
			return ChatSettings.bShowTimeStamps; 
			break;
		}
		case EChatSettingsType::ShowWhispersInAllTabs:
		{
			return ChatSettings.bShowWhispersInAllTabs; 
			break;
		}
		case EChatSettingsType::ShowCustomTab:
		{
			return ChatSettings.bShowCustomTab; 
			break;
		}
		case EChatSettingsType::ShowWhispersTab:
		{
			return ChatSettings.bShowWhispersTab;
			break;
		}
		case EChatSettingsType::ShowCombatAndEventsTab:
		{
			return ChatSettings.bShowCombatAndEventsTab; 
			break;
		}
		case EChatSettingsType::ShowClanTabs:
		{
			return ChatSettings.bShowClanTabs;
			break;
		}
		case EChatSettingsType::HideOffline:
		{
			return ChatSettings.bHideOffline;
			break;
		}
		case EChatSettingsType::HideOutgoing:
		{
			return ChatSettings.bHideOutgoing;
			break;
		}
		case EChatSettingsType::HideSuggestions:
		{
			return ChatSettings.bHideSuggestions;
			break;
		}
		case EChatSettingsType::HideRecent:
		{
			return ChatSettings.bHideRecent;
			break;
		}
		case EChatSettingsType::HideBlocked:
		{
			return ChatSettings.bHideBlocked;
			break;
		}
		default:
			return false;
		};
	}

	virtual void SetFlagOption(EChatSettingsType::Type Option, bool CheckState) override
	{
		check(EChatSettingsOptionType::GetOptionType(Option) == EChatSettingsOptionType::CheckBox);
		switch (Option)
		{
		case EChatSettingsType::ShowNotifications:
		{
			ChatSettings.bShowNotifications = CheckState;
			break;
		}
		case EChatSettingsType::NeverFadeMessages:
		{
			ChatSettings.bNeverFadeMessages = CheckState;
			break;
		}
		case EChatSettingsType::ShowTimeStamps:
		{
			ChatSettings.bShowTimeStamps = CheckState;
			break;
		}
		case EChatSettingsType::ShowWhispersInAllTabs:
		{
			ChatSettings.bShowWhispersInAllTabs = CheckState;
			break;
		}
		case EChatSettingsType::ShowCustomTab:
		{
			ChatSettings.bShowCustomTab = CheckState;
			break;
		}
		case EChatSettingsType::ShowWhispersTab:
		{
			ChatSettings.bShowWhispersTab = CheckState;
			break;
		}
		case EChatSettingsType::ShowCombatAndEventsTab:
		{
			ChatSettings.bShowCombatAndEventsTab = CheckState;
			break;
		}
		case EChatSettingsType::ShowClanTabs:
		{
			ChatSettings.bShowClanTabs = CheckState;
			break;
		}
		case EChatSettingsType::HideOffline:
		{
			ChatSettings.bHideOffline = CheckState;
			break;
		}
		case EChatSettingsType::HideOutgoing:
		{
			ChatSettings.bHideOutgoing = CheckState;
			break;
		}
		case EChatSettingsType::HideSuggestions:
		{
			ChatSettings.bHideSuggestions = CheckState;
			break;
		}
		case EChatSettingsType::HideRecent:
		{
			ChatSettings.bHideRecent = CheckState;
			break;
		}
		case EChatSettingsType::HideBlocked:
		{
			ChatSettings.bHideBlocked = CheckState;
			break;
		}
		};
		OnChatSettingStateUpdated().Broadcast(Option, CheckState);
		SaveConfig();
	}

	virtual bool GetRadioOption(EChatSettingsType::Type Option, uint8 Index) const override
	{
		check(EChatSettingsOptionType::GetOptionType(Option) == EChatSettingsOptionType::RadioBox);
		switch (Option)
		{
		case EChatSettingsType::FontSize:
		{
			return ChatSettings.FontSize == EChatFontType::Type(Index);
			break;
		}
		case EChatSettingsType::WindowHeight:
		{
			return ChatSettings.WindowHeight == EChatWindowHeight::Type(Index);
			break;
		}
		default:
			return false;
		}
	}

	virtual uint8 GetRadioOptionIndex(EChatSettingsType::Type Option) const override
	{
		check(EChatSettingsOptionType::GetOptionType(Option) == EChatSettingsOptionType::RadioBox);
		switch (Option)
		{
		case EChatSettingsType::FontSize:
		{
			return ChatSettings.FontSize;
			break;
		}
		case EChatSettingsType::WindowHeight:
		{
			return ChatSettings.WindowHeight;
			break;
		}
		default:
			return 0;
		}
	}

	virtual void SetRadioOption(EChatSettingsType::Type Option, uint8 Index) override
	{
		check(EChatSettingsOptionType::GetOptionType(Option) == EChatSettingsOptionType::RadioBox);
		switch (Option)
		{
		case EChatSettingsType::FontSize:
		{
			ChatSettings.FontSize = EChatFontType::Type(Index);
			break;
		}
		case EChatSettingsType::WindowHeight:
		{
			ChatSettings.WindowHeight = EChatWindowHeight::Type(Index);
			
			break;
		}
		};
		OnChatSettingRadioStateUpdated().Broadcast(Option, Index);
		SaveConfig();
	}

	virtual float GetSliderValue(EChatSettingsType::Type OptionType) const override
	{
		check(EChatSettingsOptionType::GetOptionType(OptionType) == EChatSettingsOptionType::Slider);
		if (OptionType == EChatSettingsType::WindowOpacity)
		{
			return ChatSettings.WindowOpacity;
		}
		return 0;
	}

	virtual void SetSliderValue(EChatSettingsType::Type OptionType, float Value) override
	{
		check(EChatSettingsOptionType::GetOptionType(OptionType) == EChatSettingsOptionType::Slider);
		if (OptionType == EChatSettingsType::WindowOpacity)
		{
			ChatSettings.WindowOpacity = Value;
		}
		OnChatSettingValueUpdated().Broadcast(OptionType, Value);
		SaveConfig();
	}

	DECLARE_DERIVED_EVENT(FChatSettingsServiceImpl, IChatSettingsService::FChatSettingsStateUpdated, FChatSettingsStateUpdated);
	virtual FChatSettingsStateUpdated& OnChatSettingStateUpdated() override
	{
		return ChatSettingsStateUpdatedEvent;
	}

	DECLARE_DERIVED_EVENT(FChatSettingsServiceImpl, IChatSettingsService::FChatSettingsRadioStateUpdated, FChatSettingsRadioStateUpdated);
	virtual FChatSettingsRadioStateUpdated& OnChatSettingRadioStateUpdated() override
	{
		return ChatSettingsRadioStateUpdatedEvent;
	}

	DECLARE_DERIVED_EVENT(FChatSettingsServiceImpl, IChatSettingsService::FChatSettingsValueUpdated, FChatSettingsValueUpdated);
	virtual FChatSettingsValueUpdated& OnChatSettingValueUpdated() override
	{
		return ChatSettingsValueUpdatedEvent;
	}

private:
	
	FFriendsAndChatSettings ChatSettings;
	FChatSettingsStateUpdated ChatSettingsStateUpdatedEvent;
	FChatSettingsRadioStateUpdated ChatSettingsRadioStateUpdatedEvent;
	FChatSettingsValueUpdated ChatSettingsValueUpdatedEvent;

	friend FChatSettingsServiceFactory;
};

TSharedRef< FChatSettingsService > FChatSettingsServiceFactory::Create()
{
	TSharedRef< FChatSettingsServiceImpl > ChatSettingsService(new FChatSettingsServiceImpl());
	return ChatSettingsService;
}