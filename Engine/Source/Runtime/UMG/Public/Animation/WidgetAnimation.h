// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieScenePlayer.h"
#include "WidgetAnimation.generated.h"

class UMovieScene;
class UWidgetTree;

/** A single object bound to a umg sequence */
USTRUCT()
struct FWidgetAnimationBinding
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName WidgetName;

	UPROPERTY()
	FName SlotWidgetName;

	UPROPERTY()
	FGuid AnimationGuid;

public:
	/**
	 * Locates a runtime object to animate from the provided tree of widgets
	 * @return the runtime object to animate or null if not found 
	 */
	UMG_API UObject* FindRuntimeObject(UWidgetTree& WidgetTree) const;

	bool operator==(const FWidgetAnimationBinding& Other) const
	{
		return WidgetName == Other.WidgetName && SlotWidgetName == Other.SlotWidgetName && AnimationGuid == Other.AnimationGuid;
	}

	friend FArchive& operator<<(FArchive& Ar, FWidgetAnimationBinding& Binding)
	{
		Ar << Binding.WidgetName;
		Ar << Binding.SlotWidgetName;
		Ar << Binding.AnimationGuid;
		return Ar;
	}
};


UCLASS(MinimalAPI)
class UWidgetAnimation : public UObject
{
	GENERATED_UCLASS_BODY()

public:

#if WITH_EDITOR
	static UMG_API UWidgetAnimation* GetNullAnimation();
#endif

	UFUNCTION(BlueprintCallable, Category="Animation")
	UMG_API float GetStartTime() const;

	UFUNCTION(BlueprintCallable, Category="Animation")
	UMG_API float GetEndTime() const;

public:
	UPROPERTY()
	UMovieScene* MovieScene;

	UPROPERTY()
	TArray<FWidgetAnimationBinding> AnimationBindings;
};
