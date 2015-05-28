// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "AnimNotify.generated.h"

class UAnimSequence;
class UAnimSequenceBase;
class USkeletalMeshComponent;
struct FAnimNotifyEvent;

UCLASS(abstract, Blueprintable, const, hidecategories=Object, collapsecategories)
class ENGINE_API UAnimNotify : public UObject
{
	GENERATED_UCLASS_BODY()

	/** 
	 * Implementable event to get a custom name for the notify
	 */
	UFUNCTION(BlueprintNativeEvent)
	FString GetNotifyName() const;

	UFUNCTION(BlueprintImplementableEvent)
	bool Received_Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) const;

#if WITH_EDITORONLY_DATA
	/** Color of Notify in editor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AnimNotify)
	FColor NotifyColor;
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	virtual void OnAnimNotifyCreatedInEditor(FAnimNotifyEvent& ContainingAnimNotifyEvent) {};
#endif

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation);

	// @todo document 
	virtual FString GetEditorComment() 
	{ 
		return TEXT(""); 
	}

	// @todo document 
	virtual FLinearColor GetEditorColor() 
	{ 
#if WITH_EDITORONLY_DATA
		return FLinearColor(NotifyColor); 
#else
		return FLinearColor::Black;
#endif // WITH_EDITORONLY_DATA
	}

	/**
	 * We don't instance UAnimNotify objects along with the animations they belong to, but
	 * we still need a way to see which world this UAnimNotify is currently operating on.
	 * So this retrieves a contextual world pointer, from the triggering animation/mesh.  
	 * 
	 * @return NULL if this isn't in the middle of a Received_Notify(), otherwise it's the world belonging to the Mesh passed to Received_Notify()
	 */
	virtual class UWorld* GetWorld() const override;

	/** UObject Interface */
	virtual void PostLoad() override;
	/** End UObject Interface */

private:
	/* The mesh we're currently triggering a UAnimNotify for (so we can retrieve per instance information) */
	class USkeletalMeshComponent* MeshContext;
};



