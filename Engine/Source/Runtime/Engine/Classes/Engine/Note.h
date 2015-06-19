// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * A sticky note.  Level designers can place these in the level and then
 * view them as a batch in the error/warnings window.
 */

#pragma once
#include "Note.generated.h"

UCLASS(MinimalAPI, hidecategories = (Input), showcategories=("Input|MouseInput", "Input|TouchInput"))
class ANote : public AActor
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Note)
	FString Text;

	// Reference to sprite visualization component
private_subobject:
	DEPRECATED_FORGAME(4.6, "SpriteComponent should not be accessed directly, please use GetSpriteComponent() function instead. SpriteComponent will soon be private and your code will not compile.")
	UPROPERTY()
	class UBillboardComponent* SpriteComponent;

	// Reference to arrow visualization component
	DEPRECATED_FORGAME(4.6, "ArrowComponent should not be accessed directly, please use GetArrowComponent() function instead. ArrowComponent will soon be private and your code will not compile.")
	UPROPERTY()
	class UArrowComponent* ArrowComponent;
public:

#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	// Begin AActor Interface
	virtual void CheckForErrors() override;
	// End AActor Interface
#endif

#if WITH_EDITORONLY_DATA
	/** Returns SpriteComponent subobject **/
	ENGINE_API class UBillboardComponent* GetSpriteComponent() const;
	/** Returns ArrowComponent subobject **/
	ENGINE_API class UArrowComponent* GetArrowComponent() const;
#endif
};



