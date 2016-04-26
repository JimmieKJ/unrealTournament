// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayDebuggerTypes.h"
#include "GameplayDebuggerConfig.generated.h"

USTRUCT()
struct GAMEPLAYDEBUGGER_API FGameplayDebuggerInputConfig
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, Category = Input)
	uint32 bShift : 1;

	UPROPERTY(EditAnywhere, Category = Input)
	uint32 bCtrl : 1;

	UPROPERTY(EditAnywhere, Category = Input)
	uint32 bAlt : 1;

	UPROPERTY(EditAnywhere, Category = Input)
	uint32 bCmd : 1;

	FGameplayDebuggerInputModifier CreateModifier() const
	{
		return FGameplayDebuggerInputModifier(bShift, bCtrl, bAlt, bCmd);
	}
};

UCLASS(config = Engine, defaultconfig)
class GAMEPLAYDEBUGGER_API UGameplayDebuggerConfig : public UObject
{
	GENERATED_UCLASS_BODY()

	/** key used to activate visual debugger tool */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey ActivationKey;

	/** select next category row */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey CategoryRowNextKey;

	/** select previous category row */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey CategoryRowPrevKey;

	/** select category slot 0 */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey CategorySlot0;

	/** select category slot 1 */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey CategorySlot1;

	/** select category slot 2 */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey CategorySlot2;

	/** select category slot 3 */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey CategorySlot3;

	/** select category slot 4 */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey CategorySlot4;

	/** select category slot 5 */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey CategorySlot5;

	/** select category slot 6 */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey CategorySlot6;

	/** select category slot 7 */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey CategorySlot7;

	/** select category slot 8 */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey CategorySlot8;

	/** select category slot 9 */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey CategorySlot9;

	/** toggle game hud */
	UPROPERTY(config, EditAnywhere, Category = Extensions)
	FKey GameHUDKey;

	UPROPERTY(config, EditAnywhere, Category = Extensions, meta = (EditCondition = "bUseGameHUDModifiers"))
	FGameplayDebuggerInputConfig GameHUDModifiers;

	/** toggle onscreen debug messages */
	UPROPERTY(config, EditAnywhere, Category = Extensions)
	FKey DebugMessagesKey;

	UPROPERTY(config, EditAnywhere, Category = Extensions, meta = (EditCondition = "bUseDebugMessagesModifiers"))
	FGameplayDebuggerInputConfig DebugMessagesModifiers;

	/** toggle spectator mode */
	UPROPERTY(config, EditAnywhere, Category = Extensions)
	FKey SpectatorKey;

	UPROPERTY(config, EditAnywhere, Category = Extensions, meta = (EditCondition = "bUseSpectatorModifiers"))
	FGameplayDebuggerInputConfig SpectatorModifiers;

	/** if set, game hud extension will be automatically added */
	UPROPERTY(config, EditAnywhere, Category = Extensions)
	uint32 bEnableExtension_GameHUD : 1;

	/** if set, spectator mode extension will be automatically added */
	UPROPERTY(config, EditAnywhere, Category = Extensions)
	uint32 bEnableExtension_Spectator : 1;

	UPROPERTY()
	uint32 bUseGameHUDModifiers : 1;
	
	UPROPERTY()
	uint32 bUseDebugMessagesModifiers : 1;
	
	UPROPERTY()
	uint32 bUseSpectatorModifiers : 1;

	/** additional canvas padding: left */
	UPROPERTY(config, EditAnywhere, Category = Display)
	float DebugCanvasPaddingLeft;

	/** additional canvas padding: right */
	UPROPERTY(config, EditAnywhere, Category = Display)
	float DebugCanvasPaddingRight;

	/** additional canvas padding: top */
	UPROPERTY(config, EditAnywhere, Category = Display)
	float DebugCanvasPaddingTop;

	/** additional canvas padding: bottom */
	UPROPERTY(config, EditAnywhere, Category = Display)
	float DebugCanvasPaddingBottom;
};
