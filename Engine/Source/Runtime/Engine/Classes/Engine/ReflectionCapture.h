// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ReflectionCapture.generated.h"

UCLASS(abstract, hidecategories=(Collision, Attachment, Actor), MinimalAPI)
class AReflectionCapture : public AActor
{
	GENERATED_UCLASS_BODY()

private_subobject:
	/** Reflection capture component. */
	DEPRECATED_FORGAME(4.6, "CaptureComponent should not be accessed directly, please use GetCaptureComponent() function instead. CaptureComponent will soon be private and your code will not compile.")
	UPROPERTY(Category = DecalActor, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UReflectionCaptureComponent* CaptureComponent;

#if WITH_EDITORONLY_DATA
	DEPRECATED_FORGAME(4.6, "SpriteComponent should not be accessed directly, please use GetSpriteComponent() function instead. SpriteComponent will soon be private and your code will not compile.")
	UPROPERTY()
	UBillboardComponent* SpriteComponent;
#endif // WITH_EDITORONLY_DATA

public:	

	virtual bool IsLevelBoundsRelevant() const override { return false; }

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
#endif // WITH_EDITOR

	/** Returns CaptureComponent subobject **/
	ENGINE_API class UReflectionCaptureComponent* GetCaptureComponent() const;
#if WITH_EDITORONLY_DATA
	/** Returns SpriteComponent subobject **/
	ENGINE_API UBillboardComponent* GetSpriteComponent() const;
#endif
};



