// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTHUDWidgetMessage.h"
#include "UTHUDWidgetMessage_KillIconMessages.generated.h"

/**
 * 
 */
UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidgetMessage_KillIconMessages : public UUTHUDWidgetMessage
{
	GENERATED_UCLASS_BODY()
	
public:
	virtual bool ShouldDraw_Implementation(bool bShowScores) override
	{
		return (!UTHUDOwner->UTPlayerOwner || !UTHUDOwner->UTPlayerOwner->UTPlayerState
			|| !UTHUDOwner->UTPlayerOwner->UTPlayerState->bOnlySpectator || !UTHUDOwner->UTPlayerOwner->bShowCameraBinds);
	}

	/**Background for kills that dont involve the local player*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture Background;

	/**BG if the local player made a kill*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture LocalKillerBG;

	/**BG if the local player was killed*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture LocalVictimBG;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float MessageHeight;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float MessagePadding;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float IconHeight;

	/**The padding between the player names and the icon*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float ColumnPadding;

protected:
	virtual void DrawMessages(float DeltaTime);
	virtual void DrawMessage(int32 QueueIndex, float X, float Y);
	virtual void LayoutMessage(int32 QueueIndex, TSubclassOf<class UUTLocalMessage> MessageClass, uint32 MessageIndex, FText LocalMessageText, int32 MessageCount, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject) override;
	virtual float GetDrawScaleOverride() override;

	virtual int32 GetNumberOfMessages();
	virtual FLinearColor GetPlayerColor(AUTPlayerState* PS, bool bKiller);

	int32 CurrentIndex;
};
