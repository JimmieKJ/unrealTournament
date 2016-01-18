// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FFriendsFontStyleService
	: public TSharedFromThis<FFriendsFontStyleService>
{
public:
	virtual ~FFriendsFontStyleService() {}

	virtual void SetFontStyles(const FFriendsFontStyle& FriendsSmallFontStyle, const FFriendsFontStyle& FriendsNormalFontStyle, const FFriendsFontStyle& FriendsLargeFontStyle) = 0;

	virtual void SetUserFontSize(EChatFontType::Type FontSize) = 0;

	virtual FSlateFontInfo GetSmallFont() const = 0;
	virtual FSlateFontInfo GetSmallBoldFont() const = 0;

	virtual FSlateFontInfo GetNormalFont() const = 0;
	virtual FSlateFontInfo GetNormalBoldFont() const = 0;

	virtual FSlateFontInfo GetLargeFont() const = 0;
	virtual FSlateFontInfo GetLargeBoldFont() const = 0;
};

/**
* Creates the implementation for an FFriendsFontStyleService.
*
* @return the newly created FFriendsFontStyleService implementation.
*/
FACTORY(TSharedRef< FFriendsFontStyleService >, FFriendsFontStyleService, const TSharedRef<class FChatSettingsService>& SettingsService);