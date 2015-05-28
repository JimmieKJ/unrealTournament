// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomNodeBuilder.h"
/**
 * Struct to uniquely identify clothing applied to a material section
 * Contains index into the ClothingAssets array and the submesh index.
 */
struct FClothAssetSubmeshIndex
{
	FClothAssetSubmeshIndex(int32 InAssetIndex, int32 InSubmeshIndex)
		:	AssetIndex(InAssetIndex)
		,	SubmeshIndex(InSubmeshIndex)
	{}

	int32 AssetIndex;
	int32 SubmeshIndex;

	bool operator==(const FClothAssetSubmeshIndex& Other) const
	{
		return (AssetIndex	== Other.AssetIndex 
			&& SubmeshIndex	== Other.SubmeshIndex
			);
	}
};

struct FClothingComboInfo
{
	/* Per-material clothing combo boxes, array size must be same to # of sections */
	TArray<TSharedPtr< STextComboBox >>		ClothingComboBoxes;
	/* Clothing combo box strings */
	TArray<TSharedPtr<FString> >			ClothingComboStrings;
	/* Mapping from a combo box string to the asset and submesh it was generated from */
	TMap<FString, FClothAssetSubmeshIndex>	ClothingComboStringReverseLookup;
	/* The currently-selected index from each clothing combo box */
	TArray<int32>							ClothingComboSelectedIndices;
};


class FSkelMeshReductionSettingsLayout : public IDetailCustomNodeBuilder, public TSharedFromThis<FSkelMeshReductionSettingsLayout>
{
public:
	FSkelMeshReductionSettingsLayout(int32 InLODIndex, TSharedRef<class FPersonaMeshDetails> InParentLODSettings, TSharedPtr<IPropertyHandle> InBoneToRemoveProperty);
	virtual ~FSkelMeshReductionSettingsLayout();

	const FSkeletalMeshOptimizationSettings& GetSettings() const;
	void UpdateSettings(const FSkeletalMeshOptimizationSettings& InSettings);
private:
	/** IDetailCustomNodeBuilder Interface*/
	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override {}
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override;
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;
	virtual void Tick(float DeltaTime) override{}
	virtual bool RequiresTick() const override { return false; }
	virtual FName GetName() const override { static FName MeshReductionSettings("MeshReductionSettings"); return MeshReductionSettings; }
	virtual bool InitiallyCollapsed() const override { return true; }

	FReply OnApplyChanges();
	float GetPercentTriangles() const;
	float GetMaxDeviation() const;
	float GetWeldingThreshold() const;
	ECheckBoxState ShouldRecalculateNormals() const;
	float GetHardAngleThreshold() const;
	int32 GetMaxBonesPerVertex() const;

	void OnPercentTrianglesChanged(float NewValue);
	void OnMaxDeviationChanged(float NewValue);
	void OnReductionAmountChanged(float NewValue);
	void OnRecalculateNormalsChanged(ECheckBoxState NewValue);
	void OnWeldingThresholdChanged(float NewValue);
	void OnHardAngleThresholdChanged(float NewValue);
	void OnMaxBonesPerVertexChanged(int32 NewValue);

	void OnSilhouetteImportanceChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	void OnTextureImportanceChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	void OnShadingImportanceChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	void OnSkinningImportanceChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);

	void UpdateBonesToRemoveProperties(int32 LODIndex);
	void RefreshBonesToRemove();

private:
	int32 LODIndex;
	TWeakPtr<class FPersonaMeshDetails> ParentLODSettings;
	TSharedPtr<IPropertyHandle>	BoneToRemoveProperty;
	FSkeletalMeshOptimizationSettings ReductionSettings;

	TArray<TSharedPtr<FString> > ImportanceOptions;
	TArray<TSharedPtr<FString> > SimplificationOptions;
	TSharedPtr<STextComboBox> SilhouetteCombo;
	TSharedPtr<STextComboBox> TextureCombo;
	TSharedPtr<STextComboBox> ShadingCombo;
	TSharedPtr<STextComboBox> SkinningCombo;
};

class FPersonaMeshDetails : public IDetailCustomization
{
public:
	FPersonaMeshDetails(TSharedPtr<FPersona> InPersona) : PersonaPtr(InPersona) {}

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance(TSharedPtr<FPersona> InPersona);

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;

private:
	/**
	 * Called by the material list widget when we need to get new materials for the list
	 *
	 * @param OutMaterials	Handle to a material list builder that materials should be added to
	 */
	void OnGetMaterialsForView( class IMaterialListBuilder& OutMaterials, int32 LODIndex );

	/**
	 * Called when a user drags a new material over a list item to replace it
	 *
	 * @param NewMaterial	The material that should replace the existing material
	 * @param PrevMaterial	The material that should be replaced
	 * @param SlotIndex		The index of the slot on the component where materials should be replaces
	 * @param bReplaceAll	If true all materials in the slot should be replaced not just ones using PrevMaterial
	 */
	void OnMaterialChanged(UMaterialInterface* NewMaterial, UMaterialInterface* PrevMaterial, int32 SlotIndex, bool bReplaceAll, int32 LODIndex);

	
	/**
	 * Called by the material list widget on generating each name widget
	 *
	 * @param Material		The material that is being displayed
	 * @param SlotIndex		The index of the material slot
	 */
	TSharedRef<SWidget> OnGenerateCustomNameWidgetsForMaterial(UMaterialInterface* Material, int32 SlotIndex, int32 LODIndex);

	/**
	 * Called by the material list widget on generating each thumbnail widget
	 *
	 * @param Material		The material that is being displayed
	 * @param SlotIndex		The index of the material slot
	 */
	TSharedRef<SWidget> OnGenerateCustomMaterialWidgetsForMaterial(UMaterialInterface* Material, int32 SlotIndex, int32 LODIndex);

