// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameMapsSettings.generated.h"


/** Ways the screen can be split with two players. */
UENUM()
namespace ETwoPlayerSplitScreenType
{
	enum Type
	{
		Horizontal,
		Vertical
	};
}


/** Ways the screen can be split with three players. */
UENUM()
namespace EThreePlayerSplitScreenType
{
	enum Type
	{
		FavorTop,
		FavorBottom
	};
}


UCLASS(config=Engine, defaultconfig)
class ENGINESETTINGS_API UGameMapsSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

	/**
	 * Get the default map specified in the settings.
	 * Makes a choice based on running as listen server/client vs dedicated server
	 *
	 * @return the default map specified in the settings
	 */
	static const FString& GetGameDefaultMap( );

	/**
	 * Get the global default game type specified in the configuration
	 * Makes a choice based on running as listen server/client vs dedicated server
	 * 
	 * @return the proper global default game type
	 */
	static const FString& GetGlobalDefaultGameMode( );

	/**
	 * Set the default map to use (see GameDefaultMap below)
	 *
	 * @param NewMap name of valid map to use
	 */
	static void SetGameDefaultMap( const FString& NewMap );

	/**
	 * Set the default game type (see GlobalDefaultGameMode below)
	 *
	 * @param NewGameMode name of valid map to use
	 */
	static void SetGlobalDefaultGameMode( const FString& NewGameMode );

public:

	/** If set, this map will be loaded when the Editor starts up. */
	UPROPERTY(config, EditAnywhere, Category=DefaultMaps)
	FString EditorStartupMap;

	/** The default options that will be appended to a map being loaded. */
	UPROPERTY(config, EditAnywhere, Category=DefaultMaps, AdvancedDisplay)
	FString LocalMapOptions;

	/** The map loaded when transition from one map to another. */
	UPROPERTY(config, EditAnywhere, Category=DefaultMaps, AdvancedDisplay)
	FString TransitionMap;

	/** Whether the screen should be split or not when multiple local players are present */
	UPROPERTY(config, EditAnywhere, Category=LocalMultiplayer)
	bool bUseSplitscreen;

	/** The viewport layout to use if the screen should be split and there are two local players */
	UPROPERTY(config, EditAnywhere, Category=LocalMultiplayer, meta=(editcondition="bUseSplitScreen"))
	TEnumAsByte<ETwoPlayerSplitScreenType::Type> TwoPlayerSplitscreenLayout;

	/** The viewport layout to use if the screen should be split and there are three local players */
	UPROPERTY(config, EditAnywhere, Category=LocalMultiplayer, meta=(editcondition="bUseSplitScreen"))
	TEnumAsByte<EThreePlayerSplitScreenType::Type> ThreePlayerSplitscreenLayout;

	/** The class to use when instantiating the transient GameInstance class */
	UPROPERTY(config, noclear, EditAnywhere, Category=GameInstance, meta=(MetaClass="GameInstance"))
	FStringClassReference GameInstanceClass;

private:

	/** The map that will be loaded by default when no other map is loaded. */
	UPROPERTY(config, EditAnywhere, Category=DefaultMaps)
	FString GameDefaultMap;

	/** The map that will be loaded by default when no other map is loaded (DEDICATED SERVER). */
	UPROPERTY(config, EditAnywhere, Category=DefaultMaps, AdvancedDisplay)
	FString ServerDefaultMap;

	/** GameMode to use if not specified in any other way. (e.g. per-map DefaultGameMode or on the URL). */
	UPROPERTY(config, noclear, EditAnywhere, Category=DefaultModes, meta=(MetaClass="GameMode", DisplayName="Default GameMode"))
	FStringClassReference GlobalDefaultGameMode;

	/**
	 * GameMode to use if not specified in any other way. (e.g. per-map DefaultGameMode or on the URL) (DEDICATED SERVERS)
	 * If not set, the GlobalDefaultGameMode value will be used.
	 */
	UPROPERTY(config, EditAnywhere, Category=DefaultModes, meta=(MetaClass="GameMode"), AdvancedDisplay)
	FStringClassReference GlobalDefaultServerGameMode;
};
