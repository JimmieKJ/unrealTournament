// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 

/**
*
* This is the basic hud messaging widget.  It's responsible for displaying UTLocalMessages on to the hud.
*
**/
#include "UTHUDWidgetMessage.h"
#include "UTHUDWidgetAnnouncements.generated.h"

USTRUCT(BlueprintType)
struct UNREALTOURNAMENT_API FAnnouncementSlot
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadOnly, Category = HUD)
		FName MessageArea;

	UPROPERTY(BlueprintReadOnly, Category = HUD)
		bool bHasBeenRendered;

	UPROPERTY(BlueprintReadOnly, Category = HUD)
		float SlotYPosition;

	UPROPERTY(BlueprintReadOnly, Category = HUD)
		int32 CurrentMessageIndex;

	FAnnouncementSlot(FName InName = NAME_None, float InYPos = 0.f)
		: MessageArea(InName), bHasBeenRendered(false), SlotYPosition(InYPos), CurrentMessageIndex(-1)
	{};
};

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidgetAnnouncements : public UUTHUDWidgetMessage
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = HUD)
		TArray<FAnnouncementSlot> Slots;

	// The outline color for this message.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = HUD)
		FLinearColor EmphasisOutlineColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
		float EmphasisScaling;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
		float PaddingBetweenTextAndDamageIcon;

	virtual void DrawMessages(float DeltaTime) override;;
	virtual void AddMessage(int32 QueueIndex, TSubclassOf<class UUTLocalMessage> MessageClass, uint32 MessageIndex, FText LocalMessageText, int32 MessageCount, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject) override;
	virtual FVector2D DrawMessage(int32 QueueIndex, float X, float Y) override;
	virtual void DrawDeathMessage(FVector2D TextSize, int32 QueueIndex, float X, float Y);
};
