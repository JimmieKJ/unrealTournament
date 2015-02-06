// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FSkeletalMeshComponentDetails : public IDetailCustomization
{
public:
	FSkeletalMeshComponentDetails();
	~FSkeletalMeshComponentDetails();

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

private:
	void UpdateAnimationCategory( IDetailLayoutBuilder& DetailBuilder );

	/** Function that returns whether the specified animation mode should be visible */
	EVisibility VisibilityForAnimationMode(EAnimationMode::Type AnimationMode) const;

	/** Helper wrapper functions for VisibilityForAnimationMode */
	EVisibility VisibilityForBlueprintMode() const {return VisibilityForAnimationMode(EAnimationMode::AnimationBlueprint);}
	EVisibility VisibilityForSingleAnimMode() const {return VisibilityForAnimationMode(EAnimationMode::AnimationSingleNode);}
	EVisibility VisibilityForAnimPicker() const;

	/** Handler for filtering animation assets in the UI picker when asset mode is selected */
	bool OnShouldFilterAnimAsset(const FAssetData& AssetData);

	/** Delegate called when a skeletal mesh property is changed on a selected object */
	USkeletalMeshComponent::FOnSkeletalMeshPropertyChanged OnSkeletalMeshPropertyChanged;
	
	/** Register/Unregister the mesh changed delegate to TargetComponent */
	void RegisterSkeletalMeshPropertyChanged(TWeakObjectPtr<USkeletalMeshComponent> Mesh);
	void UnregisterSkeletalMeshPropertyChanged(TWeakObjectPtr<USkeletalMeshComponent> Mesh);
	void UnregisterAllMeshPropertyChangedCallers();

	/** Bound to the delegate used to detect changes in skeletal mesh properties */
	void SkeletalMeshPropertyChanged();

	/** Generates menu content for the class picker when it is clicked */
	TSharedRef<SWidget> GetClassPickerMenuContent();

	/** Gets the currently selected blueprint name to display on the class picker combo button */
	FText GetSelectedAnimBlueprintName() const;

	/** Callback from the class picker when the user selects a class */
	void OnClassPicked(UClass* PickedClass);

	/** Callback from the detail panel to browse to the selected anim asset */
	void OnBrowseToAnimBlueprint();

	/** Callback from the details panel to use the currently selected asset in the content browser */
	void UseSelectedAnimBlueprint();

	/** Cached layout builder for use after customization */
	IDetailLayoutBuilder* CurrentDetailBuilder;

	/** Cached selected objects to use when the skeletal mesh property changes */
	TArray<TWeakObjectPtr<UObject>> SelectedObjects;

	/** Cache of mesh components in the current selection */
	TArray<TWeakObjectPtr<USkeletalMeshComponent>> SelectedSkeletalMeshComponents;

	/** Caches the AnimationMode Handle so we can look up its value after customization has finished */
	TSharedPtr<IPropertyHandle> AnimationModeHandle;

	/** Caches the AnimationBlueprintGeneratedClass Handle so we can look up its value after customization has finished */
	TSharedPtr<IPropertyHandle> AnimationBlueprintHandle;

	/** Full name of the currently selected skeleton to use for filtering animation assets */
	FString SelectedSkeletonName;

	/** Current visibility of the animation asset picker in the details panel */
	EVisibility AnimPickerVisibility;

	/** The combo button for the class picker, Cached so we can close it when the user picks something */
	TSharedPtr<SComboButton> ClassPickerComboButton;

	/** Per-mesh handles to registered OnSkeletalMeshPropertyChanged delegates */
	TMap<USkeletalMeshComponent*, FDelegateHandle> OnSkeletalMeshPropertyChangedDelegateHandles;
};
