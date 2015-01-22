// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "TwitchProjectSettings.generated.h"

UCLASS(Config=Engine, DefaultConfig)
class UTwitchProjectSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** This is the Twitch Client ID for your game's Twitch application.  You'll need to supply a valid Client ID for your game in order for 
		it to be allowed to broadcast to Twitch.  You can get a Client ID by visiting the Twitch web site, logging in and creating a
		Twitch Application for your game.  Copy the Client ID from the Twitch web site into this field.  */
	UPROPERTY(Config, EditAnywhere, Category=Settings)
	FString ApplicationClientID;

	/** The redirect URI is the web page that the user will be redirected to after authenticating with Twitch.  Usually this page
		would show that the login was successful and the user can simply close the browser tab.  If you don't want to host a page
		you can simply point this to a localhost port.  Remember, Twitch requires you to set the RedirectURI in your application's
		configuration page on the Twitch web site.  If the URI doesn't exactly match what is here, authentication will fail. */
	UPROPERTY(Config, EditAnywhere, Category=Settings)
	FString RedirectURI;

};


