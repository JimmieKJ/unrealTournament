// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * To render text in the 3d world (quads with UV assignment to render text with material support).
 */

#pragma once

#include "TextRenderActor.generated.h"

UCLASS(MinimalAPI, ComponentWrapperClass, hideCategories = (Collision, Attachment, Actor))
class ATextRenderActor : public AActor
{
	GENERATED_UCLASS_BODY()

	friend class UActorFactoryTextRender;

private_subobject:
	/** Component to render a text in 3d with a font */
	DEPRECATED_FORGAME(4.6, "TextRender should not be accessed directly, please use GetTextRender() function instead. TextRender will soon be private and your code will not compile.")
	UPROPERTY(Category = TextRenderActor, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Rendering|Components|TextRender", AllowPrivateAccess = "true"))
	class UTextRenderComponent* TextRender;

#if WITH_EDITORONLY_DATA
	// Reference to the billboard component
	DEPRECATED_FORGAME(4.6, "SpriteComponent should not be accessed directly, please use GetSpriteComponent() function instead. SpriteComponent will soon be private and your code will not compile.")
	UPROPERTY()
	UBillboardComponent* SpriteComponent;
#endif

public:
	/** Returns TextRender subobject **/
	ENGINE_API class UTextRenderComponent* GetTextRender() const;
#if WITH_EDITORONLY_DATA
	/** Returns SpriteComponent subobject **/
	ENGINE_API UBillboardComponent* GetSpriteComponent() const;
#endif
};



