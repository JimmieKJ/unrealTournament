// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTATypes.generated.h"

// Const defines for Dialogs
const uint16 UTDIALOG_BUTTON_OK = 0x0001;			
const uint16 UTDIALOG_BUTTON_CANCEL = 0x0002;
const uint16 UTDIALOG_BUTTON_YES = 0x0004;
const uint16 UTDIALOG_BUTTON_NO = 0x0008;
const uint16 UTDIALOG_BUTTON_HELP = 0x0010;
const uint16 UTDIALOG_BUTTON_RECONNECT = 0x0020;
const uint16 UTDIALOG_BUTTON_EXIT = 0x0040;
const uint16 UTDIALOG_BUTTON_QUIT = 0x0080;

DECLARE_DELEGATE_OneParam(FDialogResultDelegate, uint16);

UENUM()
namespace EGameStage
{
	enum Type
	{
		Initializing,
		PreGame, 
		GameInProgress,
		GameOver,
		MAX,
	};
}

UENUM()
namespace ETextHorzPos
{
	enum Type
	{
		Left,
		Center, 
		Right,
		MAX,
	};
}

UENUM()
namespace ETextVertPos
{
	enum Type
	{
		Top,
		Center,
		Bottom,
		MAX,
	};
}

USTRUCT(BlueprintType)
struct FTextureUVs
{

	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TextureUVs)
	float U;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TextureUVs)
	float V;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TextureUVs)
	float UL;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TextureUVs)
	float VL;

	FTextureUVs() : U(0), V(0), UL(0), VL(0){};

	FTextureUVs(float inU, float inV, float inUL, float inVL)
		: U(inU)
		, V(inV)
		, UL(inUL)
		, VL(inVL)
	{};

};

