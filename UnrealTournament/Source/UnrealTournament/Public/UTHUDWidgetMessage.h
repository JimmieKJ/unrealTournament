// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 

/**
 *
 * This is the basic hud messaging widget.  It's responsible for displaying UTLocalMessages on to the hud.  
 *
 **/
#include "UTAnnouncer.h"
#include "UTUMGHudWidget.h"
#include "UTATypes.h"
#include "UTHUDWidgetMessage.generated.h"

const int32 MESSAGE_QUEUE_LENGTH = 8;


UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidgetMessage : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:

	// Which type of Localized message will this Widget manage.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	FName ManagedMessageArea;

	// If true, this text will be drawn with an outline
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = HUD)
	uint32 bOutlinedText:1;

	// The outline color for this message.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = HUD)
	FLinearColor OutlineColor;

	// If true, this text will be drawn with an shadow
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = HUD)
	uint32 bShadowedText:1;

	// The shadow color for this message.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = HUD)
	FLinearColor ShadowColor;

	// The shadow direction with large fonts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = HUD)
	FVector2D LargeShadowDirection;

	// The shadow direction with small fonts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = HUD)
		FVector2D SmallShadowDirection;

	// direction messages scale in from
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = HUD)
	float ScaleInDirection;

	// How long should it take for a message to fade out.  Set to 0 and the message will just wink out of
	// existence.,  
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	float FadeTime;

	UPROPERTY()
		FText CombinedEmphasisText;

	UPROPERTY()
		int32 CombinedMessageIndex;

	virtual void PreDraw(float DeltaTime, AUTHUD* InUTHUDOwner, UCanvas* InCanvas, FVector2D InCanvasCenter);
	virtual void Draw_Implementation(float DeltaTime);
	virtual bool ShouldDraw_Implementation(bool bShowScores) override;


	// This function is called each frame and is used to age out messages in the queue. 
	UFUNCTION(BlueprintNativeEvent)
	void AgeMessages(float DeltaTime);

	/** Clear all messages from this widget */
	virtual void ClearMessages();

	virtual void DrawMessages(float DeltaTime);

	// immediately fade message which matches FadeMessageText
	virtual void FadeMessage(FText FadeMessageText);

	// MessageWidgets need to be able to receive local messages from the HUD.  They will be responsible for managing their own messages.
	virtual void ReceiveLocalMessage(TSubclassOf<class UUTLocalMessage> MessageClass, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, uint32 MessageIndex, FText LocalMessageText, UObject* OptionalObject = NULL);

	virtual void InitializeWidget(AUTHUD* Hud);
	virtual void DumpMessages();
	virtual float GetDrawScaleOverride() override;

protected:

	// Since our depth is small, use a static array so that we 
	// don't have to shrink/grow it.
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	TArray<FLocalizedMessageData> MessageQueue;

	/** Max number of messages to draw in this area. */
	UPROPERTY(BlueprintReadOnly, Category = HUD)
		int32 NumVisibleLines;

	virtual void ClearMessage(FLocalizedMessageData& Message);
	virtual void AddMessage(int32 QueueIndex, TSubclassOf<class UUTLocalMessage> MessageClass, uint32 MessageIndex, FText LocalMessageText, int32 MessageCount, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject);
	virtual void LayoutMessage(int32 QueueIndex, TSubclassOf<class UUTLocalMessage> MessageClass, uint32 MessageIndex, FText LocalMessageText, int32 MessageCount, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject);
	
	/** Returns the length of the drawn message. */
	virtual FVector2D DrawMessage(int32 QueueIndex, float X, float Y);

	// returns the text scaling factor for a given message.  Exposed here to make extending
	// the widget easier.
	virtual float GetTextScale(int32 QueueIndex);

	bool bDebugWidget;

	void DumpQueueIndex(int32 QueueIndex, FString Prefix);

private:

};
