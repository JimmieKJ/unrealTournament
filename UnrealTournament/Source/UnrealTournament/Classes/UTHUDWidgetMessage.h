// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once 

/**
 *
 * This is the basic hud messaging widget.  It's responsible for displaying UTLocalMessages on to the hud.  
 *
 **/

#include "UTHUDWidgetMessage.generated.h"

const int32 MESSAGE_QUEUE_LENGTH = 8;

USTRUCT()
struct FLocalizedMessageData
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

	// When will this message disappear
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	float EndOfLife;

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


	FLocalizedMessageData()
		: MessageClass(NULL)
		, MessageIndex(0)
		, EndOfLife(0)
		, DrawColor(ForceInit)
		, OptionalObject(NULL)
		, DisplayFont(NULL)
		, TextWidth(0)
		, TextHeight(0)
		, bHasBeenRendered(false)
		, MessageCount(0)
	{
	}

};

UCLASS()
class UUTHUDWidgetMessage : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:

	// Which type of Localized message will this Widget manage.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	FName ManagedMessageArea;

	// The Font that messages will be displayed in.  If you need more than a single font, then
	// you should create a special handler.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	UFont* MessageFont;

	// The base color for this message.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	FLinearColor MessageColor;

	virtual void Draw(float DeltaTime);

	// MessageWidgets need to be able to receive local messages from the HUD.  They will be responsible for managing their own messages.
	virtual void ReceiveLocalMessage(TSubclassOf<class UUTLocalMessage> MessageClass, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, uint32 MessageIndex, FText LocalMessageText, UObject* OptionalObject = NULL);

protected:

	// Since our depth is small, use a static array so that we 
	// don't have to shrink/grow it.
	FLocalizedMessageData MessageQueue[MESSAGE_QUEUE_LENGTH];

	virtual void ClearMessage(FLocalizedMessageData& Message);
	virtual void AddMessage(int32 QueueIndex, TSubclassOf<class UUTLocalMessage> MessageClass, uint32 MessageIndex, FText LocalMessageText, int32 MessageCount, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject);
	virtual void DrawMessage(int32 QueueIndex);

private:

};
