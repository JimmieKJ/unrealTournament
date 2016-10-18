// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSequence.h"
#include "WidgetAnimationBinding.h"
#include "WidgetAnimation.generated.h"


class UMovieScene;
class UUserWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWidgetAnimationPlaybackStatusChanged);

UCLASS(BlueprintType, MinimalAPI)
class UWidgetAnimation
	: public UMovieSceneSequence
{
	GENERATED_UCLASS_BODY()

public:

#if WITH_EDITOR
	/**
	 * Get a placeholder animation.
	 *
	 * @return Placeholder animation.
	 */
	static UMG_API UWidgetAnimation* GetNullAnimation();
#endif

	/**
	 * Get the start time of this animation.
	 *
	 * @return Start time in seconds.
	 * @see GetEndTime
	 */
	UFUNCTION(BlueprintCallable, Category="Animation")
	UMG_API float GetStartTime() const;

	/**
	 * Get the end time of this animation.
	 *
	 * @return End time in seconds.
	 * @see GetStartTime
	 */
	UFUNCTION(BlueprintCallable, Category="Animation")
	UMG_API float GetEndTime() const;

	/** Fires when the widget animation starts playing. */
	UPROPERTY(BlueprintAssignable, Category="Animation")
	FOnWidgetAnimationPlaybackStatusChanged OnAnimationStarted;

	/** Fires when the widget animation is finished. */
	UPROPERTY(BlueprintAssignable, Category="Animation")
	FOnWidgetAnimationPlaybackStatusChanged OnAnimationFinished;

	/**
	 * Initialize the animation with a new user widget.
	 *
	 * @param InPreviewWidget The user widget to preview.
	 */
	UMG_API void Initialize(UUserWidget* InPreviewWidget);

public:

	// UMovieSceneAnimation overrides

	virtual void BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context) override;
	virtual bool CanPossessObject(UObject& Object) const override;
	virtual UObject* FindPossessableObject(const FGuid& ObjectId, UObject* Context) const override;
	virtual FGuid FindPossessableObjectId(UObject& Object) const override;
	virtual UMovieScene* GetMovieScene() const override;
	virtual UObject* GetParentObject(UObject* Object) const override;
	virtual void UnbindPossessableObjects(const FGuid& ObjectId) override;

	// UWidgetAnimation specific
	/**
	 * Replace the PossessedObject with a NewObject
	 * 
	 * @param OldId the Guid of the object is about to be replaced
	 * @param NewId the Guid of the new object
	 * @param OldObject the object is about to be replaced
	 * @param NewObject the new object to replace
	 */
	UMG_API void ReplacePossessableObject(const FGuid& OldId, const FGuid& NewId, UObject& OldObject, UObject& NewObject);

	/** Get Animation bindings of the animation */
	const TArray<FWidgetAnimationBinding>& GetBindings() const { return AnimationBindings; }
	/** Get the preview widget of the animation, if any */
	const TWeakObjectPtr<UUserWidget> GetPreviewWidget() const { return PreviewWidget; }
public:

	/** Pointer to the movie scene that controls this animation. */
	UPROPERTY()
	UMovieScene* MovieScene;

	UPROPERTY()
	TArray<FWidgetAnimationBinding> AnimationBindings;

private:

	/** The current preview widget, if any. */
	TWeakObjectPtr<UUserWidget> PreviewWidget;

	/** Mapping of preview objects to sequencer GUIDs */
	TMultiMap<FGuid, TWeakObjectPtr<UObject>> IdToPreviewObjects;
	TMultiMap<FGuid, TWeakObjectPtr<UObject>> IdToSlotContentPreviewObjects;
	TMap<TWeakObjectPtr<UObject>, FGuid> PreviewObjectToIds;
	TMap<TWeakObjectPtr<UObject>, FGuid> SlotContentPreviewObjectToIds;
};
