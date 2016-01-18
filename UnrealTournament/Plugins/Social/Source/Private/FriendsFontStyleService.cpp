// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "FriendsFontStyleService.h"
#include "ChatSettingsService.h"

class FFriendsFontStyleServiceImpl
	: public FFriendsFontStyleService
{
public:

	virtual void SetFontStyles(const FFriendsFontStyle& InFriendsSmallFontStyle, const FFriendsFontStyle& InFriendsNormalFontStyle, const FFriendsFontStyle& InFriendsLargeFontStyle) override
	{
		FriendsSmallFontStyle = InFriendsSmallFontStyle;
		FriendsNormalFontStyle = InFriendsNormalFontStyle;
		FriendsLargeFontStyle = InFriendsLargeFontStyle;
	}

	virtual void SetUserFontSize(EChatFontType::Type InFontSize) override
	{
		FontSize = InFontSize;
	}

	virtual FSlateFontInfo GetSmallFont() const override
	{
		if (FontSize == EChatFontType::Large)
		{
			return FriendsSmallFontStyle.FriendsFontLarge;
		}
		if (FontSize == EChatFontType::Small)
		{
			return FriendsSmallFontStyle.FriendsFontSmall;
		}
		return FriendsSmallFontStyle.FriendsFontNormal;
	}

	virtual FSlateFontInfo GetSmallBoldFont() const override
	{
		if (FontSize == EChatFontType::Large)
		{
			return FriendsSmallFontStyle.FriendsFontLargeBold;
		}
		if (FontSize == EChatFontType::Small)
		{
			return FriendsSmallFontStyle.FriendsFontSmallBold;
		}
		return FriendsSmallFontStyle.FriendsFontNormalBold;
	}

	virtual FSlateFontInfo GetNormalFont() const override
	{
		if (FontSize == EChatFontType::Large)
		{
			return FriendsNormalFontStyle.FriendsFontLarge;
		}
		if (FontSize == EChatFontType::Small)
		{
			return FriendsNormalFontStyle.FriendsFontSmall;
		}
		return FriendsNormalFontStyle.FriendsFontNormal;
	}

	virtual FSlateFontInfo GetNormalBoldFont() const override
	{
		if (FontSize == EChatFontType::Large)
		{
			return FriendsNormalFontStyle.FriendsFontLargeBold;
		}
		if (FontSize == EChatFontType::Small)
		{
			return FriendsNormalFontStyle.FriendsFontSmallBold;
		}
		return FriendsNormalFontStyle.FriendsFontNormalBold;
	}

	virtual FSlateFontInfo GetLargeFont() const override
	{
		if (FontSize == EChatFontType::Large)
		{
			return FriendsLargeFontStyle.FriendsFontLarge;
		}
		if (FontSize == EChatFontType::Small)
		{
			return FriendsLargeFontStyle.FriendsFontSmall;
		}
		return FriendsLargeFontStyle.FriendsFontNormal;
	}

	virtual FSlateFontInfo GetLargeBoldFont() const override
	{
		if (FontSize == EChatFontType::Large)
		{
			return FriendsLargeFontStyle.FriendsFontLargeBold;
		}
		if (FontSize == EChatFontType::Small)
		{
			return FriendsLargeFontStyle.FriendsFontSmallBold;
		}
		return FriendsLargeFontStyle.FriendsFontNormalBold;
	}

private:
	
	FFriendsFontStyleServiceImpl(const TSharedRef<class FChatSettingsService>& InSettingsService)
	{
		SettingsService = InSettingsService;
		FontSize = EChatFontType::Type(SettingsService->GetRadioOptionIndex(EChatSettingsType::FontSize));		
	}

	void Initialize()
	{
		SettingsService->OnChatSettingRadioStateUpdated().AddSP(this, &FFriendsFontStyleServiceImpl::SettingUpdated);
	}

	void SettingUpdated(EChatSettingsType::Type Setting, uint8 InValue)
	{
		if (Setting == EChatSettingsType::FontSize)
		{
			FontSize = EChatFontType::Type(InValue);
		}
	}

	FFriendsFontStyle FriendsSmallFontStyle;
	FFriendsFontStyle FriendsNormalFontStyle;
	FFriendsFontStyle FriendsLargeFontStyle;
	EChatFontType::Type FontSize;
	TSharedPtr<FChatSettingsService> SettingsService;

	friend FFriendsFontStyleServiceFactory;
};

TSharedRef< FFriendsFontStyleService > FFriendsFontStyleServiceFactory::Create(const TSharedRef<class FChatSettingsService>& SettingsService)
{
	TSharedRef< FFriendsFontStyleServiceImpl > StyleService(new FFriendsFontStyleServiceImpl(SettingsService));
	StyleService->Initialize();
	return StyleService;
}