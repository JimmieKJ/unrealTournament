// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPathBuilderInterface.h"

#include "UTGenericObjectivePoint.generated.h"

/** a utility class to make it easier for maps (particularly DM maps) to support multiple gametypes
 * place these to indicate good places to locate game objectives, for example control points in Domination or scoring points in Greed
 */
UCLASS(Blueprintable)
class UNREALTOURNAMENT_API AUTGenericObjectivePoint : public AActor, public IUTPathBuilderInterface
{
	GENERATED_BODY()
public:
	AUTGenericObjectivePoint(const FObjectInitializer& OI)
		: Super(OI)
	{
		RootComponent = CreateDefaultSubobject<USceneComponent>(FName(TEXT("SceneRoot")));
#if WITH_EDITORONLY_DATA
		EditorSprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(FName(TEXT("EditorSprite")));
		if (EditorSprite != NULL)
		{
			EditorSprite->SetupAttachment(RootComponent);
			if (!IsRunningCommandlet())
			{
				ConstructorHelpers::FObjectFinderOptional<UTexture2D> SpriteObj(TEXT("/Game/RestrictedAssets/EditorAssets/Icons/generic_objective.generic_objective"));
				EditorSprite->Sprite = SpriteObj.Get();
				if (EditorSprite->Sprite != NULL)
				{
					EditorSprite->UL = EditorSprite->Sprite->GetSurfaceWidth();
					EditorSprite->VL = EditorSprite->Sprite->GetSurfaceHeight();
				}
			}
		}
#endif
		TeamIndex = 255;
	}

#if WITH_EDITORONLY_DATA
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	UBillboardComponent* EditorSprite;
#endif
	/** optional baked-in team association */
	UPROPERTY(EditAnywhere)
	uint8 TeamIndex;
	/** optional hint to gametype and mod authors of what this point is best used for */
	UPROPERTY(EditAnywhere)
	FName Purpose;
};