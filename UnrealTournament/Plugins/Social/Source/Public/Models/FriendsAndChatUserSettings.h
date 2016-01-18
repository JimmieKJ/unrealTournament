// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace EChatSettingsType
{
	enum Type : uint8
	{
		ShowNotifications, // Show Notification
		NeverFadeMessages,
		ShowTimeStamps,
		ShowWhispersInAllTabs,
		ShowCustomTab,
		ShowWhispersTab,
		ShowCombatAndEventsTab,
		ShowClanTabs,
		FontSize,
		WindowHeight,
		WindowOpacity,
		HideOffline,
		HideOutgoing,
		HideSuggestions,
		HideRecent,
		HideBlocked,

		/** Invalid enum type, may be used as a number of enumerations. */
		InvalidOrMax
	};

	inline FText ToText(EChatSettingsType::Type State)
	{
		switch (State)
		{
			case ShowNotifications: return NSLOCTEXT("ChatSettingsType","ShowNotifications", "Show notifications");
			case NeverFadeMessages: return NSLOCTEXT("ChatSettingsType", "NeverFadeMessages", "Never fade messages away");
			case ShowTimeStamps: return NSLOCTEXT("ChatSettingsType", "ShowTimeStamps", "Show time stamps");
			case ShowWhispersInAllTabs: return NSLOCTEXT("ChatSettingsType", "ShowWhispersInAllTabs", "Show whispers in all tabs");
			case ShowCustomTab: return NSLOCTEXT("ChatSettingsType", "ShowCustomTab", "Show custom tab");
			case ShowWhispersTab: return NSLOCTEXT("ChatSettingsType", "ShowWhispersTab", "Show whisper tab");
			case ShowCombatAndEventsTab: return NSLOCTEXT("ChatSettingsType", "ShowCombatAndEventsTab", "Show combat and events tab");
			case ShowClanTabs: return NSLOCTEXT("ChatSettingsType", "ShowClanTabs", "Show clan tab");
			case FontSize: return NSLOCTEXT("ChatSettingsType", "FontSize", "Font size");
			case WindowHeight: return NSLOCTEXT("ChatSettingsType", "WindowHeight", "Height");
			case WindowOpacity: return NSLOCTEXT("ChatSettingsType", "WindowOpacity", "Opacity");
			case HideOffline: return NSLOCTEXT("ChatSettingsType", "Offline", "Hide Offline Friends");
			case HideOutgoing: return NSLOCTEXT("ChatSettingsType", "Outgoing", "Hide Outgoing Requests");
			case HideSuggestions: return NSLOCTEXT("ChatSettingsType", "Suggestions", "Hide Friend Suggestions");
			case HideRecent: return NSLOCTEXT("ChatSettingsType", "Recent", "Hide Recent Players");
			case HideBlocked: return NSLOCTEXT("ChatSettingsType", "Blocked", "Hide Blocked Players");
			default: return FText::GetEmpty();
		}
	}
};

namespace EChatSettingsOptionType
{
	enum Type : uint8
	{
		CheckBox,
		RadioBox,
		Slider
	};

	inline EChatSettingsOptionType::Type GetOptionType(EChatSettingsType::Type State)
	{
		switch (State)
		{
		case EChatSettingsType::ShowNotifications:
		case EChatSettingsType::NeverFadeMessages:
		case EChatSettingsType::ShowTimeStamps:
		case EChatSettingsType::ShowWhispersInAllTabs:
		case EChatSettingsType::ShowCustomTab:
		case EChatSettingsType::ShowWhispersTab:
		case EChatSettingsType::ShowCombatAndEventsTab:
		case EChatSettingsType::ShowClanTabs:
		case EChatSettingsType::HideOffline:
		case EChatSettingsType::HideOutgoing:
		case EChatSettingsType::HideSuggestions:
		case EChatSettingsType::HideRecent:
		case EChatSettingsType::HideBlocked:
			return EChatSettingsOptionType::CheckBox;
		case EChatSettingsType::FontSize:
		case EChatSettingsType::WindowHeight:
			return EChatSettingsOptionType::RadioBox;
		case EChatSettingsType::WindowOpacity:
			return EChatSettingsOptionType::Slider;
		default:
			return EChatSettingsOptionType::CheckBox;
		}
	}
};

namespace EChatFontType
{
	enum Type : uint8
	{
		Small,
		Standard,
		Large,
		/** Invalid enum type, may be used as a number of enumerations. */
		InvalidOrMax
	};

	inline FText ToText(EChatFontType::Type State)
	{
		switch (State)
		{
		case Small: return NSLOCTEXT("ChatSettingsType", "Small", "Small");
		case Standard: return NSLOCTEXT("ChatSettingsType", "Standard", "Standard");
		case Large: return NSLOCTEXT("ChatSettingsType", "Large", "Large");
		default: return FText::GetEmpty();
		}
	}
};

namespace EChatWindowHeight
{
	enum Type : uint8
	{
		Standard,
		Tall,
		/** Invalid enum type, may be used as a number of enumerations. */
		InvalidOrMax
	};

	inline FText ToText(EChatWindowHeight::Type State)
	{
		switch (State)
		{
		case Standard: return NSLOCTEXT("ChatSettingsType", "Standard", "Standard");
		case Tall: return NSLOCTEXT("ChatSettingsType", "Tall", "Tall");
		default: return FText::GetEmpty();
		}
	}
};

struct FFriendsAndChatSettings
{
	bool bShowNotifications;
	bool bNeverFadeMessages;
	bool bShowTimeStamps;
	bool bShowWhispersInAllTabs;
	bool bShowCustomTab;
	bool bShowWhispersTab;
	bool bShowCombatAndEventsTab;
	bool bShowClanTabs;
	bool bHideOffline;
	bool bHideOutgoing;
	bool bHideSuggestions;
	bool bHideRecent;
	bool bHideBlocked;
	EChatFontType::Type	FontSize;
	EChatWindowHeight::Type WindowHeight;
	float WindowOpacity;
};