	/**
	 * Handler for check box display based on whether the material is highlighted
	 *
	 * @param SectionIndex	The material section that is being tested
	 */
	ECheckBoxState IsSectionSelected(int32 SectionIndex) const;

	/**
	 * Handler for changing highlight status on a material
	 *
	 * @param SectionIndex	The material section that is being tested
	 */
	void OnSectionSelectedChanged(ECheckBoxState NewState, int32 SectionIndex);

		/**
	 * Handler for check box display based on whether the material has shadow casting enabled
	 *
	 * @param MaterialIndex	The material index which a section in a specific LOD model has
	 */
	ECheckBoxState IsShadowCastingEnabled(int32 MaterialIndex) const;

	/**
	 * Handler for changing shadow casting status on a material
	 *
	 * @param MaterialIndex	The material index which a section in a specific LOD model has
	 */
	void OnShadowCastingChanged(ECheckBoxState NewState, int32 MaterialIndex);

	/**
	 * Handler for enabling delete button on materials
	 *
	 * @param SectionIndex - index of the section to check
	 */
	bool CanDeleteMaterialElement(int32 LODIndex, int32 SectionIndex) const;

	/**
	 * Handler for deleting material elements
	 * 
	 * @Param SectionIndex - material section to remove
	 */
	FReply OnDeleteButtonClicked(int32 LODIndex, int32 SectionIndex);

	/** Creates the UI for Current LOD panel */
	void AddLODLevelCategories(IDetailLayoutBuilder& DetailLayout);

	bool IsDuplicatedMaterialIndex(int32 LODIndex, int32 MaterialIndex);

	/** Get a material index from LOD index and section index */
	int32 GetMaterialIndex(int32 LODIndex, int32 SectionIndex);

	/** for LOD settings category */
	void CustomizeLODSettingsCategories(IDetailLayoutBuilder& DetailLayout);

	void OnImportLOD(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo, IDetailLayoutBuilder* DetailLayout);
	void UpdateLODNames();
	int32 GetLODCount() const;
	void OnLODCountChanged(int32 NewValue);
	void OnLODCountCommitted(int32 InValue, ETextCommit::Type CommitInfo);
	FText GetLODCountTooltip() const;
	/** apply LOD changes if the user modified LOD reduction settings */
	FReply OnApplyChanges();
	/** hide properties which don't need to be showed to end users */
	void HideUnnecessaryProperties(IDetailLayoutBuilder& DetailLayout);
	/** clear "None" bones and remove already included bones */
	void RefreshBonesToRemove(TArray<FBoneReference>& BonesToRemove, int32 LODIndex);

public:

	bool IsApplyNeeded() const;
	bool IsGenerateAvailable() const;
	void ApplyChanges(bool bForceUpdate);
	FText GetApplyButtonText() const;

	USkeletalMesh* GetMesh()
	{ 
		if (PersonaPtr.IsValid())
		{
			return PersonaPtr->GetMesh();
		}
		else
		{
			return NULL;
		}
	}
private:
	// Container for the objects to display
	TArray< TWeakObjectPtr<UObject>> SelectedObjects;

	// Pointer back to Persona
	TSharedPtr<FPersona> PersonaPtr;

	IDetailLayoutBuilder* MeshDetailLayout;

	/** LOD import options */
	TArray<TSharedPtr<FString> > LODNames;
	/** Helper value that corresponds to the 'Number of LODs' spinbox.*/
	int32 LODCount;

	/** Simplification options for each LOD level */
	TArray<TSharedPtr<FSkelMeshReductionSettingsLayout>> ReductionSettingsWidgets;

#if WITH_APEX_CLOTHING
private:

	// info about clothing combo boxes for multiple LOD
	TArray<FClothingComboInfo>				ClothingComboLODInfos;


	/* Clothing combo box functions */
	EVisibility IsClothingComboBoxVisible(int32 LODIndex, int32 SectionIndex) const;
	FString HandleSectionsComboBoxGetRowText(TSharedPtr<FString> Section, int32 LODIndex, int32 SectionIndex);
	void HandleSectionsComboBoxSelectionChanged(TSharedPtr<FString> SelectedItem, ESelectInfo::Type SelectInfo, int32 LODIndex, int32 SectionIndex );
	void UpdateComboBoxStrings();

	/* Generate slate UI for Clothing category */
	void CustomizeClothingProperties(class IDetailLayoutBuilder& DetailLayout, class IDetailCategoryBuilder& ClothingFilesCategory);

	/* Generate each ClothingAsset array entry */
	void OnGenerateElementForClothingAsset( TSharedRef<IPropertyHandle> ElementProperty, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder, IDetailLayoutBuilder* DetailLayout );

	/* Make uniform grid widget for Apex details */
	TSharedRef<SUniformGridPanel> MakeApexDetailsWidget(int32 AssetIndex) const;

	/* Opens dialog to add a new clothing asset */
	FReply OnOpenClothingFileClicked(IDetailLayoutBuilder* DetailLayout);

	/* Reimports a clothing asset */ 
	FReply OnReimportApexFileClicked(int32 AssetIndex, IDetailLayoutBuilder* DetailLayout);

	/* Removes a clothing asset */ 
	FReply OnRemoveApexFileClicked(int32 AssetIndex, IDetailLayoutBuilder* DetailLayout);


	/* if physics properties are changed, then save to the clothing asset */
	void UpdateClothPhysicsProperties(int32 AssetIndex);
#endif // #if WITH_APEX_CLOTHING

};
