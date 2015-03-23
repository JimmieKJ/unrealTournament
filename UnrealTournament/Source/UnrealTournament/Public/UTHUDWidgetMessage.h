// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once 

/**
 *
 * This is the basic hud messaging widget.  It's responsible for displaying UTLocalMessages on to the hud.  
 *
 **/

#include "UTHUDWidgetMessage.generated.h"

const int32 MESSAGE_QUEUE_LENGTH = 8;

USTRUCT(BlueprintType)
struct UNREALTOURNAMENT_API FLocalizedMessageData
{
	GENERATED_USTRUCT_BODY()

	// These members are static and set only upon construction

	// A cached reference to the class of this message.
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	TSubclassOf<UUTLocalMessage> MessageClass;

	// The index.
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	int32 MessageIndex;

	// The text of this message.  We build this once so we don't have to process the string each frame
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	FText Text;

	// How much time does this message have left
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	float LifeLeft;

	// How long total this message has in its life
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	float LifeSpan;

	// How long to scale in this message
	UPROPERTY(BlueprintReadOnly, Category = HUD)
		float ScaleInTime;

	// Starting scale of message
	UPROPERTY(BlueprintReadOnly, Category = HUD)
		float ScaleInSize;

	// The optional object for this class.  
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	UObject* OptionalObject;

	// DrawColor will get set to the base color upon creation.  You can manually apply any 
	// palette/alpha shifts safely during render.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	FLinearColor DrawColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	UFont* DisplayFont;

	// These members are setup at first render.
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	float TextWidth;
	
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	float TextHeight;

	UPROPERTY(BlueprintReadOnly, Category = HUD)
	bool bHasBeenRendered;

	// Count is tracked differently.  It's incremented when the same message arrives
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	int32 MessageCount;

	virtual bool ShouldDraw_Implementation(bool bShowScores)
	{
		return bShowScores;
	}

	FLocalizedMessageData()
		: MessageClass(NULL)
		, MessageIndex(0)
		, LifeLeft(0)
		, LifeSpan(0)
		, OptionalObject(NULL)
		, DrawColor(ForceInit)
		, DisplayFont(NULL)
		, TextWidth(0)
		, TextHeight(0)
		, bHasBeenRendered(false)
		, MessageCount(0)
	{
	}

};

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidgetMessage : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:

	// Which type of Localized message will this Widget manage.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	FName ManagedMessageArea;

	// The large Font that messages will be displayed in. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
		UFont* MessageFont;

	// The small Font that messages will be displayed in. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	UFont* SmallMessageFont;

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
	
	UPROPERTY()
		FVector2D ShadowDirection;

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

	virtual void PreDraw(float DeltaTime, AUTHUD* InUTHUDOwner, UCanvas* InCanvas, FVector2D InCanvasCenter);
	virtual void Draw_Implementation(float DeltaTime);

	/** Set when HUD fonts have been cached. */
	UPROPERTY(BlueprintReadOnly, Category = HUD)
		bool bFontsCached;

	/** Cache fonts this widget will use */
	virtual void CacheFonts();

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

protected:

	// Since our depth is small, use a static array so that we 
	// don't have to shrink/grow it.
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	TArray<FLocalizedMessageData> MessageQueue;

	virtual void ClearMessage(FLocalizedMessageData& Message);
	virtual void AddMessage(int32 QueueIndex, TSubclassOf<class UUTLocalMessage> MessageClass, uint32 MessageIndex, FText LocalMessageText, int32 MessageCount, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject);
	virtual void LayoutMessage(int32 QueueIndex, TSubclassOf<class UUTLocalMessage> MessageClass, uint32 MessageIndex, FText LocalMessageText, int32 MessageCount, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject);
	virtual void DrawMessage(int32 QueueIndex, float X, float Y);

	// returns the text scaling factor for a given message.  Exposed here to make extending
	// the widget easier.
	virtual float GetTextScale(int32 QueueIndex);

private:

};
