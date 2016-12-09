// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
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

	/** The port number on the localhost to use when authenticating using implicit grant authentication.  This should be the same
	    port number as the one used in the RedirectURI. */
	UPROPERTY( Config, EditAnywhere, Category = Settings )
	int32 LocalPortNumber;

	/** The web page the user should be sent to after they've logged in successfully.  This page should usually tell the user
	    that they've logged in, and they can task switch back to the application now. */
	UPROPERTY( Config, EditAnywhere, Category = Settings )
	FString AuthSuccessRedirectURI;

	/** The web page the user should be sent to if something went wrong after logging in. */
	UPROPERTY( Config, EditAnywhere, Category = Settings )
	FString AuthFailureRedirectURI;

	/** Twitch "client secret" ID.  This is required if you want to use direct authentication instead of authenticating through
	    a web browser. */
	UPROPERTY( Config, EditAnywhere, Category = Settings )
	FString DirectAuthenticationClientSecretID;
};


