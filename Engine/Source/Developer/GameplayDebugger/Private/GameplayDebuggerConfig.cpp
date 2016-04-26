// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerPrivatePCH.h"
#include "GameplayDebuggerConfig.h"
#include "GameplayDebuggerTypes.h"

UGameplayDebuggerConfig::UGameplayDebuggerConfig(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ActivationKey = EKeys::Apostrophe;

	CategoryRowNextKey = EKeys::Add;
	CategoryRowPrevKey = EKeys::Subtract;

	CategorySlot0 = EKeys::NumPadZero;
	CategorySlot1 = EKeys::NumPadOne;
	CategorySlot2 = EKeys::NumPadTwo;
	CategorySlot3 = EKeys::NumPadThree;
	CategorySlot4 = EKeys::NumPadFour;
	CategorySlot5 = EKeys::NumPadFive;
	CategorySlot6 = EKeys::NumPadSix;
	CategorySlot7 = EKeys::NumPadSeven;
	CategorySlot8 = EKeys::NumPadEight;
	CategorySlot9 = EKeys::NumPadNine;

	GameHUDKey = EKeys::Tilde;
	GameHUDModifiers.bCtrl = true;
	bUseGameHUDModifiers = true;
	DebugMessagesKey = EKeys::Tab;
	DebugMessagesModifiers.bCtrl = true;
	bUseDebugMessagesModifiers = true;
	SpectatorKey = EKeys::Tab;

	bEnableExtension_GameHUD = true;
	bEnableExtension_Spectator = true;

	DebugCanvasPaddingLeft = 10.0f;
	DebugCanvasPaddingRight = 10.0f;
	DebugCanvasPaddingTop = 10.0f;
	DebugCanvasPaddingBottom = 10.0f;
}
