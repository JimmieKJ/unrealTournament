// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PersonaMeshDetails.h"
#include "Widgets/Input/SCheckBox.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "Widgets/SToolTip.h"
#include "IDocumentation.h"
#include "Components/SkinnedMeshComponent.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "IPersonaPreviewScene.h"
#include "Animation/AnimBlueprint.h"
#include "SlateOptMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Editor/UnrealEdEngine.h"
#include "EditorDirectories.h"
#include "UnrealEdGlobals.h"
#include "IDetailsView.h"
#include "PropertyCustomizationHelpers.h"
#include "DesktopPlatformModule.h"
#include "Interfaces/IMainFrameModule.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "Animation/AnimBlueprintGeneratedClass.h"

#if WITH_APEX_CLOTHING
	#include "ApexClothingUtils.h"
	#include "ApexClothingOptionWindow.h"
#endif // #if WITH_APEX_CLOTHING

#include "LODUtilities.h"
#include "MeshUtilities.h"
#include "FbxMeshUtils.h"

#include "Widgets/Input/STextComboBox.h"

#include "Engine/SkeletalMeshReductionSettings.h"
#include "Animation/AnimInstance.h"
#include "IPersonaToolkit.h"
#include "Widgets/Input/SNumericEntryBox.h"

#define LOCTEXT_NAMESPACE "PersonaMeshDetails"

/** Returns true if automatic mesh reduction is available. */
static bool IsAutoMeshReductionAvailable()
{
	static bool bAutoMeshReductionAvailable = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities").GetSkeletalMeshReductionInterface() != NULL;
	return bAutoMeshReductionAvailable;
}

/** Returns true if Skincache and -shaders are enabled */
static bool IsSkinCacheAvailable()
{
	extern ENGINE_API int32 GEnableGPUSkinCacheShaders;
	static bool bSkinCacheAvailable = !!GEnableGPUSkinCacheShaders;
	return bSkinCacheAvailable;
}

/**
* FMeshReductionSettings
*/

static UEnum& GetFeatureImportanceEnum()
{
	static FName FeatureImportanceName(TEXT("EMeshFeatureImportance::Off"));
	static UEnum* FeatureImportanceEnum = NULL;
	if (FeatureImportanceEnum == NULL)
	{
		UEnum::LookupEnumName(FeatureImportanceName, &FeatureImportanceEnum);
		check(FeatureImportanceEnum);
	}
	return *FeatureImportanceEnum;
}

static UEnum& GetFeatureSimpificationEnum()
{
	static FName FeatureSimpificationName(TEXT("SMOT_NumOfTriangles"));
	static UEnum* FeatureSimpificationEnum = NULL;
	if (FeatureSimpificationEnum == NULL)
	{
		UEnum::LookupEnumName(FeatureSimpificationName, &FeatureSimpificationEnum);
		check(FeatureSimpificationEnum);
	}
	return *FeatureSimpificationEnum;
}

static void FillEnumOptions(TArray<TSharedPtr<FString> >& OutStrings, UEnum& InEnum)
{
	for (int32 EnumIndex = 0; EnumIndex < InEnum.NumEnums() - 1; ++EnumIndex)
	{
		OutStrings.Add(MakeShareable(new FString(InEnum.GetEnumName(EnumIndex))));
	}
}

// Container widget for LOD buttons
class SSkeletalLODActions : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSkeletalLODActions)
		: _LODIndex(INDEX_NONE)
		, _PersonaToolkit(nullptr)
	{}
	SLATE_ARGUMENT(int32, LODIndex)
	SLATE_ARGUMENT(TWeakPtr<IPersonaToolkit>, PersonaToolkit)
	SLATE_EVENT(FOnClicked, OnRemoveLODClicked)
	SLATE_EVENT(FOnClicked, OnReimportClicked)
	SLATE_EVENT(FOnClicked, OnReimportNewFileClicked)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:

	EActiveTimerReturnType RefreshExistFlag(double InCurrentTime, float InDeltaSeconds)
	{
		bDoesSourceFileExist_Cached = false;

		TSharedPtr<IPersonaToolkit> SharedToolkit = PersonaToolkit.Pin();
		if(SharedToolkit.IsValid())
		{
			USkeletalMesh* SkelMesh = SharedToolkit->GetMesh();

			if(!SkelMesh)
			{
				return EActiveTimerReturnType::Continue;
			}

			if(SkelMesh->LODInfo.IsValidIndex(LODIndex))
			{
				FSkeletalMeshLODInfo& LODInfo = SkelMesh->LODInfo[LODIndex];

				bDoesSourceFileExist_Cached = !LODInfo.SourceImportFilename.IsEmpty() && FPaths::FileExists(LODInfo.SourceImportFilename);
			}
		}
		return EActiveTimerReturnType::Continue;
	}

	FText GetReimportButtonToolTipText() const
	{
		TSharedPtr<IPersonaToolkit> SharedToolkit = PersonaToolkit.Pin();

		if(!CanReimportFromSource() || !SharedToolkit.IsValid())
		{
			return LOCTEXT("ReimportButton_NewFile_NoSource_ToolTip", "No source file available for reimport");
		}

		USkeletalMesh* SkelMesh = SharedToolkit->GetMesh();
		check(SkelMesh);
		check(SkelMesh->LODInfo.IsValidIndex(LODIndex)); // Should be true for the button to exist

		FSkeletalMeshLODInfo& LODInfo = SkelMesh->LODInfo[LODIndex];

		FString Filename = FPaths::GetCleanFilename(LODInfo.SourceImportFilename);

		return FText::Format(LOCTEXT("ReimportButton_NewFile_ToolTip", "Reimport LOD{0} using the current source file ({1})"), FText::AsNumber(LODIndex), FText::FromString(Filename));
	}

	FText GetReimportButtonNewFileToolTipText() const
	{
		return FText::Format(LOCTEXT("ReimportButton_ToolTip", "Choose a new file to reimport over this LOD (LOD{0})"), FText::AsNumber(LODIndex));
	}

	bool CanReimportFromSource() const
	{
		return bDoesSourceFileExist_Cached;
	}

	// Incoming arg data
	int32 LODIndex;
	TWeakPtr<IPersonaToolkit> PersonaToolkit;
	FOnClicked OnRemoveLODClicked;
	FOnClicked OnReimportClicked;
	FOnClicked OnReimportNewFileClicked;

	// Cached exists flag so we don't constantly hit the file system
	bool bDoesSourceFileExist_Cached;
};

void SSkeletalLODActions::Construct(const FArguments& InArgs)
{
	LODIndex = InArgs._LODIndex;
	PersonaToolkit = InArgs._PersonaToolkit;
	OnRemoveLODClicked = InArgs._OnRemoveLODClicked;
	OnReimportClicked = InArgs._OnReimportClicked;
	OnReimportNewFileClicked = InArgs._OnReimportNewFileClicked;

	this->ChildSlot

		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(OnRemoveLODClicked)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("RemoveLOD", "Remove this LOD"))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ToolTipText(this, &SSkeletalLODActions::GetReimportButtonToolTipText)
				.IsEnabled(this, &SSkeletalLODActions::CanReimportFromSource)
				.OnClicked(OnReimportClicked)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ReimportLOD", "Reimport"))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ToolTipText(this, &SSkeletalLODActions::GetReimportButtonNewFileToolTipText)
				.OnClicked(OnReimportNewFileClicked)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ReimportLOD_NewFile", "Reimport (New File)"))
				]
			]
		];

	// Register timer to refresh out exists flag periodically, with a bit added per LOD so we're not doing everything on the same frame
	const float LODTimeOffset = 1.0f / 30.0f;
	RegisterActiveTimer(1.0f + LODTimeOffset * LODIndex, FWidgetActiveTimerDelegate::CreateSP(this, &SSkeletalLODActions::RefreshExistFlag));
}

FSkelMeshReductionSettingsLayout::FSkelMeshReductionSettingsLayout(int32 InLODIndex, TSharedRef<FPersonaMeshDetails> InParentLODSettings, TSharedPtr<IPropertyHandle> InBoneToRemoveProperty)
: LODIndex(InLODIndex)
, ParentLODSettings(InParentLODSettings)
, BoneToRemoveProperty(InBoneToRemoveProperty)
{
	FillEnumOptions(SimplificationOptions, GetFeatureSimpificationEnum());
	FillEnumOptions(ImportanceOptions, GetFeatureImportanceEnum());
}

FSkelMeshReductionSettingsLayout::~FSkelMeshReductionSettingsLayout()
{
}

void FSkelMeshReductionSettingsLayout::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{
	NodeRow.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MeshReductionSettings", "Reduction Settings"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];
}


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FSkelMeshReductionSettingsLayout::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	{
		ChildrenBuilder.AddChildContent(LOCTEXT("PercentTriangles", "Percent Triangles"))
			.NameContent()
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(LOCTEXT("PercentTriangles", "Percent Triangles"))
			]
		.ValueContent()
			[
				SNew(SSpinBox<float>)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.MinValue(0.0f)
				.MaxValue(100.0f)
				.Value(this, &FSkelMeshReductionSettingsLayout::GetPercentTriangles)
				.OnValueChanged(this, &FSkelMeshReductionSettingsLayout::OnPercentTrianglesChanged)
			];

	}

	{
		ChildrenBuilder.AddChildContent(LOCTEXT("MaxDeviation", "Max Deviation"))
			.NameContent()
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(LOCTEXT("MaxDeviation", "Max Deviation"))
			]
		.ValueContent()
			[
				SNew(SSpinBox<float>)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.MinValue(0.0f)
				.MaxValue(100.0f)
				.Value(this, &FSkelMeshReductionSettingsLayout::GetMaxDeviation)
				.OnValueChanged(this, &FSkelMeshReductionSettingsLayout::OnMaxDeviationChanged)
			];

	}

	{
		ChildrenBuilder.AddChildContent(LOCTEXT("Silhouette_MeshSimplification", "Silhouette"))
			.NameContent()
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(LOCTEXT("Silhouette_MeshSimplification", "Silhouette"))
			]
		.ValueContent()
			[
				SAssignNew(SilhouetteCombo, STextComboBox)
				//.Font( IDetailLayoutBuilder::GetDetailFont() )
				.ContentPadding(0)
				.OptionsSource(&ImportanceOptions)
				.InitiallySelectedItem(ImportanceOptions[ReductionSettings.SilhouetteImportance])
				.OnSelectionChanged(this, &FSkelMeshReductionSettingsLayout::OnSilhouetteImportanceChanged)
			];

	}

	{
		ChildrenBuilder.AddChildContent(LOCTEXT("Texture_MeshSimplification", "Texture"))
			.NameContent()
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(LOCTEXT("Texture_MeshSimplification", "Texture"))
			]
		.ValueContent()
			[
				SAssignNew(TextureCombo, STextComboBox)
				//.Font( IDetailLayoutBuilder::GetDetailFont() )
				.ContentPadding(0)
				.OptionsSource(&ImportanceOptions)
				.InitiallySelectedItem(ImportanceOptions[ReductionSettings.TextureImportance])
				.OnSelectionChanged(this, &FSkelMeshReductionSettingsLayout::OnTextureImportanceChanged)
			];

	}

	{
		ChildrenBuilder.AddChildContent(LOCTEXT("Shading_MeshSimplification", "Shading"))
			.NameContent()
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(LOCTEXT("Shading_MeshSimplification", "Shading"))
			]
		.ValueContent()
			[
				SAssignNew(ShadingCombo, STextComboBox)
				//.Font( IDetailLayoutBuilder::GetDetailFont() )
				.ContentPadding(0)
				.OptionsSource(&ImportanceOptions)
				.InitiallySelectedItem(ImportanceOptions[ReductionSettings.ShadingImportance])
				.OnSelectionChanged(this, &FSkelMeshReductionSettingsLayout::OnShadingImportanceChanged)
			];

	}

	{
		ChildrenBuilder.AddChildContent(LOCTEXT("Skinning_MeshSimplification", "Skinning"))
			.NameContent()
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(LOCTEXT("Skinning_MeshSimplification", "Skinning"))
			]
		.ValueContent()
			[
				SAssignNew(SkinningCombo, STextComboBox)
				//.Font( IDetailLayoutBuilder::GetDetailFont() )
				.ContentPadding(0)
				.OptionsSource(&ImportanceOptions)
				.InitiallySelectedItem(ImportanceOptions[ReductionSettings.SkinningImportance])
				.OnSelectionChanged(this, &FSkelMeshReductionSettingsLayout::OnSkinningImportanceChanged)
			];

	}

	{
		ChildrenBuilder.AddChildContent(LOCTEXT("WeldingThreshold", "Welding Threshold"))
			.NameContent()
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(LOCTEXT("WeldingThreshold", "Welding Threshold"))
			]
		.ValueContent()
			[
				SNew(SSpinBox<float>)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.MinValue(0.0f)
				.MaxValue(10.0f)
				.Value(this, &FSkelMeshReductionSettingsLayout::GetWeldingThreshold)
				.OnValueChanged(this, &FSkelMeshReductionSettingsLayout::OnWeldingThresholdChanged)
			];

	}

	{
		ChildrenBuilder.AddChildContent(LOCTEXT("RecomputeNormals", "Recompute Normals"))
			.NameContent()
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(LOCTEXT("RecomputeNormals", "Recompute Normals"))

			]
		.ValueContent()
			[
				SNew(SCheckBox)
				.IsChecked(this, &FSkelMeshReductionSettingsLayout::ShouldRecomputeTangents)
				.OnCheckStateChanged(this, &FSkelMeshReductionSettingsLayout::OnRecomputeTangentsChanged)
			];
	}

	{
		ChildrenBuilder.AddChildContent(LOCTEXT("HardEdgeAngle", "Hard Edge Angle"))
			.NameContent()
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(LOCTEXT("HardEdgeAngle", "Hard Edge Angle"))
			]
		.ValueContent()
			[
				SNew(SSpinBox<float>)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.MinValue(0.0f)
				.MaxValue(180.0f)
				.Value(this, &FSkelMeshReductionSettingsLayout::GetHardAngleThreshold)
				.OnValueChanged(this, &FSkelMeshReductionSettingsLayout::OnHardAngleThresholdChanged)
			];

	}

	{
		if (BoneToRemoveProperty.IsValid() && BoneToRemoveProperty->IsValidHandle())
		{
			ChildrenBuilder.AddChildProperty(BoneToRemoveProperty.ToSharedRef());
		}
	}

	{
		ChildrenBuilder.AddChildContent(LOCTEXT("MaxBonesPerVertex", "Max Bones Per Vertex"))
			.NameContent()
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(LOCTEXT("MaxBonesPerVertex", "Max Bones Per Vertex"))
			]
		.ValueContent()
			[
				SNew(SSpinBox<int32>)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.MinValue(1)
				.MaxValue(MAX_TOTAL_INFLUENCES)
				.Value(this, &FSkelMeshReductionSettingsLayout::GetMaxBonesPerVertex)
				.OnValueChanged(this, &FSkelMeshReductionSettingsLayout::OnMaxBonesPerVertexChanged)
			];

	}

	{
		ChildrenBuilder.AddChildContent(LOCTEXT("BaseLOD", "Base LOD"))
			.NameContent()
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(LOCTEXT("BaseLODTitle", "Base LOD"))
			]
		.ValueContent()
			[
				SNew(SSpinBox<int32>)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.MinValue(0)
				.Value(this, &FSkelMeshReductionSettingsLayout::GetBaseLOD)
				.OnValueChanged(this, &FSkelMeshReductionSettingsLayout::OnBaseLODChanged)
			];

	}

	{
		ChildrenBuilder.AddChildContent(LOCTEXT("ApplyChangeToLOD", "Apply Change to LOD"))
			.ValueContent()
			.HAlign(HAlign_Left)
			[
				SNew(SButton)
				.OnClicked(this, &FSkelMeshReductionSettingsLayout::OnApplyChanges)
				.IsEnabled(ParentLODSettings.Pin().ToSharedRef(), &FPersonaMeshDetails::IsApplyNeeded)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ApplyChangeToLOD", "Apply Change to LOD"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			];
	}

	SilhouetteCombo->SetSelectedItem(ImportanceOptions[ReductionSettings.SilhouetteImportance]);
	TextureCombo->SetSelectedItem(ImportanceOptions[ReductionSettings.TextureImportance]);
	ShadingCombo->SetSelectedItem(ImportanceOptions[ReductionSettings.ShadingImportance]);
	SkinningCombo->SetSelectedItem(ImportanceOptions[ReductionSettings.SkinningImportance]);
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

const FSkeletalMeshOptimizationSettings& FSkelMeshReductionSettingsLayout::GetSettings() const
{
	return ReductionSettings;
}

void FSkelMeshReductionSettingsLayout::UpdateSettings(const FSkeletalMeshOptimizationSettings& InSettings)
{
	ReductionSettings = InSettings;
}

FReply FSkelMeshReductionSettingsLayout::OnApplyChanges()
{
	if (ParentLODSettings.IsValid())
	{
		ParentLODSettings.Pin()->ApplyChanges(LODIndex, ReductionSettings);
	}
	return FReply::Handled();
}

float FSkelMeshReductionSettingsLayout::GetPercentTriangles() const
{
	return ReductionSettings.NumOfTrianglesPercentage * 100.0f; // Display fraction as percentage.
}

float FSkelMeshReductionSettingsLayout::GetMaxDeviation() const
{
	return ReductionSettings.MaxDeviationPercentage * 2000.0f;
}

float FSkelMeshReductionSettingsLayout::GetWeldingThreshold() const
{
	return ReductionSettings.WeldingThreshold;
}

ECheckBoxState FSkelMeshReductionSettingsLayout::ShouldRecomputeTangents() const
{
	return ReductionSettings.bRecalcNormals ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

float FSkelMeshReductionSettingsLayout::GetHardAngleThreshold() const
{
	return ReductionSettings.NormalsThreshold;
}

int32 FSkelMeshReductionSettingsLayout::GetMaxBonesPerVertex() const
{
	return ReductionSettings.MaxBonesPerVertex;
}

int32 FSkelMeshReductionSettingsLayout::GetBaseLOD() const
{
	return ReductionSettings.BaseLOD;
}

void FSkelMeshReductionSettingsLayout::OnPercentTrianglesChanged(float NewValue)
{
	// Percentage -> fraction.
	ReductionSettings.NumOfTrianglesPercentage = NewValue * 0.01f;
}

void FSkelMeshReductionSettingsLayout::OnMaxDeviationChanged(float NewValue)
{
	ReductionSettings.MaxDeviationPercentage = NewValue / 2000.0f;
}

void FSkelMeshReductionSettingsLayout::OnWeldingThresholdChanged(float NewValue)
{
	ReductionSettings.WeldingThreshold = NewValue;
}

void FSkelMeshReductionSettingsLayout::OnRecomputeTangentsChanged(ECheckBoxState NewValue)
{
	ReductionSettings.bRecalcNormals = NewValue == ECheckBoxState::Checked;
}

void FSkelMeshReductionSettingsLayout::OnHardAngleThresholdChanged(float NewValue)
{
	ReductionSettings.NormalsThreshold = NewValue;
}

void FSkelMeshReductionSettingsLayout::OnMaxBonesPerVertexChanged(int32 NewValue)
{
	ReductionSettings.MaxBonesPerVertex = NewValue;
}

void FSkelMeshReductionSettingsLayout::OnBaseLODChanged(int32 NewLOD)
{
	ReductionSettings.BaseLOD = NewLOD;
}

void FSkelMeshReductionSettingsLayout::OnSilhouetteImportanceChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	ReductionSettings.SilhouetteImportance = (SkeletalMeshOptimizationImportance)ImportanceOptions.Find(NewValue);
}

void FSkelMeshReductionSettingsLayout::OnTextureImportanceChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	ReductionSettings.TextureImportance = (SkeletalMeshOptimizationImportance)ImportanceOptions.Find(NewValue);
}

void FSkelMeshReductionSettingsLayout::OnShadingImportanceChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	ReductionSettings.ShadingImportance = (SkeletalMeshOptimizationImportance)ImportanceOptions.Find(NewValue);
}

void FSkelMeshReductionSettingsLayout::OnSkinningImportanceChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	ReductionSettings.SkinningImportance = (SkeletalMeshOptimizationImportance)ImportanceOptions.Find(NewValue);
}

/**
* FPersonaMeshDetails
*/
TSharedRef<IDetailCustomization> FPersonaMeshDetails::MakeInstance(TSharedRef<class IPersonaToolkit> InPersonaToolkit)
{
	return MakeShareable( new FPersonaMeshDetails(InPersonaToolkit) );
}

void FPersonaMeshDetails::AddLODLevelCategories(IDetailLayoutBuilder& DetailLayout)
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();

	if (SkelMesh)
	{
		const int32 SkelMeshLODCount = SkelMesh->LODInfo.Num();

		if (ReductionSettingsWidgets.Num() < SkelMeshLODCount)
		{
			ReductionSettingsWidgets.AddZeroed(SkelMeshLODCount - ReductionSettingsWidgets.Num());
		}

		//Create material list panel to let users control the materials array
		{
			FString MaterialCategoryName = FString(TEXT("Materials"));
			IDetailCategoryBuilder& MaterialCategory = DetailLayout.EditCategory(*MaterialCategoryName, FText::GetEmpty(), ECategoryPriority::Important);
			MaterialCategory.AddCustomRow(LOCTEXT("AddLODLevelCategories_MaterialArrayOperationAdd", "Materials Operation Add Material Slot"))
				.NameContent()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("AddLODLevelCategories_MaterialArrayOperations", "Material Slots"))
				]
				.ValueContent()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Font(IDetailLayoutBuilder::GetDetailFont())
							.Text(this, &FPersonaMeshDetails::GetMaterialArrayText)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.Padding(2.0f, 1.0f)
						[
							SNew(SButton)
							.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
							.Text(LOCTEXT("AddLODLevelCategories_MaterialArrayOpAdd", "Add Material Slot"))
							.ToolTipText(LOCTEXT("AddLODLevelCategories_MaterialArrayOpAdd_Tooltip", "Add Material Slot at the end of the Material slot array. Those Material slots can be used to override a LODs section, (not the base LOD)"))
							.ContentPadding(4.0f)
							.ForegroundColor(FSlateColor::UseForeground())
							.OnClicked(this, &FPersonaMeshDetails::AddMaterialSlot)
							.IsEnabled(true)
							.IsFocusable(false)
							[
								SNew(SImage)
								.Image(FEditorStyle::GetBrush("PropertyWindow.Button_AddToArray"))
								.ColorAndOpacity(FSlateColor::UseForeground())
							]
						]
					]
				];
			{
				FMaterialListDelegates MaterialListDelegates;

				MaterialListDelegates.OnGetMaterials.BindSP(this, &FPersonaMeshDetails::OnGetMaterialsForArray, 0);
				MaterialListDelegates.OnMaterialChanged.BindSP(this, &FPersonaMeshDetails::OnMaterialArrayChanged, 0);
				MaterialListDelegates.OnGenerateCustomMaterialWidgets.BindSP(this, &FPersonaMeshDetails::OnGenerateCustomMaterialWidgetsForMaterialArray, 0);
				MaterialListDelegates.OnMaterialListDirty.BindSP(this, &FPersonaMeshDetails::OnMaterialListDirty);
				MaterialCategory.AddCustomBuilder(MakeShareable(new FMaterialList(MaterialCategory.GetParentLayout(), MaterialListDelegates, false)));
			}
		}

		// Create information panel for each LOD level.
		for (int32 LODIndex = 0; LODIndex < SkelMeshLODCount; ++LODIndex)
		{
			if (LODIndex > 0 && IsAutoMeshReductionAvailable())
			{
				TSharedRef<IPropertyHandle> LODInfoProperty = DetailLayout.GetProperty(FName("LODInfo"), USkeletalMesh::StaticClass());
				uint32 NumChildren = 0;
				LODInfoProperty->GetNumChildren(NumChildren);

				check(NumChildren > (uint32)LODIndex);
	
				TSharedPtr<IPropertyHandle> ChildHandle = LODInfoProperty->GetChildHandle(LODIndex);
				check(ChildHandle.IsValid());
				TSharedPtr<IPropertyHandle> ReductionHandle = ChildHandle->GetChildHandle(FName("ReductionSettings"));
				check(ReductionHandle.IsValid());
				TSharedPtr<IPropertyHandle> BonesToRemoveHandle = ReductionHandle->GetChildHandle(FName("BonesToRemove"));
				ReductionSettingsWidgets[LODIndex] = MakeShareable(new FSkelMeshReductionSettingsLayout(LODIndex, SharedThis(this), BonesToRemoveHandle));
			}

			FSkeletalMeshLODInfo& LODInfo = SkelMesh->LODInfo[LODIndex];
			if (ReductionSettingsWidgets[LODIndex].IsValid())
			{
				ReductionSettingsWidgets[LODIndex]->UpdateSettings(LODInfo.ReductionSettings);
			}

			FString CategoryName = FString(TEXT("LOD"));
			CategoryName.AppendInt(LODIndex);

			FText LODLevelString = FText::Format(LOCTEXT("LODLevel", "LOD{0}"), FText::AsNumber(LODIndex));

			IDetailCategoryBuilder& LODCategory = DetailLayout.EditCategory(*CategoryName, LODLevelString, ECategoryPriority::Important);
			TSharedRef<SWidget> LODCategoryWidget =

				SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				[
					SNew(STextBlock)
					.Text_Raw(this, &FPersonaMeshDetails::GetLODImportedText, LODIndex)
					.Font(IDetailLayoutBuilder::GetDetailFontItalic())
				];

			// want to make sure if this data has imporetd or not
			LODCategory.HeaderContent(LODCategoryWidget);

			{
				FSectionListDelegates SectionListDelegates;

				SectionListDelegates.OnGetSections.BindSP(this, &FPersonaMeshDetails::OnGetSectionsForView, LODIndex);
				SectionListDelegates.OnSectionChanged.BindSP(this, &FPersonaMeshDetails::OnSectionChanged);
				SectionListDelegates.OnGenerateCustomNameWidgets.BindSP(this, &FPersonaMeshDetails::OnGenerateCustomNameWidgetsForSection);
				SectionListDelegates.OnGenerateCustomSectionWidgets.BindSP(this, &FPersonaMeshDetails::OnGenerateCustomSectionWidgetsForSection);
				LODCategory.AddCustomBuilder(MakeShareable(new FSectionList(LODCategory.GetParentLayout(), SectionListDelegates, (LODIndex > 0))));
			}
			// add each LOD Info to each LOD category
			TSharedRef<IPropertyHandle> LODInfoProperty = DetailLayout.GetProperty(FName("LODInfo"), USkeletalMesh::StaticClass());

			if (LODInfoProperty->IsValidHandle())
			{
				uint32 NumChildren;
				LODInfoProperty->GetNumChildren(NumChildren);
				check((uint32)LODIndex < NumChildren);

				// changing property name to "LOD Info" because it shows only array index
				TSharedPtr<IPropertyHandle> LODInfoChild = LODInfoProperty->GetChildHandle(LODIndex);
				LODInfoChild->CreatePropertyNameWidget(LOCTEXT("LODInfoLabel", "LOD Info"));
				LODCategory.AddProperty(LODInfoChild);

				//@Todo : ideally this should be inside of LODinfo customization, but for now this will allow users to re-apply removed joints after re-import if they want to.
				// this also can be buggy if you have this opened and you removed joints using skeleton tree, in that case, it might not show
				if (SkelMesh->LODInfo[LODIndex].RemovedBones.Num() > 0)
				{
					// add custom button to re-apply bone reduction if they want to
					LODCategory.AddCustomRow(LOCTEXT("ReapplyRemovedBones", "Reapply Removed Bones"))
					.ValueContent()
					.HAlign(HAlign_Left)
					[
						SNew(SButton)
						.OnClicked(this, &FPersonaMeshDetails::RemoveBones, LODIndex)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("ReapplyRemovedBonesButton", "Reapply removed bones"))
							.Font(DetailLayout.GetDetailFont())
						]
					];
				}
			}

			if (ReductionSettingsWidgets[LODIndex].IsValid())
			{
				LODCategory.AddCustomBuilder(ReductionSettingsWidgets[LODIndex].ToSharedRef());
			}

			if (LODIndex > 0)
			{
				LODCategory.AddCustomRow(LOCTEXT("RemoveLODRow", "Remove LOD"))
				.ValueContent()
				.HAlign(HAlign_Fill)
				[
					SNew(SSkeletalLODActions)
					.LODIndex(LODIndex)
					.PersonaToolkit(GetPersonaToolkit())
					.OnRemoveLODClicked(this, &FPersonaMeshDetails::RemoveOneLOD, LODIndex)
					.OnReimportClicked(this, &FPersonaMeshDetails::OnReimportLodClicked, &DetailLayout, EReimportButtonType::Reimport, LODIndex)
					.OnReimportNewFileClicked(this, &FPersonaMeshDetails::OnReimportLodClicked, &DetailLayout, EReimportButtonType::ReimportWithNewFile, LODIndex)
				];
			}
		}
	}
}

void FPersonaMeshDetails::CustomizeLODSettingsCategories(IDetailLayoutBuilder& DetailLayout)
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();
	LODCount = SkelMesh->LODInfo.Num();

	UpdateLODNames();

	IDetailCategoryBuilder& LODSettingsCategory = DetailLayout.EditCategory("LodSettings", LOCTEXT("LodSettingsCategory", "LOD Settings"), ECategoryPriority::TypeSpecific);

	TSharedPtr<SWidget> LodTextPtr;

	LODSettingsCategory.AddCustomRow(LOCTEXT("LODImport", "LOD Import"))
		.NameContent()
		[
			SAssignNew(LodTextPtr, STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("LODImport", "LOD Import"))
		]
	.ValueContent()
		[
			SNew(STextComboBox)
			.ContentPadding(0)
			.OptionsSource(&LODNames)
			.InitiallySelectedItem(LODNames[0])
			.OnSelectionChanged(this, &FPersonaMeshDetails::OnImportLOD, &DetailLayout)
		];

	// Add Number of LODs slider.
	const int32 MinAllowedLOD = 1;
	LODSettingsCategory.AddCustomRow(LOCTEXT("NumberOfLODs", "Number of LODs"))
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("NumberOfLODs", "Number of LODs"))
		]
	.ValueContent()
		[
			SNew(SSpinBox<int32>)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Value(this, &FPersonaMeshDetails::GetLODCount)
			.OnValueChanged(this, &FPersonaMeshDetails::OnLODCountChanged)
			.OnValueCommitted(this, &FPersonaMeshDetails::OnLODCountCommitted)
			.MinValue(MinAllowedLOD)
			.MaxValue(MAX_SKELETAL_MESH_LODS)
			.ToolTipText(this, &FPersonaMeshDetails::GetLODCountTooltip)
			.IsEnabled(IsAutoMeshReductionAvailable())
		];

	LODSettingsCategory.AddCustomRow(LOCTEXT("ApplyChanges", "Apply Changes"))
		.ValueContent()
		.HAlign(HAlign_Left)
		[
			SNew(SButton)
			.OnClicked(this, &FPersonaMeshDetails::OnApplyChanges)
			.IsEnabled(this, &FPersonaMeshDetails::IsGenerateAvailable)
			[
				SNew(STextBlock)
				.Text(this, &FPersonaMeshDetails::GetApplyButtonText)
				.Font(DetailLayout.GetDetailFont())
			]
		];
}

void FPersonaMeshDetails::OnImportLOD(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo, IDetailLayoutBuilder* DetailLayout)
{
	int32 LODIndex = 0;
	if (LODNames.Find(NewValue, LODIndex) && LODIndex > 0)
	{
		USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();
		check(SkelMesh);

		FbxMeshUtils::ImportMeshLODDialog(SkelMesh, LODIndex);

		DetailLayout->ForceRefreshDetails();
	}
}

int32 FPersonaMeshDetails::GetLODCount() const
{
	return LODCount;
}

void FPersonaMeshDetails::OnLODCountChanged(int32 NewValue)
{
	LODCount = FMath::Clamp<int32>(NewValue, 1, MAX_SKELETAL_MESH_LODS);

	UpdateLODNames();
}

void FPersonaMeshDetails::OnLODCountCommitted(int32 InValue, ETextCommit::Type CommitInfo)
{
	OnLODCountChanged(InValue);
}

FReply FPersonaMeshDetails::OnApplyChanges()
{
	ApplyChanges();
	return FReply::Handled();
}

FReply FPersonaMeshDetails::RemoveOneLOD(int32 LODIndex)
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();
	check(SkelMesh);
	check(SkelMesh->LODInfo.IsValidIndex(LODIndex));

	FSkeletalMeshUpdateContext UpdateContext;
	UpdateContext.SkeletalMesh = SkelMesh;
	UpdateContext.AssociatedComponents.Push(GetPersonaToolkit()->GetPreviewMeshComponent());

	FLODUtilities::RemoveLOD(UpdateContext, LODIndex);

	MeshDetailLayout->ForceRefreshDetails();
	return FReply::Handled();
}

FReply FPersonaMeshDetails::RemoveBones(int32 LODIndex)
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();
	check(SkelMesh);
	check(SkelMesh->LODInfo.IsValidIndex(LODIndex));

	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	MeshUtilities.RemoveBonesFromMesh(SkelMesh, LODIndex, NULL);

	MeshDetailLayout->ForceRefreshDetails();
	return FReply::Handled();
}

FText FPersonaMeshDetails::GetApplyButtonText() const
{
	if (IsApplyNeeded())	
	{
		return LOCTEXT("ApplyChanges", "Apply Changes");
	}
	else if (IsGenerateAvailable())
	{
		return LOCTEXT("Regenerate", "Regenerate");
	}

	return LOCTEXT("ApplyChanges", "Apply Changes");
}

void FPersonaMeshDetails::ApplyChanges(int32 DesiredLOD, const FSkeletalMeshOptimizationSettings& ReductionSettings)
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();
	check(SkelMesh);

	FSkeletalMeshUpdateContext UpdateContext;
	UpdateContext.SkeletalMesh = SkelMesh;
	UpdateContext.AssociatedComponents.Push(GetPersonaToolkit()->GetPreviewMeshComponent());

	if (SkelMesh->LODInfo.IsValidIndex(DesiredLOD))
	{
		if (SkelMesh->LODInfo[DesiredLOD].bHasBeenSimplified == false)
		{
			const FText Text = FText::Format(LOCTEXT("Warning_SimplygonApplyingToImportedMesh", "LOD {0} has been imported. Are you sure you'd like to apply mesh reduction? This will destroy imported LOD."), FText::AsNumber(DesiredLOD));
			EAppReturnType::Type Ret = FMessageDialog::Open(EAppMsgType::YesNo, Text);
			if (Ret == EAppReturnType::No)
			{
				return;
			}
		}

		FLODUtilities::SimplifySkeletalMeshLOD(UpdateContext, ReductionSettings, DesiredLOD);

		// update back to LODInfo
		FSkeletalMeshLODInfo& CurrentLODInfo = SkelMesh->LODInfo[DesiredLOD];
		CurrentLODInfo.ReductionSettings = ReductionSettings;
	}
}

void FPersonaMeshDetails::ApplyChanges()
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();
	check(SkelMesh);

	FSkeletalMeshUpdateContext UpdateContext;
	UpdateContext.SkeletalMesh = SkelMesh;
	UpdateContext.AssociatedComponents.Push(GetPersonaToolkit()->GetPreviewMeshComponent());

	// remove LODs
	int32 CurrentNumLODs = SkelMesh->LODInfo.Num();
	if (LODCount < CurrentNumLODs)
	{
		for (int32 LODIdx = CurrentNumLODs - 1; LODIdx >= LODCount; LODIdx--)
		{
			FLODUtilities::RemoveLOD(UpdateContext, LODIdx);
		}
	}
	// we need to add more
	else if (LODCount > CurrentNumLODs)
	{
		// Retrieve default skeletal mesh reduction settings
		USkeletalMeshReductionSettings* ReductionSettings = USkeletalMeshReductionSettings::Get();		

		// Only create new skeletal mesh LOD level entries
		for (int32 LODIdx = CurrentNumLODs; LODIdx < LODCount; LODIdx++)
		{
			FSkeletalMeshOptimizationSettings Settings;
			
			const int32 SettingsIndex = LODIdx - 1;
			// If there are valid default settings use those for the new LOD

			const bool bHasValidUserSetting = ReductionSettings->HasValidSettings() && ReductionSettings->GetNumberOfSettings() > SettingsIndex;
			if (bHasValidUserSetting)
			{
				const FSkeletalMeshLODGroupSettings& GroupSettings = ReductionSettings->GetDefaultSettingsForLODLevel(SettingsIndex);
				Settings = GroupSettings.GetSettings();
			}
			else
			{
				// Otherwise find whatever latest that was using mesh reduction, and make it 50% of that. 	
				for (int32 SubLOD = LODIdx - 1; SubLOD >= 0; --SubLOD)
				{
					if (SkelMesh->LODInfo[SubLOD].bHasBeenSimplified)
					{
						// copy whatever latest LOD info reduction setting
						Settings = SkelMesh->LODInfo[SubLOD].ReductionSettings;
						// and make it 50 % of that
						Settings.NumOfTrianglesPercentage *= 0.5f;
						break;
					}
				}
			}

			// if no previous setting found, it will use default setting. 
			FLODUtilities::SimplifySkeletalMeshLOD(UpdateContext, Settings, LODIdx);

			if (SkelMesh->LODInfo.IsValidIndex(LODIdx))
			{
				FSkeletalMeshLODInfo& Info = SkelMesh->LODInfo[LODIdx];
				Info.ReductionSettings = Settings;

				// If there is a valid screensize value use that one for this new LOD
				if (bHasValidUserSetting)
				{
					const FSkeletalMeshLODGroupSettings& GroupSettings = ReductionSettings->GetDefaultSettingsForLODLevel(SettingsIndex);
					Info.ScreenSize = GroupSettings.GetScreenSize();
				}
			}
		}
	}
	else if (IsApplyNeeded())
	{
		for (int32 LODIdx = 1; LODIdx < LODCount; LODIdx++)
		{
			FSkeletalMeshLODInfo& CurrentLODInfo = SkelMesh->LODInfo[LODIdx];
			// it has been updated, and then following data
			bool bNeedsUpdate = ReductionSettingsWidgets.IsValidIndex(LODIdx)
				&& ReductionSettingsWidgets[LODIdx].IsValid()
				&& ReductionSettingsWidgets[LODIdx]->GetSettings() != CurrentLODInfo.ReductionSettings;

			// if it has been simplified, regenerate
			if (bNeedsUpdate)
			{
				if (CurrentLODInfo.bHasBeenSimplified == false)
				{
					const FText Text = FText::Format(LOCTEXT("Warning_SimplygonApplyingToImportedMesh", "LOD {0} has been imported. Are you sure you'd like to apply mesh reduction? This will destroy imported LOD."), FText::AsNumber(LODIdx));
					EAppReturnType::Type Ret = FMessageDialog::Open(EAppMsgType::YesNo, Text);
					if (Ret == EAppReturnType::No)
					{
						continue;
					}
				}

				const FSkeletalMeshOptimizationSettings& Setting = ReductionSettingsWidgets[LODIdx]->GetSettings();
				FLODUtilities::SimplifySkeletalMeshLOD(UpdateContext, Setting, LODIdx);
				CurrentLODInfo.ReductionSettings = Setting;
			}
		}
	}
	else
	{
		for (int32 LODIdx = 1; LODIdx < LODCount; LODIdx++)
		{
			FSkeletalMeshLODInfo& CurrentLODInfo = SkelMesh->LODInfo[LODIdx];
			if (CurrentLODInfo.bHasBeenSimplified == true)
			{
				const FSkeletalMeshOptimizationSettings& Setting = ReductionSettingsWidgets[LODIdx]->GetSettings();
				FLODUtilities::SimplifySkeletalMeshLOD(UpdateContext, Setting, LODIdx);
				CurrentLODInfo.ReductionSettings = Setting;
			}
		}
	}

	MeshDetailLayout->ForceRefreshDetails();
}

void FPersonaMeshDetails::UpdateLODNames()
{
	LODNames.Empty();
	LODNames.Add(MakeShareable(new FString(LOCTEXT("BaseLOD", "Base LOD").ToString())));
	for (int32 LODLevelID = 1; LODLevelID < LODCount; ++LODLevelID)
	{
		LODNames.Add(MakeShareable(new FString(FText::Format(NSLOCTEXT("LODSettingsLayout", "LODLevel_Reimport", "Reimport LOD Level {0}"), FText::AsNumber(LODLevelID)).ToString())));
	}
	LODNames.Add(MakeShareable(new FString(FText::Format(NSLOCTEXT("LODSettingsLayout", "LODLevel_Import", "Import LOD Level {0}"), FText::AsNumber(LODCount)).ToString())));
}

bool FPersonaMeshDetails::IsGenerateAvailable() const
{
	if (IsApplyNeeded())
	{
		return true;
	}

	// if you have LOD
	return (LODCount > 1);
}
bool FPersonaMeshDetails::IsApplyNeeded() const
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();
	check(SkelMesh);

	if (SkelMesh->LODInfo.Num() != LODCount)
	{
		return true;
	}


	for (int32 LODIndex = 0; LODIndex < LODCount; ++LODIndex)
	{
		FSkeletalMeshLODInfo& Info = SkelMesh->LODInfo[LODIndex];
		if (ReductionSettingsWidgets.IsValidIndex(LODIndex)
			&& ReductionSettingsWidgets[LODIndex].IsValid()
			&& Info.ReductionSettings != ReductionSettingsWidgets[LODIndex]->GetSettings())
		{
			return true;
		}
	}

	return false;
}

FText FPersonaMeshDetails::GetLODCountTooltip() const
{
	if (IsAutoMeshReductionAvailable())
	{
		return LOCTEXT("LODCountTooltip", "The number of LODs for this skeletal mesh. If auto mesh reduction is available, setting this number will determine the number of LOD levels to auto generate.");
	}

	return LOCTEXT("LODCountTooltip_Disabled", "Auto mesh reduction is unavailable! Please provide a mesh reduction interface such as Simplygon to use this feature or manually import LOD levels.");
}

FText FPersonaMeshDetails::GetLODImportedText(int32 LODIndex) const
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();
	if (Mesh && Mesh->LODInfo.IsValidIndex(LODIndex))
	{
		if (Mesh->LODInfo[LODIndex].bHasBeenSimplified)
		{
			return  LOCTEXT("LODMeshReductionText_Label", "[generated]");
		}
	}

	return FText();
}

FText FPersonaMeshDetails::GetMaterialSlotNameText(int32 MaterialIndex) const
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();
	if (Mesh && Mesh->Materials.IsValidIndex(MaterialIndex))
	{
		return FText::FromName(Mesh->Materials[MaterialIndex].MaterialSlotName);
	}

	return LOCTEXT("SkeletalMeshMaterial_InvalidIndex", "Invalid Material Index");
}

void FPersonaMeshDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailLayout.GetDetailsView().GetSelectedObjects();
	check(SelectedObjects.Num()<=1); // The OnGenerateCustomWidgets delegate will not be useful if we try to process more than one object.

	SkeletalMeshPtr = SelectedObjects.Num() > 0 ? Cast<USkeletalMesh>(SelectedObjects[0].Get()) : nullptr;

	// copy temporarily to refresh Mesh details tab from the LOD settings window
	MeshDetailLayout = &DetailLayout;

	// add multiple LOD levels to LOD category
	AddLODLevelCategories(DetailLayout);

	CustomizeLODSettingsCategories(DetailLayout);

#if WITH_APEX_CLOTHING
	IDetailCategoryBuilder& ClothingCategory = DetailLayout.EditCategory("Clothing", FText::GetEmpty(), ECategoryPriority::TypeSpecific);
	CustomizeClothingProperties(DetailLayout,ClothingCategory);
#endif// #if WITH_APEX_CLOTHING

	// Post process selector
	IDetailCategoryBuilder& SkelMeshCategory = DetailLayout.EditCategory("SkeletalMesh");
	TSharedRef<IPropertyHandle> PostProcessHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(USkeletalMesh, PostProcessAnimBlueprint), USkeletalMesh::StaticClass());
	PostProcessHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FPersonaMeshDetails::OnPostProcessBlueprintChanged, &DetailLayout));
	PostProcessHandle->MarkHiddenByCustomization();

	FDetailWidgetRow& PostProcessRow = SkelMeshCategory.AddCustomRow(LOCTEXT("PostProcessFilterString", "Post Process Blueprint"));
	PostProcessRow.NameContent()
		[
			PostProcessHandle->CreatePropertyNameWidget()
		];

	PostProcessRow.ValueContent()
	[
			SNew(SObjectPropertyEntryBox)
			.ObjectPath(this, &FPersonaMeshDetails::GetCurrentPostProcessBlueprintPath)
			.AllowedClass(UAnimBlueprint::StaticClass())
			.NewAssetFactories(TArray<UFactory*>())
			.OnShouldFilterAsset(FOnShouldFilterAsset::CreateSP(this, &FPersonaMeshDetails::OnShouldFilterPostProcessBlueprint))
			.OnObjectChanged(FOnSetObject::CreateSP(this, &FPersonaMeshDetails::OnSetPostProcessBlueprint, PostProcessHandle))
	];

	HideUnnecessaryProperties(DetailLayout);
}

void FPersonaMeshDetails::HideUnnecessaryProperties(IDetailLayoutBuilder& DetailLayout)
{
	// LODInfo doesn't need to be showed anymore because it was moved to each LOD category
	TSharedRef<IPropertyHandle> LODInfoProperty = DetailLayout.GetProperty(FName("LODInfo"), USkeletalMesh::StaticClass());
	DetailLayout.HideProperty(LODInfoProperty);
	uint32 NumChildren = 0;
	LODInfoProperty->GetNumChildren(NumChildren);
	// Hide reduction settings property because it is duplicated with Reduction settings layout created by ReductionSettingsWidgets
	for (uint32 ChildIdx = 0; ChildIdx < NumChildren; ChildIdx++)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = LODInfoProperty->GetChildHandle(ChildIdx);
		if (ChildHandle.IsValid())
		{
			TSharedPtr<IPropertyHandle> MaterialMapHandle = ChildHandle->GetChildHandle(FName("LODMaterialMap"));
			DetailLayout.HideProperty(MaterialMapHandle);

			TSharedPtr<IPropertyHandle> TriangleSortHandle = ChildHandle->GetChildHandle(FName("TriangleSortSettings"));
			DetailLayout.HideProperty(TriangleSortHandle);

			TSharedPtr<IPropertyHandle> ReductionHandle = ChildHandle->GetChildHandle(FName("ReductionSettings"));
			DetailLayout.HideProperty(ReductionHandle);
		}
	}

	TSharedRef<IPropertyHandle> MaterialsProperty = DetailLayout.GetProperty(FName("Materials"), USkeletalMesh::StaticClass());
	DetailLayout.HideProperty(MaterialsProperty);

	// hide all properties in Mirroring category to hide Mirroring category itself
	IDetailCategoryBuilder& MirroringCategory = DetailLayout.EditCategory("Mirroring", FText::GetEmpty(), ECategoryPriority::Default);
	TArray<TSharedRef<IPropertyHandle>> MirroringProperties;
	MirroringCategory.GetDefaultProperties(MirroringProperties);
	for (int32 MirrorPropertyIdx = 0; MirrorPropertyIdx < MirroringProperties.Num(); MirrorPropertyIdx++)
	{
		DetailLayout.HideProperty(MirroringProperties[MirrorPropertyIdx]);
	}
}

void FPersonaMeshDetails::OnPostProcessBlueprintChanged(IDetailLayoutBuilder* DetailBuilder)
{
	DetailBuilder->ForceRefreshDetails();
}

FString FPersonaMeshDetails::GetCurrentPostProcessBlueprintPath() const
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();
	if(UClass* PostProcessClass = *SkelMesh->PostProcessAnimBlueprint)
	{
		return PostProcessClass->GetPathName();
	}

	return FString();
}

bool FPersonaMeshDetails::OnShouldFilterPostProcessBlueprint(const FAssetData& AssetData) const
{
	if(USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh())
	{
		const FString CurrentMeshSkeletonName = FString::Printf(TEXT("%s'%s'"), *SkelMesh->Skeleton->GetClass()->GetName(), *SkelMesh->Skeleton->GetPathName());
		const FString SkeletonName = AssetData.GetTagValueRef<FString>("TargetSkeleton");

		return SkeletonName != CurrentMeshSkeletonName;
	}

	return true;
}

void FPersonaMeshDetails::OnSetPostProcessBlueprint(const FAssetData& AssetData, TSharedRef<IPropertyHandle> BlueprintProperty)
{
	if(UAnimBlueprint* SelectedBlueprint = Cast<UAnimBlueprint>(AssetData.GetAsset()))
	{
		BlueprintProperty->SetValue(SelectedBlueprint->GetAnimBlueprintGeneratedClass());
	}
}

FReply FPersonaMeshDetails::OnReimportLodClicked(IDetailLayoutBuilder* DetailLayout, EReimportButtonType InReimportType, int32 InLODIndex)
{
	if(USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh())
	{
		if(!SkelMesh->LODInfo.IsValidIndex(InLODIndex))
		{
			return FReply::Unhandled();
		}

		FString SourceFilenameBackup("");
		if(InReimportType == EReimportButtonType::ReimportWithNewFile)
		{
			// Back up current source filename and empty it so the importer asks for a new one.
			SourceFilenameBackup = SkelMesh->LODInfo[InLODIndex].SourceImportFilename;
			SkelMesh->LODInfo[InLODIndex].SourceImportFilename.Empty();
		}

		bool bImportSucceeded = FbxMeshUtils::ImportMeshLODDialog(SkelMesh, InLODIndex);

		if(InReimportType == EReimportButtonType::ReimportWithNewFile && !bImportSucceeded)
		{
			// Copy old source file back, as this one failed
			SkelMesh->LODInfo[InLODIndex].SourceImportFilename = SourceFilenameBackup;
		}

		if(DetailLayout)
		{
			DetailLayout->ForceRefreshDetails();
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void FPersonaMeshDetails::OnGetMaterialsForArray(class IMaterialListBuilder& OutMaterials, int32 LODIndex)
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();

	if (!SkelMesh)
		return;

	for (int32 MaterialIndex = 0; MaterialIndex < SkelMesh->Materials.Num(); ++MaterialIndex)
	{
		OutMaterials.AddMaterial(MaterialIndex, SkelMesh->Materials[MaterialIndex].MaterialInterface, true);
	}
}

void FPersonaMeshDetails::OnMaterialArrayChanged(UMaterialInterface* NewMaterial, UMaterialInterface* PrevMaterial, int32 SlotIndex, bool bReplaceAll, int32 LODIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();
	if (Mesh)
	{
		// Whether or not we made a transaction and need to end it
		bool bMadeTransaction = false;

		UProperty* MaterialProperty = FindField<UProperty>(USkeletalMesh::StaticClass(), "Materials");
		check(MaterialProperty);
		Mesh->PreEditChange(MaterialProperty);
		check(Mesh->Materials.Num() > SlotIndex)

		if (NewMaterial != PrevMaterial)
		{
			GEditor->BeginTransaction(LOCTEXT("PersonaReplaceMaterial", "Replace material on mesh"));
			bMadeTransaction = true;
			Mesh->Modify();
			Mesh->Materials[SlotIndex].MaterialInterface = NewMaterial;

			//Add a default name to the material slot if this slot was manually add and there is no name yet
			if (NewMaterial != nullptr && Mesh->Materials[SlotIndex].ImportedMaterialSlotName == NAME_None && Mesh->Materials[SlotIndex].MaterialSlotName == NAME_None)
			{
				Mesh->Materials[SlotIndex].MaterialSlotName = FName(*(NewMaterial->GetName()));
			}
		}

		FPropertyChangedEvent PropertyChangedEvent(MaterialProperty);
		Mesh->PostEditChangeProperty(PropertyChangedEvent);

		if (bMadeTransaction)
		{
			// End the transation if we created one
			GEditor->EndTransaction();
			// Redraw viewports to reflect the material changes 
			GUnrealEd->RedrawLevelEditingViewports();
		}
	}
}

FReply FPersonaMeshDetails::AddMaterialSlot()
{
	if (!SkeletalMeshPtr.IsValid())
	{
		return FReply::Handled();
	}

	FScopedTransaction Transaction(LOCTEXT("PersonaChangedMaterialIndex", "Change material index on mesh"));

	SkeletalMeshPtr->Modify();
	SkeletalMeshPtr->Materials.Add(FSkeletalMaterial());

	SkeletalMeshPtr->PostEditChange();

	return FReply::Handled();
}

FText FPersonaMeshDetails::GetMaterialArrayText() const
{
	FString MaterialArrayText = TEXT(" Material Slots");
	int32 SlotNumber = 0;
	if (SkeletalMeshPtr.IsValid())
	{
		SlotNumber = SkeletalMeshPtr->Materials.Num();
	}
	MaterialArrayText = FString::FromInt(SlotNumber) + MaterialArrayText;
	return FText::FromString(MaterialArrayText);
}

void FPersonaMeshDetails::OnGetSectionsForView(ISectionListBuilder& OutSections, int32 LODIndex)
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();

	FSkeletalMeshResource* ImportedResource = SkelMesh->GetImportedResource();

	if (ImportedResource && ImportedResource->LODModels.IsValidIndex(LODIndex))
	{
		FStaticLODModel& Model = ImportedResource->LODModels[LODIndex];

		bool bHasMaterialMap = SkelMesh->LODInfo.IsValidIndex(LODIndex) && SkelMesh->LODInfo[LODIndex].LODMaterialMap.Num() > 0;

		if (LODIndex == 0 || !bHasMaterialMap)
		{
			int32 NumSections = Model.NumNonClothingSections();
			for (int32 SectionIdx = 0; SectionIdx < NumSections; SectionIdx++)
			{
				int32 MaterialIndex = Model.Sections[SectionIdx].MaterialIndex;

				if (SkelMesh->Materials.IsValidIndex(MaterialIndex))
				{
					FName CurrentSectionMaterialSlotName = SkelMesh->Materials[MaterialIndex].MaterialSlotName;
					FName CurrentSectionOriginalImportedMaterialName = SkelMesh->Materials[MaterialIndex].ImportedMaterialSlotName;
					TMap<int32, FName> AvailableSectionName;
					int32 CurrentIterMaterialIndex = 0;
					for (const FSkeletalMaterial &SkeletalMaterial : SkelMesh->Materials)
					{
						if (MaterialIndex != CurrentIterMaterialIndex)
							AvailableSectionName.Add(CurrentIterMaterialIndex, SkeletalMaterial.MaterialSlotName);
						CurrentIterMaterialIndex++;
					}
					bool bClothSection = Model.Sections[SectionIdx].CorrespondClothSectionIndex >= 0;
					OutSections.AddSection(LODIndex, SectionIdx, CurrentSectionMaterialSlotName, MaterialIndex, CurrentSectionOriginalImportedMaterialName, AvailableSectionName, SkelMesh->Materials[MaterialIndex].MaterialInterface, bClothSection);
				}
			}
		}
		else // refers to LODMaterialMap
		{
			TArray<int32>& MaterialMap = SkelMesh->LODInfo[LODIndex].LODMaterialMap;

			for(int32 MapIdx = 0; MapIdx < MaterialMap.Num(); MapIdx++)
			{
				int32 MaterialIndex = MaterialMap[MapIdx];

				if (!SkelMesh->Materials.IsValidIndex(MaterialIndex))
				{
					MaterialMap[MapIdx] = MaterialIndex = SkelMesh->Materials.Add(FSkeletalMaterial());
				}
				FName CurrentSectionMaterialSlotName = SkelMesh->Materials[MaterialIndex].MaterialSlotName;
				FName CurrentSectionOriginalImportedMaterialName = SkelMesh->Materials[MaterialIndex].ImportedMaterialSlotName;
				TMap<int32, FName> AvailableSectionName;
				int32 CurrentIterMaterialIndex = 0;
				for (const FSkeletalMaterial &SkeletalMaterial : SkelMesh->Materials)
				{
					if (MaterialIndex != CurrentIterMaterialIndex)
						AvailableSectionName.Add(CurrentIterMaterialIndex, SkeletalMaterial.MaterialSlotName);
					CurrentIterMaterialIndex++;
				}
				OutSections.AddSection(LODIndex, MapIdx, CurrentSectionMaterialSlotName, MaterialIndex, CurrentSectionOriginalImportedMaterialName, AvailableSectionName, SkelMesh->Materials[MaterialIndex].MaterialInterface, false);
			}
		}

	}
}

FText FPersonaMeshDetails::GetMaterialNameText(int32 MaterialIndex) const
{
	if (SkeletalMeshPtr.IsValid() && SkeletalMeshPtr->Materials.IsValidIndex(MaterialIndex))
	{
		return FText::FromName(SkeletalMeshPtr->Materials[MaterialIndex].MaterialSlotName);
	}
	return FText::FromName(NAME_None);
}

FText FPersonaMeshDetails::GetOriginalImportMaterialNameText(int32 MaterialIndex) const
{
	if (SkeletalMeshPtr.IsValid() && SkeletalMeshPtr->Materials.IsValidIndex(MaterialIndex))
	{
		FString OriginalImportMaterialName;
		SkeletalMeshPtr->Materials[MaterialIndex].ImportedMaterialSlotName.ToString(OriginalImportMaterialName);
		OriginalImportMaterialName = TEXT("Original Imported Material Name: ") + OriginalImportMaterialName;
		return FText::FromString(OriginalImportMaterialName);
			}
	return FText::FromName(NAME_None);
		}

void FPersonaMeshDetails::OnMaterialNameCommitted(const FText& InValue, ETextCommit::Type CommitType, int32 MaterialIndex)
{
	OnMaterialNameChanged(InValue, MaterialIndex);
}

void FPersonaMeshDetails::OnMaterialNameChanged(const FText& InValue, int32 MaterialIndex)
{
	if (SkeletalMeshPtr.IsValid() && SkeletalMeshPtr->Materials.IsValidIndex(MaterialIndex))
	{
		SkeletalMeshPtr->Materials[MaterialIndex].MaterialSlotName = FName(*(InValue.ToString()));
	}
}


TSharedRef<SWidget> FPersonaMeshDetails::OnGenerateCustomMaterialWidgetsForMaterialArray(UMaterialInterface* Material, int32 MaterialIndex, int32 LODIndex)
{
	bool bMaterialIsUsed = false;
	if(SkeletalMeshPtr.IsValid() && MaterialUsedMap.Contains(MaterialIndex))
	{
		bMaterialIsUsed = MaterialUsedMap.Find(MaterialIndex)->Num() > 0;
	}

	return
		SNew(SMaterialSlotWidget, MaterialIndex, bMaterialIsUsed)
		.MaterialName(this, &FPersonaMeshDetails::GetMaterialNameText, MaterialIndex)
		.OnMaterialNameChanged(this, &FPersonaMeshDetails::OnMaterialNameChanged, MaterialIndex)
		.OnMaterialNameCommitted(this, &FPersonaMeshDetails::OnMaterialNameCommitted, MaterialIndex)
		.CanDeleteMaterialSlot(this, &FPersonaMeshDetails::CanDeleteMaterialSlot, MaterialIndex)
		.OnDeleteMaterialSlot(this, &FPersonaMeshDetails::OnDeleteMaterialSlot, MaterialIndex)
		.ToolTipText(this, &FPersonaMeshDetails::GetOriginalImportMaterialNameText, MaterialIndex);
}

FText FPersonaMeshDetails::GetFirstMaterialSlotUsedBySection(int32 MaterialIndex) const
{
	if (SkeletalMeshPtr.IsValid() && MaterialUsedMap.Contains(MaterialIndex))
	{
		const TArray<FSectionLocalizer> *SectionLocalizers = MaterialUsedMap.Find(MaterialIndex);
		if (SectionLocalizers->Num() > 0)
		{
			FString ArrayItemName = FString::FromInt(SectionLocalizers->Num()) + TEXT(" Sections");
			return FText::FromString(ArrayItemName);
		}
	}
	return FText();
}

TSharedRef<SWidget> FPersonaMeshDetails::OnGetMaterialSlotUsedByMenuContent(int32 MaterialIndex)
{
	FMenuBuilder MenuBuilder(true, NULL);

	TArray<FSectionLocalizer> *SectionLocalizers;
	if (SkeletalMeshPtr.IsValid() && MaterialUsedMap.Contains(MaterialIndex))
{
		SectionLocalizers = MaterialUsedMap.Find(MaterialIndex);
		FUIAction Action;
		FText EmptyTooltip;
		// Add a menu item for each texture.  Clicking on the texture will display it in the content browser
		for (const FSectionLocalizer& SectionUsingMaterial : (*SectionLocalizers))
		{
			FString ArrayItemName = TEXT("Lod ") + FString::FromInt(SectionUsingMaterial.LODIndex) + TEXT("  Index ") + FString::FromInt(SectionUsingMaterial.SectionIndex);
			MenuBuilder.AddMenuEntry(FText::FromString(ArrayItemName), EmptyTooltip, FSlateIcon(), Action);
		}
	}
	

	return MenuBuilder.MakeWidget();
}

bool FPersonaMeshDetails::CanDeleteMaterialSlot(int32 MaterialIndex) const
	{
	if (!SkeletalMeshPtr.IsValid())
	{
		return false;
	}

	return (MaterialIndex + 1) == SkeletalMeshPtr->Materials.Num();
	}
	
void FPersonaMeshDetails::OnDeleteMaterialSlot(int32 MaterialIndex)
{
	if (!SkeletalMeshPtr.IsValid() || !CanDeleteMaterialSlot(MaterialIndex))
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("PersonaDeletedMaterialSlot", "Delete material slot on Skeletal Mesh"));
	SkeletalMeshPtr->Modify();
	SkeletalMeshPtr->Materials.RemoveAt(MaterialIndex);

	SkeletalMeshPtr->PostEditChange();
}

bool FPersonaMeshDetails::OnMaterialListDirty()
{
	bool ForceMaterialListRefresh = false;
	TMap<int32, TArray<FSectionLocalizer>> TempMaterialUsedMap;
	if (SkeletalMeshPtr.IsValid())
	{
		for (int32 MaterialIndex = 0; MaterialIndex < SkeletalMeshPtr->Materials.Num(); ++MaterialIndex)
		{
			TArray<FSectionLocalizer> SectionLocalizers;
			FSkeletalMeshResource* ImportedResource = SkeletalMeshPtr->GetImportedResource();
			check(ImportedResource);
			for (int32 LODIndex = 0; LODIndex < ImportedResource->LODModels.Num(); ++LODIndex)
			{
				FSkeletalMeshLODInfo& Info = SkeletalMeshPtr->LODInfo[LODIndex];
				if (LODIndex == 0 || SkeletalMeshPtr->LODInfo[LODIndex].LODMaterialMap.Num() == 0)
				{
					for (int32 SectionIndex = 0; SectionIndex < ImportedResource->LODModels[LODIndex].Sections.Num(); ++SectionIndex)
					{
						if (GetMaterialIndex(LODIndex, SectionIndex) == MaterialIndex)
						{
							SectionLocalizers.Add(FSectionLocalizer(LODIndex, SectionIndex));
						}
					}
				}
				else
				{
					for (int32 SectionIndex = 0; SectionIndex < SkeletalMeshPtr->LODInfo[LODIndex].LODMaterialMap.Num(); ++SectionIndex)
					{
						if (GetMaterialIndex(LODIndex, SectionIndex) == MaterialIndex)
						{
							SectionLocalizers.Add(FSectionLocalizer(LODIndex, SectionIndex));
						}
					}
				}
			}
			TempMaterialUsedMap.Add(MaterialIndex, SectionLocalizers);
		}
	}
	if (TempMaterialUsedMap.Num() != MaterialUsedMap.Num())
	{
		ForceMaterialListRefresh = true;
	}
	else if (!ForceMaterialListRefresh)
	{
		for (auto KvpOld : MaterialUsedMap)
		{
			if (!TempMaterialUsedMap.Contains(KvpOld.Key))
			{
				ForceMaterialListRefresh = true;
				break;
			}
			const TArray<FSectionLocalizer> &TempSectionLocalizers = (*(TempMaterialUsedMap.Find(KvpOld.Key)));
			const TArray<FSectionLocalizer> &OldSectionLocalizers = KvpOld.Value;
			if (TempSectionLocalizers.Num() != OldSectionLocalizers.Num())
			{
				ForceMaterialListRefresh = true;
				break;
			}
			for (int32 SectionLocalizerIndex = 0; SectionLocalizerIndex < OldSectionLocalizers.Num(); ++SectionLocalizerIndex)
			{
				if (OldSectionLocalizers[SectionLocalizerIndex] != TempSectionLocalizers[SectionLocalizerIndex])
				{
					ForceMaterialListRefresh = true;
					break;
				}
			}
			if (ForceMaterialListRefresh)
	{
				break;
			}
		}
	}
	MaterialUsedMap = TempMaterialUsedMap;

	return ForceMaterialListRefresh;
}

TSharedRef<SWidget> FPersonaMeshDetails::OnGenerateCustomNameWidgetsForSection(int32 LodIndex, int32 SectionIndex)
{
	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FPersonaMeshDetails::IsSectionSelected, SectionIndex)
			.OnCheckStateChanged(this, &FPersonaMeshDetails::OnSectionSelectedChanged, SectionIndex)
			.ToolTipText(LOCTEXT("Highlight_ToolTip", "Highlights this section in the viewport"))
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f, 1.0f))
				.Text(LOCTEXT("Highlight", "Highlight"))
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 0)
			[
			SNew(SCheckBox)
			.IsChecked(this, &FPersonaMeshDetails::IsIsolateSectionEnabled, SectionIndex)
			.OnCheckStateChanged(this, &FPersonaMeshDetails::OnSectionIsolatedChanged, SectionIndex)
			.ToolTipText(LOCTEXT("Isolate_ToolTip", "Isolates this section in the viewport"))
				[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f, 1.0f))
				.Text(LOCTEXT("Isolate", "Isolate"))
				]
		];
}

TSharedRef<SWidget> FPersonaMeshDetails::OnGenerateCustomSectionWidgetsForSection(int32 LODIndex, int32 SectionIndex)
{
	TSharedRef<SVerticalBox> SectionWidget
		= SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.Padding(0, 2, 0, 0)
		[
			SNew(SCheckBox)
			.IsChecked(this, &FPersonaMeshDetails::IsSectionShadowCastingEnabled, LODIndex, SectionIndex)
			.OnCheckStateChanged(this, &FPersonaMeshDetails::OnSectionShadowCastingChanged, LODIndex, SectionIndex)
			[
				SNew(STextBlock)
				.Font(FEditorStyle::GetFontStyle("StaticMeshEditor.NormalFont"))
				.Text(LOCTEXT("Cast Shadows", "Cast Shadows"))
			]
		]

		+ SVerticalBox::Slot()
		.Padding(0, 2, 0, 0)
		[
			SNew(SCheckBox)
			.IsEnabled(IsSkinCacheAvailable())
			.IsChecked(this, &FPersonaMeshDetails::IsSectionRecomputeTangentEnabled, LODIndex, SectionIndex)
			.OnCheckStateChanged(this, &FPersonaMeshDetails::OnSectionRecomputeTangentChanged, LODIndex, SectionIndex)
			[
				SNew(STextBlock)
				.Font(FEditorStyle::GetFontStyle("StaticMeshEditor.NormalFont"))
				.Text(LOCTEXT("RecomputeTangent_Title", "Recompute Tangent"))
				.ToolTipText(LOCTEXT("RecomputeTangent_Tooltip", "This feature only works if you enable (Support Skincache Shaders) in the Project Settings. Please note that skin cache is an experimental feature and only works if you have compute shaders."))
			]
		];
#if WITH_APEX_CLOTHING

	if (LODIndex == 0 && SectionIndex == 0)
	{
		ClothingComboLODInfos.Empty();
	}

	if (ClothingComboLODInfos.Num() <= LODIndex)
	{
		// Generate strings for the combo boxes assets
		UpdateComboBoxStrings();
	}
	
	TArray<TSharedPtr< STextComboBox >>& ComboBoxes = ClothingComboLODInfos[LODIndex].ClothingComboBoxes;
	TArray<TSharedPtr<FString> >& ComboStrings = ClothingComboLODInfos[LODIndex].ClothingComboStrings;
	TMap<FString, FClothAssetSubmeshIndex>&	ComboStringReverseLookup = ClothingComboLODInfos[LODIndex].ClothingComboStringReverseLookup;
	TArray<int32>& ComboSelectedIndices = ClothingComboLODInfos[LODIndex].ClothingComboSelectedIndices;

	while (ComboBoxes.Num() <= SectionIndex)
	{
		ComboBoxes.AddZeroed();
	}


	int32 MaterialIndex = GetMaterialIndex(LODIndex, SectionIndex);
	SectionWidget->AddSlot()
		// Add APEX clothing combo boxes
		.AutoHeight()
		.Padding(0,2,0,0)
		[
			SNew( SHorizontalBox )
			.Visibility(this, &FPersonaMeshDetails::IsClothingComboBoxVisible, LODIndex, SectionIndex)

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Clothing", "Clothing"))
			]

			+SHorizontalBox::Slot()
			.Padding(2,2,0,0)
			.AutoWidth()
			[
				SAssignNew(ComboBoxes[SectionIndex], STextComboBox)
				.ContentPadding(FMargin(6.0f, 2.0f))
				.OptionsSource(&ComboStrings)
				.ToolTipText(LOCTEXT("SectionsComboBoxToolTip", "Select the clothing asset and submesh to use as clothing for this section"))
				.OnSelectionChanged(this, &FPersonaMeshDetails::HandleSectionsComboBoxSelectionChanged, LODIndex, SectionIndex)
			]
		];

		int32 SelectedIndex = ComboSelectedIndices.IsValidIndex(SectionIndex) ? ComboSelectedIndices[SectionIndex] : -1;
		ComboBoxes[SectionIndex]->SetSelectedItem(ComboStrings.IsValidIndex(SelectedIndex) ? ComboStrings[SelectedIndex] : ComboStrings[0]);
		
#endif// #if WITH_APEX_CLOTHING
	return SectionWidget;
}

ECheckBoxState FPersonaMeshDetails::IsSectionSelected(int32 SectionIndex) const
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	const USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();
	if (Mesh)
	{
		State = Mesh->SelectedEditorSection == SectionIndex ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	return State;
}

void FPersonaMeshDetails::OnSectionSelectedChanged(ECheckBoxState NewState, int32 SectionIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();

	// Currently assumes that we only ever have one preview mesh in Persona.
	UDebugSkelMeshComponent* MeshComponent = GetPersonaToolkit()->GetPreviewScene()->GetPreviewMeshComponent();

	if (Mesh && MeshComponent)
	{
		if (NewState == ECheckBoxState::Checked)
		{
			Mesh->SelectedEditorSection = SectionIndex;
			if (MeshComponent->SectionIndexPreview != SectionIndex)
			{
				// Unhide all mesh sections
				MeshComponent->SetSectionPreview(INDEX_NONE);
			}
		}
		else if (NewState == ECheckBoxState::Unchecked)
		{
			Mesh->SelectedEditorSection = INDEX_NONE;
		}
		GetPersonaToolkit()->GetPreviewScene()->InvalidateViews();
	}
}

ECheckBoxState FPersonaMeshDetails::IsIsolateSectionEnabled(int32 SectionIndex) const
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	const UDebugSkelMeshComponent* MeshComponent = GetPersonaToolkit()->GetPreviewScene()->GetPreviewMeshComponent();
	if (MeshComponent)
	{
		State = MeshComponent->SectionIndexPreview == SectionIndex ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return State;
}

void FPersonaMeshDetails::OnSectionIsolatedChanged(ECheckBoxState NewState, int32 SectionIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();
	UDebugSkelMeshComponent * MeshComponent = GetPersonaToolkit()->GetPreviewScene()->GetPreviewMeshComponent();
	if (Mesh && MeshComponent)
	{
		if (NewState == ECheckBoxState::Checked)
		{
			MeshComponent->SetSectionPreview(SectionIndex);
			if (Mesh->SelectedEditorSection != SectionIndex)
			{
				Mesh->SelectedEditorSection = INDEX_NONE;
			}
		}
		else if (NewState == ECheckBoxState::Unchecked)
		{
			MeshComponent->SetSectionPreview(INDEX_NONE);
		}
		GetPersonaToolkit()->GetPreviewScene()->InvalidateViews();
	}
}


ECheckBoxState FPersonaMeshDetails::IsShadowCastingEnabled(int32 MaterialIndex) const
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	const USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();
	bool FirstValueSet = false;
	bool AllValueState = false;
	bool AllValueSame = true;
	if (Mesh == nullptr)
		return State;
	check(Mesh->GetResourceForRendering());
	for (int32 LODIdx = 0; LODIdx < Mesh->GetResourceForRendering()->LODModels.Num(); LODIdx++)
	{
		const FStaticLODModel& LODModel = Mesh->GetResourceForRendering()->LODModels[LODIdx];
		for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); ++SectionIndex)
		{
			const FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

			if (GetMaterialIndex(LODIdx, SectionIndex) != MaterialIndex)
				continue;

			if (!FirstValueSet)
			{
				FirstValueSet = true;
				AllValueState = Section.bCastShadow;
				AllValueSame = true;
			}
			else
			{
				if (AllValueState != Section.bCastShadow)
				{
					AllValueSame = false;
					break;
				}
			}
		}
	}

	State = AllValueSame ? (AllValueState ? ECheckBoxState::Checked : ECheckBoxState::Unchecked) : ECheckBoxState::Undetermined;
	return State;
}

void FPersonaMeshDetails::OnShadowCastingChanged(ECheckBoxState NewState, int32 MaterialIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();

	if (Mesh)
	{
		bool NewValue = false;
		if (NewState == ECheckBoxState::Checked)
		{
			const FScopedTransaction Transaction(LOCTEXT("SetShadowCastingFlag", "Set Shadow Casting For Material"));
			Mesh->Modify();
			NewValue = true;
		}
		else if (NewState == ECheckBoxState::Unchecked)
		{
			const FScopedTransaction Transaction(LOCTEXT("ClearShadowCastingFlag", "Clear Shadow Casting For Material"));
			Mesh->Modify();
			NewValue = false;
		}

		check(Mesh->GetResourceForRendering());
		for (int32 LODIdx = 0; LODIdx < Mesh->GetResourceForRendering()->LODModels.Num(); LODIdx++)
		{
			FStaticLODModel& LODModel = Mesh->GetResourceForRendering()->LODModels[LODIdx];
			for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); ++SectionIndex)
			{
				FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

				if (Section.MaterialIndex != MaterialIndex)
					continue;

				Section.bCastShadow = NewValue;
			}
		}

		for (TObjectIterator<USkinnedMeshComponent> It; It; ++It)
		{
			USkinnedMeshComponent* MeshComponent = *It;
			if (MeshComponent &&
				!MeshComponent->IsTemplate() &&
				MeshComponent->SkeletalMesh == Mesh)
			{
				MeshComponent->MarkRenderStateDirty();
			}
		}
		GetPersonaToolkit()->GetPreviewScene()->InvalidateViews();
	}
}


ECheckBoxState FPersonaMeshDetails::IsRecomputeTangentEnabled(int32 MaterialIndex) const
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	const USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();
	if (Mesh == nullptr)
		return State;

	bool FirstValueSet = false;
	bool AllValueState = false;
	bool AllValueSame = true;
	check(Mesh->GetResourceForRendering());
	for (int32 LODIdx = 0; LODIdx < Mesh->GetResourceForRendering()->LODModels.Num(); LODIdx++)
	{
		const FStaticLODModel& LODModel = Mesh->GetResourceForRendering()->LODModels[LODIdx];
		for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); ++SectionIndex)
		{
			const FSkelMeshSection& Section = LODModel.Sections[SectionIndex];
			
			if (Section.MaterialIndex != MaterialIndex)
				continue;

			if (!FirstValueSet)
			{
				FirstValueSet = true;
				AllValueState = Section.bRecomputeTangent;
				AllValueSame = true;
			}
			else
			{
				if (AllValueState != Section.bRecomputeTangent)
				{
					AllValueSame = false;
					break;
				}
			}
		}
	}

	State = AllValueSame ? (AllValueState ? ECheckBoxState::Checked : ECheckBoxState::Unchecked) : ECheckBoxState::Undetermined;

	return State;
}

void FPersonaMeshDetails::OnRecomputeTangentChanged(ECheckBoxState NewState, int32 MaterialIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();

	if (Mesh)
	{
		bool NewValue = false;
		if (NewState == ECheckBoxState::Checked)
		{
			const FScopedTransaction Transaction(LOCTEXT("SetRecomputeTangentFlag", "Set Recompute Tangent For Material"));
			Mesh->Modify();
			NewValue = true;
		}
		else if (NewState == ECheckBoxState::Unchecked)
		{
			const FScopedTransaction Transaction(LOCTEXT("ClearRecomputeTangentFlag", "Clear Recompute Tangent For Material"));
			Mesh->Modify();
			NewValue = false;
		}

		for (TObjectIterator<USkinnedMeshComponent> It; It; ++It)
		{
			USkinnedMeshComponent* MeshComponent = *It;
			if (MeshComponent &&
				!MeshComponent->IsTemplate() &&
				MeshComponent->SkeletalMesh == Mesh)
			{
				MeshComponent->UpdateRecomputeTangent(MaterialIndex, INDEX_NONE, NewValue);
				MeshComponent->MarkRenderStateDirty();
			}
		}
		GetPersonaToolkit()->GetPreviewScene()->InvalidateViews();
	}
}

ECheckBoxState FPersonaMeshDetails::IsSectionShadowCastingEnabled(int32 LODIndex, int32 SectionIndex) const
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	const USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();
	if (Mesh == nullptr)
		return State;
	
	check(Mesh->GetResourceForRendering());

	if (!Mesh->GetResourceForRendering()->LODModels.IsValidIndex(LODIndex))
		return State;

	const FStaticLODModel& LODModel = Mesh->GetResourceForRendering()->LODModels[LODIndex];

	if (!LODModel.Sections.IsValidIndex(SectionIndex))
		return State;

	const FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

	State = Section.bCastShadow ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	return State;
}

void FPersonaMeshDetails::OnSectionShadowCastingChanged(ECheckBoxState NewState, int32 LODIndex, int32 SectionIndex)
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();
	if (Mesh == nullptr)
		return;

	check(Mesh->GetResourceForRendering());

	if (!Mesh->GetResourceForRendering()->LODModels.IsValidIndex(LODIndex))
		return;

	FStaticLODModel& LODModel = Mesh->GetResourceForRendering()->LODModels[LODIndex];

	if (!LODModel.Sections.IsValidIndex(SectionIndex))
		return;

	FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

	if (NewState == ECheckBoxState::Checked)
	{
		const FScopedTransaction Transaction(LOCTEXT("SetSectionShadowCastingFlag", "Set Shadow Casting For Section"));
		Mesh->Modify();
		Section.bCastShadow = true;
		}
	else if (NewState == ECheckBoxState::Unchecked)
	{
		const FScopedTransaction Transaction(LOCTEXT("ClearSectionShadowCastingFlag", "Clear Shadow Casting For Section"));
		Mesh->Modify();
		Section.bCastShadow = false;
	}
	
		for (TObjectIterator<USkinnedMeshComponent> It; It; ++It)
		{
			USkinnedMeshComponent* MeshComponent = *It;
			if (MeshComponent &&
				!MeshComponent->IsTemplate() &&
				MeshComponent->SkeletalMesh == Mesh)
			{
				MeshComponent->MarkRenderStateDirty();
			}
		}
		GetPersonaToolkit()->GetPreviewScene()->InvalidateViews();
	}

ECheckBoxState FPersonaMeshDetails::IsSectionRecomputeTangentEnabled(int32 LODIndex, int32 SectionIndex) const
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	const USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();
	if (Mesh == nullptr)
		return State;

	check(Mesh->GetResourceForRendering());

	if (!Mesh->GetResourceForRendering()->LODModels.IsValidIndex(LODIndex))
		return State;

	const FStaticLODModel& LODModel = Mesh->GetResourceForRendering()->LODModels[LODIndex];

	if (!LODModel.Sections.IsValidIndex(SectionIndex))
		return State;

	const FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

	State = Section.bRecomputeTangent ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	return State;
}

void FPersonaMeshDetails::OnSectionRecomputeTangentChanged(ECheckBoxState NewState, int32 LODIndex, int32 SectionIndex)
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();
	if (Mesh == nullptr)
		return;

	check(Mesh->GetResourceForRendering());

	if (!Mesh->GetResourceForRendering()->LODModels.IsValidIndex(LODIndex))
		return;

	FStaticLODModel& LODModel = Mesh->GetResourceForRendering()->LODModels[LODIndex];

	if (!LODModel.Sections.IsValidIndex(SectionIndex))
		return;

	FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

	bool NewRecomputeTangentValue = false;
	if (NewState == ECheckBoxState::Checked)
	{
		const FScopedTransaction Transaction(LOCTEXT("SetSectionShadowCastingFlag", "Set Shadow Casting For Section"));
		Mesh->Modify();
		NewRecomputeTangentValue = true;
	}
	else if (NewState == ECheckBoxState::Unchecked)
	{
		const FScopedTransaction Transaction(LOCTEXT("ClearSectionShadowCastingFlag", "Clear Shadow Casting For Section"));
		Mesh->Modify();
		NewRecomputeTangentValue = false;
	}

	for (TObjectIterator<USkinnedMeshComponent> It; It; ++It)
	{
		USkinnedMeshComponent* MeshComponent = *It;
		if (MeshComponent &&
			!MeshComponent->IsTemplate() &&
			MeshComponent->SkeletalMesh == Mesh)
		{
			MeshComponent->UpdateRecomputeTangent(SectionIndex, LODIndex, NewRecomputeTangentValue);
			MeshComponent->MarkRenderStateDirty();
		}
	}
	GetPersonaToolkit()->GetPreviewScene()->InvalidateViews();
}

EVisibility FPersonaMeshDetails::GetOverrideUVDensityVisibililty() const
{
	if (/*GetViewMode() == VMI_MeshUVDensityAccuracy*/ true)
	{
		return EVisibility::SelfHitTestInvisible;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

ECheckBoxState FPersonaMeshDetails::IsUVDensityOverridden(int32 MaterialIndex) const
{
	USkeletalMesh* Mesh = SkeletalMeshPtr.Get();
	if (!Mesh || !Mesh->Materials.IsValidIndex(MaterialIndex))
	{
		return ECheckBoxState::Undetermined;
	}
	else if (Mesh->Materials[MaterialIndex].UVChannelData.bOverrideDensities)
	{
		return ECheckBoxState::Checked;
	}
	else
	{
		return ECheckBoxState::Unchecked;
	}
}


void FPersonaMeshDetails::OnOverrideUVDensityChanged(ECheckBoxState NewState, int32 MaterialIndex)
{
	USkeletalMesh* Mesh = SkeletalMeshPtr.Get();
	if (NewState != ECheckBoxState::Undetermined && Mesh && Mesh->Materials.IsValidIndex(MaterialIndex))
	{
		Mesh->Materials[MaterialIndex].UVChannelData.bOverrideDensities = (NewState == ECheckBoxState::Checked);
		Mesh->UpdateUVChannelData(true);
	}
}

EVisibility FPersonaMeshDetails::GetUVDensityVisibility(int32 MaterialIndex, int32 UVChannelIndex) const
{
	USkeletalMesh* Mesh = SkeletalMeshPtr.Get();
	if (/*MeshGetViewMode() == VMI_MeshUVDensityAccuracy && */ Mesh && IsUVDensityOverridden(MaterialIndex) == ECheckBoxState::Checked)
	{
		return EVisibility::SelfHitTestInvisible;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

TOptional<float> FPersonaMeshDetails::GetUVDensityValue(int32 MaterialIndex, int32 UVChannelIndex) const
{
	USkeletalMesh* Mesh = SkeletalMeshPtr.Get();
	if (Mesh && Mesh->Materials.IsValidIndex(MaterialIndex))
	{
		float Value = Mesh->Materials[MaterialIndex].UVChannelData.LocalUVDensities[UVChannelIndex];
		return FMath::RoundToFloat(Value * 4.f) * .25f;
	}
	return TOptional<float>();
}

void FPersonaMeshDetails::SetUVDensityValue(float InDensity, ETextCommit::Type CommitType, int32 MaterialIndex, int32 UVChannelIndex)
{
	USkeletalMesh* Mesh = SkeletalMeshPtr.Get();
	if (Mesh && Mesh->Materials.IsValidIndex(MaterialIndex))
	{
		Mesh->Materials[MaterialIndex].UVChannelData.LocalUVDensities[UVChannelIndex] = FMath::Max<float>(0, InDensity);
		Mesh->UpdateUVChannelData(true);
	}
}

int32 FPersonaMeshDetails::GetMaterialIndex(int32 LODIndex, int32 SectionIndex) const
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();

	check(LODIndex < SkelMesh->LODInfo.Num());

	FSkeletalMeshLODInfo& Info = SkelMesh->LODInfo[LODIndex];
	if (LODIndex == 0 || Info.LODMaterialMap.Num() == 0 || SectionIndex >= Info.LODMaterialMap.Num())
	{
		FSkeletalMeshResource* ImportedResource = SkelMesh->GetImportedResource();
		check(ImportedResource && ImportedResource->LODModels.IsValidIndex(LODIndex));
		return ImportedResource->LODModels[LODIndex].Sections[SectionIndex].MaterialIndex;
	}
	else
	{
		return Info.LODMaterialMap[SectionIndex];
	}
}

bool FPersonaMeshDetails::IsDuplicatedMaterialIndex(int32 LODIndex, int32 MaterialIndex)
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();

	// finding whether this material index is being used in parent LODs
	for (int32 LODInfoIdx = 0; LODInfoIdx < LODIndex; LODInfoIdx++)
	{
		FSkeletalMeshLODInfo& Info = SkelMesh->LODInfo[LODInfoIdx];
		if (LODIndex == 0 || Info.LODMaterialMap.Num() == 0)
		{
			FSkeletalMeshResource* ImportedResource = SkelMesh->GetImportedResource();

			if (ImportedResource && ImportedResource->LODModels.IsValidIndex(LODInfoIdx))
			{
				FStaticLODModel& Model = ImportedResource->LODModels[LODInfoIdx];

				for (int32 SectionIdx = 0; SectionIdx < Model.Sections.Num(); SectionIdx++)
				{
					if (MaterialIndex == Model.Sections[SectionIdx].MaterialIndex)
					{
						return true;
					}
				}
			}
		}
		else // if LODMaterialMap exists
		{
			for (int32 MapIdx = 0; MapIdx < Info.LODMaterialMap.Num(); MapIdx++)
			{
				if (MaterialIndex == Info.LODMaterialMap[MapIdx])
				{
					return true;
				}
			}
		}
	}

	return false;
}

void FPersonaMeshDetails::OnSectionChanged(int32 LODIndex, int32 SectionIndex, int32 NewMaterialSlotIndex, FName NewMaterialSlotName)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();
	if(Mesh)
	{
		FSkeletalMeshResource* ImportedResource = Mesh->GetImportedResource();
		check(ImportedResource && ImportedResource->LODModels.IsValidIndex(LODIndex));
		const int32 TotalSectionCount = ImportedResource->LODModels[LODIndex].Sections.Num();

		check(TotalSectionCount > SectionIndex);

		int32 NewSkeletalMaterialIndex = INDEX_NONE;
		for (int SkeletalMaterialIndex = 0; SkeletalMaterialIndex < Mesh->Materials.Num(); ++SkeletalMaterialIndex)
		{
			if (NewMaterialSlotIndex == SkeletalMaterialIndex && Mesh->Materials[SkeletalMaterialIndex].MaterialSlotName == NewMaterialSlotName)
			{
				NewSkeletalMaterialIndex = SkeletalMaterialIndex;
				break;
			}
			}

		check(NewSkeletalMaterialIndex != INDEX_NONE);

		// Begin a transaction for undo/redo the first time we encounter a material to replace.  
		// There is only one transaction for all replacement
		FScopedTransaction Transaction(LOCTEXT("PersonaChangedMaterialIndex", "Change material index on mesh"));
		Mesh->Modify();

		FSkeletalMeshLODInfo& Info = Mesh->LODInfo[LODIndex];
		if (LODIndex == 0 || Info.LODMaterialMap.Num() == 0)
				{
			ImportedResource->LODModels[LODIndex].Sections[SectionIndex].MaterialIndex = NewSkeletalMaterialIndex;
				}
				else
				{
			check(SectionIndex < Info.LODMaterialMap.Num());
			Info.LODMaterialMap[SectionIndex] = NewSkeletalMaterialIndex;
				}

		Mesh->PostEditChange();

			// Redraw viewports to reflect the material changes 
			GUnrealEd->RedrawLevelEditingViewports();
		}
	}

#if WITH_APEX_CLOTHING

//
// Generate slate UI for Clothing category
//
void FPersonaMeshDetails::CustomizeClothingProperties(IDetailLayoutBuilder& DetailLayout, IDetailCategoryBuilder& ClothingFilesCategory)
{
	TSharedRef<IPropertyHandle> ClothingAssetsProperty = DetailLayout.GetProperty(FName("ClothingAssets"), USkeletalMesh::StaticClass());

	if( ClothingAssetsProperty->IsValidHandle() )
	{
		TSharedRef<FDetailArrayBuilder> ClothingAssetsPropertyBuilder = MakeShareable( new FDetailArrayBuilder( ClothingAssetsProperty ) );
		ClothingAssetsPropertyBuilder->OnGenerateArrayElementWidget( FOnGenerateArrayElementWidget::CreateSP( this, &FPersonaMeshDetails::OnGenerateElementForClothingAsset, &DetailLayout) );

		ClothingFilesCategory.AddCustomBuilder(ClothingAssetsPropertyBuilder, false);
	}

	// Button to add a new clothing file
	ClothingFilesCategory.AddCustomRow( LOCTEXT("AddAPEXClothingFileFilterString", "Add APEX clothing file"))
	[
		SNew(SHorizontalBox)
		 
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.OnClicked(this, &FPersonaMeshDetails::OnOpenClothingFileClicked, &DetailLayout)
			.Text(LOCTEXT("AddAPEXClothingFile", "Add APEX clothing file..."))
			.ToolTip(IDocumentation::Get()->CreateToolTip(
				LOCTEXT("AddClothingButtonTooltip", "Select a new APEX clothing file and add it to the skeletal mesh."),
				NULL,
				TEXT("Shared/Editors/Persona"),
				TEXT("AddClothing")))
		]
	];
}

//
// Generate each ClothingAsset array entry
//
void FPersonaMeshDetails::OnGenerateElementForClothingAsset( TSharedRef<IPropertyHandle> StructProperty, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder, IDetailLayoutBuilder* DetailLayout )
{
	const FSlateFontInfo DetailFontInfo = IDetailLayoutBuilder::GetDetailFont();

	// Remove and reimport asset buttons
	ChildrenBuilder.AddChildContent( FText::GetEmpty() ) 
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1)

		// re-import button
		+ SHorizontalBox::Slot()
		.VAlign( VAlign_Center )
		.Padding(2)
		.AutoWidth()
		[
			SNew( SButton )
			.Text( LOCTEXT("ReimportButtonLabel", "Reimport") )
			.OnClicked(this, &FPersonaMeshDetails::OnReimportApexFileClicked, ElementIndex, DetailLayout)
			.IsFocusable( false )
			.ContentPadding(0)
			.ForegroundColor( FSlateColor::UseForeground() )
			.ButtonColorAndOpacity(FLinearColor(1.0f,1.0f,1.0f,0.0f))
			.ToolTipText(LOCTEXT("ReimportApexFileTip", "Reimport this APEX asset"))
			[ 
				SNew( SImage )
				.Image( FEditorStyle::GetBrush("Persona.ReimportAsset") )
				.ColorAndOpacity( FSlateColor::UseForeground() )
			]
		]

		// remove button
		+ SHorizontalBox::Slot()
		.VAlign( VAlign_Center )
		.Padding(2)
		.AutoWidth()
		[
			SNew( SButton )
			.Text( LOCTEXT("ClearButtonLabel", "Remove") )
			.OnClicked( this, &FPersonaMeshDetails::OnRemoveApexFileClicked, ElementIndex, DetailLayout )
			.IsFocusable( false )
			.ContentPadding(0)
			.ForegroundColor( FSlateColor::UseForeground() )
			.ButtonColorAndOpacity(FLinearColor(1.0f,1.0f,1.0f,0.0f))
			.ToolTipText(LOCTEXT("RemoveApexFileTip", "Remove this APEX asset"))
			[ 
				SNew( SImage )
				.Image( FEditorStyle::GetBrush("PropertyWindow.Button_Clear") )
				.ColorAndOpacity( FSlateColor::UseForeground() )
			]
		]
	];	

	// Asset Name
	TSharedRef<IPropertyHandle> AssetNameProperty = StructProperty->GetChildHandle(FName("AssetName")).ToSharedRef();
	FSimpleDelegate UpdateComboBoxStringsDelegate = FSimpleDelegate::CreateSP(this, &FPersonaMeshDetails::UpdateComboBoxStrings);
	AssetNameProperty->SetOnPropertyValueChanged(UpdateComboBoxStringsDelegate);
	ChildrenBuilder.AddChildProperty(AssetNameProperty);

	// APEX Details
	TSharedRef<IPropertyHandle> ApexFileNameProperty = StructProperty->GetChildHandle(FName("ApexFileName")).ToSharedRef();
	ChildrenBuilder.AddChildProperty(ApexFileNameProperty)
	.CustomWidget()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("APEXDetails", "APEX Details"))
		.Font(DetailFontInfo)
	]
	.ValueContent()
	.HAlign(HAlign_Fill)
	[
		MakeApexDetailsWidget(ElementIndex)
	];	
	
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();
	FClothingAssetData& AssetData = SkelMesh->ClothingAssets[ElementIndex];

	ApexClothingUtils::GetPhysicsPropertiesFromApexAsset(AssetData.ApexClothingAsset, AssetData.PhysicsProperties);

	// cloth physics properties
	TSharedRef<IPropertyHandle> ClothPhysicsProperties = StructProperty->GetChildHandle(FName("PhysicsProperties")).ToSharedRef();
	uint32 NumChildren;
	ClothPhysicsProperties->GetNumChildren(NumChildren);
	FSimpleDelegate UpdatePhysicsPropertyDelegate = FSimpleDelegate::CreateSP(this, &FPersonaMeshDetails::UpdateClothPhysicsProperties, ElementIndex);
	for (uint32 ChildIdx = 0; ChildIdx < NumChildren; ChildIdx++)
	{
		ClothPhysicsProperties->GetChildHandle(ChildIdx)->SetOnPropertyValueChanged(UpdatePhysicsPropertyDelegate);
	}

	ChildrenBuilder.AddChildProperty(ClothPhysicsProperties);
}

TSharedRef<SUniformGridPanel> FPersonaMeshDetails::MakeApexDetailsWidget(int32 AssetIndex) const
{
	const FSlateFontInfo DetailFontInfo = IDetailLayoutBuilder::GetDetailFont();

	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();

	FClothingAssetData& Asset = SkelMesh->ClothingAssets[AssetIndex];

	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(2.0f);

	// temporarily while removing
	if(!SkelMesh->ClothingAssets.IsValidIndex(AssetIndex))
	{
		Grid->AddSlot(0, 0) // x, y
			.HAlign(HAlign_Fill)
			[
				SNew(STextBlock)
				.Font(DetailFontInfo)
				.Text(LOCTEXT("Removing", "Removing..."))
			];

		return Grid;
	}

	int32 NumLODs = ApexClothingUtils::GetNumLODs(Asset.ApexClothingAsset);
	int32 RowNumber = 0;

	for(int32 LODIndex=0; LODIndex < NumLODs; LODIndex++)
	{
		Grid->AddSlot(0, RowNumber) // x, y
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Font(DetailFontInfo)
			.Text(FText::Format(LOCTEXT("LODIndex", "LOD {0}"), FText::AsNumber(LODIndex)))			
		];

		RowNumber++;

		TArray<FSubmeshInfo> SubmeshInfos;
		if(ApexClothingUtils::GetSubmeshInfoFromApexAsset(Asset.ApexClothingAsset, LODIndex, SubmeshInfos))
		{
			// content names
			Grid->AddSlot(0, RowNumber) // x, y
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(DetailFontInfo)
				.Text(LOCTEXT("SubmeshIndex", "Submesh"))
			];

			Grid->AddSlot(1, RowNumber) 
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(DetailFontInfo)
				.Text(LOCTEXT("SimulVertexCount", "Simul Verts"))
			];

			Grid->AddSlot(2, RowNumber) 
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(DetailFontInfo)
				.Text(LOCTEXT("RenderVertexCount", "Render Verts"))
			];

			Grid->AddSlot(3, RowNumber)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(DetailFontInfo)
				.Text(LOCTEXT("FixedVertexCount", "Fixed Verts"))
			];

			Grid->AddSlot(4, RowNumber)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(DetailFontInfo)
				.Text(LOCTEXT("TriangleCount", "Triangles"))
			];

			Grid->AddSlot(5, RowNumber)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(DetailFontInfo)
				.Text(LOCTEXT("NumUsedBones", "Bones"))
			];

			Grid->AddSlot(6, RowNumber)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(DetailFontInfo)
				.Text(LOCTEXT("NumBoneSpheres", "Spheres"))
			];

			RowNumber++;

			for(int32 i=0; i < SubmeshInfos.Num(); i++)
			{
				Grid->AddSlot(0, RowNumber)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Font(DetailFontInfo)
					.Text(FText::AsNumber(SubmeshInfos[i].SubmeshIndex))
				];

				if(i == 0)
				{
					Grid->AddSlot(1, RowNumber)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Font(DetailFontInfo)
						.Text(FText::AsNumber(SubmeshInfos[i].SimulVertexCount))
					];
				}
				else
				{
					Grid->AddSlot(1, RowNumber)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Font(DetailFontInfo)
						.Text(LOCTEXT("SharedLabel", "Shared"))
					];
				}

				Grid->AddSlot(2, RowNumber)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Font(DetailFontInfo)
						.Text(FText::AsNumber(SubmeshInfos[i].VertexCount))
					];

				Grid->AddSlot(3, RowNumber)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Font(DetailFontInfo)
					.Text(FText::AsNumber(SubmeshInfos[i].FixedVertexCount))
				];

				Grid->AddSlot(4, RowNumber)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Font(DetailFontInfo)
					.Text(FText::AsNumber(SubmeshInfos[i].TriangleCount))
				];

				Grid->AddSlot(5, RowNumber)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Font(DetailFontInfo)
					.Text(FText::AsNumber(SubmeshInfos[i].NumUsedBones))
				];

				Grid->AddSlot(6, RowNumber)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Font(DetailFontInfo)
					.Text(FText::AsNumber(SubmeshInfos[i].NumBoneSpheres))
				];

				RowNumber++;
			}
		}
		else
		{
			Grid->AddSlot(0, RowNumber) // x, y
			.HAlign(HAlign_Fill)
			[
				SNew(STextBlock)
				.Font(DetailFontInfo)
				.Text(LOCTEXT("FailedGetSubmeshInfo", "Failed to get sub-mesh Info"))
			];

			RowNumber++;
		}
	}

	return Grid;
}

FReply FPersonaMeshDetails::OnReimportApexFileClicked(int32 AssetIndex, IDetailLayoutBuilder* DetailLayout)
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();

	check(SkelMesh);

	FString FileName(SkelMesh->ClothingAssets[AssetIndex].ApexFileName);

	bool bNeedToLeaveProperties = false;
	FClothPhysicsProperties CurClothPhysicsProperties;

	// if this asset's cloth properties were changed in UE4 editor, then shows a warning dialog
	if (SkelMesh->ClothingAssets[AssetIndex].bClothPropertiesChanged)
	{
		const FText Text = LOCTEXT("Warning_ClothPropertiesChanged", "This asset's physics properties have been modified inside UE4. \nWould you like to reapply the modified property values after reimporting?");
		EAppReturnType::Type Ret = FMessageDialog::Open(EAppMsgType::YesNoCancel, Text);
		if (EAppReturnType::Yes == Ret)
		{
			bNeedToLeaveProperties = true;
			ApexClothingUtils::GetPhysicsPropertiesFromApexAsset(SkelMesh->ClothingAssets[AssetIndex].ApexClothingAsset, CurClothPhysicsProperties);
		}
		else if (EAppReturnType::Cancel == Ret)
		{
			return FReply::Handled();
		}
	}
	ApexClothingUtils::EClothUtilRetType RetType = ApexClothingUtils::ImportApexAssetFromApexFile(FileName, SkelMesh, AssetIndex);
	switch(RetType)
	{
	case ApexClothingUtils::CURT_Ok:
		UpdateComboBoxStrings();
		if (bNeedToLeaveProperties)
		{
			// overwrites changed values instead of original values
			SkelMesh->ClothingAssets[AssetIndex].bClothPropertiesChanged = true;
			ApexClothingUtils::SetPhysicsPropertiesToApexAsset(SkelMesh->ClothingAssets[AssetIndex].ApexClothingAsset, CurClothPhysicsProperties);
		}
		else
		{
			// will lose all changed values through UE4 editor
			SkelMesh->ClothingAssets[AssetIndex].bClothPropertiesChanged = false;
		}

		DetailLayout->ForceRefreshDetails();

		break;
	case ApexClothingUtils::CURT_Fail:
		// Failed to create or import
		FMessageDialog::Open( EAppMsgType::Ok, FText::Format( LOCTEXT("ReimportFailed", "Failed to re-import APEX clothing asset from {0}."), FText::FromString( FileName ) ) );
		break;
	case ApexClothingUtils::CURT_Cancel:
		break;
	}

	return FReply::Handled();
}

FReply FPersonaMeshDetails::OnRemoveApexFileClicked(int32 AssetIndex, IDetailLayoutBuilder* DetailLayout)
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();

	// SkeMesh->ClothingAssets.RemoveAt(AssetIndex) is conducted inside RemoveAssetFromSkeletalMesh
	ApexClothingUtils::RemoveAssetFromSkeletalMesh(SkelMesh, AssetIndex, true);

	// Force layout to refresh
	DetailLayout->ForceRefreshDetails();

	// Update the combo-box
	UpdateComboBoxStrings();

	return FReply::Handled();
}

FReply FPersonaMeshDetails::OnOpenClothingFileClicked(IDetailLayoutBuilder* DetailLayout)
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();

	if( ensure(SkelMesh) )
	{
		FString Filename;
		FName AssetName;

		// Display open dialog box
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		if ( DesktopPlatform )
		{
			const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

			TArray<FString> OpenFilenames;	
			FString OpenFilePath = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::MESH_IMPORT_EXPORT);

			if( DesktopPlatform->OpenFileDialog(
				ParentWindowWindowHandle,
				*LOCTEXT("Persona_ChooseAPEXFile", "Choose file to load APEX clothing asset").ToString(), 
				OpenFilePath,
				TEXT(""), 
				TEXT("APEX clothing asset(*.apx,*.apb)|*.apx;*.apb|All files (*.*)|*.*"),
				EFileDialogFlags::None,
				OpenFilenames
				) )
			{
				FEditorDirectories::Get().SetLastDirectory(ELastDirectory::MESH_IMPORT_EXPORT, OpenFilenames[0]);

				ApexClothingUtils::EClothUtilRetType RetType = ApexClothingUtils::ImportApexAssetFromApexFile( OpenFilenames[0], SkelMesh, -1);

				if(RetType  == ApexClothingUtils::CURT_Ok)
				{
					int32 AssetIndex = SkelMesh->ClothingAssets.Num()-1;
					FClothingAssetData& AssetData = SkelMesh->ClothingAssets[AssetIndex];
					int32 NumLODs = ApexClothingUtils::GetNumLODs(AssetData.ApexClothingAsset);

					uint32 MaxClothVertices = ApexClothingUtils::GetMaxClothSimulVertices(GMaxRHIFeatureLevel);

					// check whether there are sub-meshes which have over MaxClothVertices or not
					// this checking will be removed after changing implementation way for supporting over MaxClothVertices
					uint32 NumSubmeshesOverMaxSimulVerts = 0;

					for(int32 LODIndex=0; LODIndex < NumLODs; LODIndex++)
					{
						TArray<FSubmeshInfo> SubmeshInfos;
						if(ApexClothingUtils::GetSubmeshInfoFromApexAsset(AssetData.ApexClothingAsset, LODIndex, SubmeshInfos))
						{
							for(int32 SubIndex=0; SubIndex < SubmeshInfos.Num(); SubIndex++)
							{
								if(SubmeshInfos[SubIndex].SimulVertexCount >= MaxClothVertices)
								{
									NumSubmeshesOverMaxSimulVerts++;
								}
							}
						}
					}

					// shows warning 
					if(NumSubmeshesOverMaxSimulVerts > 0)
					{
						const FText Text = FText::Format(LOCTEXT("Warning_OverMaxClothSimulVerts", "{0} submeshes have over {1} cloth simulation vertices. Please reduce simulation vertices under {1}. It has possibility not to work correctly.\n\n Proceed?"), FText::AsNumber(NumSubmeshesOverMaxSimulVerts), FText::AsNumber(MaxClothVertices));
						if (EAppReturnType::Ok != FMessageDialog::Open(EAppMsgType::OkCancel, Text))
						{
							ApexClothingUtils::RemoveAssetFromSkeletalMesh(SkelMesh, AssetIndex, true);
							return FReply::Handled();
						}
					}

					// show import option dialog if this asset has multiple LODs 
					if(NumLODs > 1)
					{
						TSharedPtr<SWindow> ParentWindow;
						// Check if the main frame is loaded.  When using the old main frame it may not be.
						if( FModuleManager::Get().IsModuleLoaded( "MainFrame" ) )
						{
							IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" );
							ParentWindow = MainFrame.GetParentWindow();
						}

						TSharedRef<SWindow> Window = SNew(SWindow)
							.Title(LOCTEXT("ClothImportInfo", "Clothing Import Information"))
							.SizingRule( ESizingRule::Autosized );

						TSharedPtr<SApexClothingOptionWindow> ClothingOptionWindow;
						Window->SetContent
							(
							SAssignNew(ClothingOptionWindow, SApexClothingOptionWindow)
								.WidgetWindow(Window)
								.NumLODs(NumLODs)
								.ApexDetails(MakeApexDetailsWidget(AssetIndex))
							);

						FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

						if(!ClothingOptionWindow->CanImport())
						{
							ApexClothingUtils::RemoveAssetFromSkeletalMesh(SkelMesh, AssetIndex, true);
						}
					}

					DetailLayout->ForceRefreshDetails();

					UpdateComboBoxStrings();
				}
				else
				if(RetType == ApexClothingUtils::CURT_Fail)
				{
					// Failed to create or import
					FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("ImportFailed", "Failed to import APEX clothing asset from the selected file.") );
				}
			}
		}
	}

	return FReply::Handled();
}

void FPersonaMeshDetails::UpdateComboBoxStrings()
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();

	if (!SkelMesh)
	{
		return;
	}

	int32 NumLODs = SkelMesh->LODInfo.Num();

	while (ClothingComboLODInfos.Num() < NumLODs)
	{
		new (ClothingComboLODInfos)FClothingComboInfo;
	}

	for (int32 LODIdx = 0; LODIdx < NumLODs; LODIdx++)
	{
		FClothingComboInfo& NewLODInfo = ClothingComboLODInfos[LODIdx];

		TArray<TSharedPtr< STextComboBox >>& ClothingComboBoxes = NewLODInfo.ClothingComboBoxes;
		TArray<TSharedPtr<FString> >& ClothingComboStrings = NewLODInfo.ClothingComboStrings;
		TMap<FString, FClothAssetSubmeshIndex>&	ClothingComboStringReverseLookup = NewLODInfo.ClothingComboStringReverseLookup;
		TArray<int32>& ClothingComboSelectedIndices = NewLODInfo.ClothingComboSelectedIndices;

		// Clear out old strings
		ClothingComboStrings.Empty();
		ClothingComboStringReverseLookup.Empty();
		ClothingComboStrings.Add(MakeShareable(new FString(TEXT("None"))));

		FStaticLODModel& LODModel = SkelMesh->GetImportedResource()->LODModels[LODIdx];

		// # of non clothing sections means # of slots
		int32 NumNonClothingSections = LODModel.NumNonClothingSections();

		TArray<FClothAssetSubmeshIndex> SectionAssetSubmeshIndices;

		ClothingComboSelectedIndices.Empty(NumNonClothingSections);
		SectionAssetSubmeshIndices.Empty(NumNonClothingSections);

		for (int32 SecIdx = 0; SecIdx < NumNonClothingSections; SecIdx++)
		{
			ClothingComboSelectedIndices.Add(0); // None
			new(SectionAssetSubmeshIndices)FClothAssetSubmeshIndex(INDEX_NONE, INDEX_NONE);
		}

		// Setup Asset's sub-mesh indices
		for (int32 SecIdx = 0; SecIdx < NumNonClothingSections; SecIdx++)
		{
			int32 ClothSection = LODModel.Sections[SecIdx].CorrespondClothSectionIndex;
			if (ClothSection >= 0)
			{
				FSkelMeshSection& Section = LODModel.Sections[ClothSection];
				SectionAssetSubmeshIndices[SecIdx] = FClothAssetSubmeshIndex(Section.CorrespondClothAssetIndex, Section.ClothAssetSubmeshIndex);
			}
		}

		for (int32 AssetIndex = 0; AssetIndex < SkelMesh->ClothingAssets.Num(); AssetIndex++)
		{
			FClothingAssetData& ClothingAssetData = SkelMesh->ClothingAssets[AssetIndex];

			TArray<FSubmeshInfo> SubmeshInfos;
			// if failed to get sub-mesh info, then skip
			if (!ApexClothingUtils::GetSubmeshInfoFromApexAsset(ClothingAssetData.ApexClothingAsset, LODIdx, SubmeshInfos))
			{
				continue;
			}

			if (SubmeshInfos.Num() == 1)
			{
				FString ComboString = ClothingAssetData.AssetName.ToString();
				ClothingComboStrings.Add(MakeShareable(new FString(ComboString)));
				FClothAssetSubmeshIndex& AssetSubmeshIndex = ClothingComboStringReverseLookup.Add(ComboString, FClothAssetSubmeshIndex(AssetIndex, SubmeshInfos[0].SubmeshIndex)); //submesh index is not always same as ordered index

				for (int32 SlotIdx = 0; SlotIdx < NumNonClothingSections; SlotIdx++)
				{
					if (SectionAssetSubmeshIndices[SlotIdx] == AssetSubmeshIndex)
					{
						ClothingComboSelectedIndices[SlotIdx] = ClothingComboStrings.Num() - 1;
					}
				}
			}
			else
			{
				for (int32 SubIdx = 0; SubIdx < SubmeshInfos.Num(); SubIdx++)
				{
					FString ComboString = FText::Format(LOCTEXT("Persona_ApexClothingAsset_Submesh", "{0} submesh {1}"), FText::FromName(ClothingAssetData.AssetName), FText::AsNumber(SubmeshInfos[SubIdx].SubmeshIndex)).ToString(); //submesh index is not always same as SubIdx
					ClothingComboStrings.Add(MakeShareable(new FString(ComboString)));
					FClothAssetSubmeshIndex& AssetSubmeshIndex = ClothingComboStringReverseLookup.Add(ComboString, FClothAssetSubmeshIndex(AssetIndex, SubIdx));

					for (int32 SlotIdx = 0; SlotIdx < NumNonClothingSections; SlotIdx++)
					{
						if (SectionAssetSubmeshIndices[SlotIdx] == AssetSubmeshIndex)
						{
							ClothingComboSelectedIndices[SlotIdx] = NewLODInfo.ClothingComboStrings.Num() - 1;
						}
					}
				}
			}
		}
		// Cause combo boxes to refresh their strings lists and selected value
		for (int32 i = 0; i < NewLODInfo.ClothingComboBoxes.Num(); i++)
		{
			ClothingComboBoxes[i]->RefreshOptions();
			ClothingComboBoxes[i]->SetSelectedItem(ClothingComboSelectedIndices.IsValidIndex(i) && ClothingComboStrings.IsValidIndex(ClothingComboSelectedIndices[i]) ? ClothingComboStrings[ClothingComboSelectedIndices[i]] : ClothingComboStrings[0]);
		}
	}


#if 0
	if(SkelMesh)
	{
		int32 NumMaterials = SkelMesh->Materials.Num();

		TArray<FClothAssetSubmeshIndex> SectionAssetSubmeshIndices;
		// Num of materials = slot size
		ClothingComboSelectedIndices.Empty(NumMaterials);
		SectionAssetSubmeshIndices.Empty(NumMaterials);

		// Initialize Indices
		for( int32 SlotIdx=0; SlotIdx<NumMaterials; SlotIdx++ )
		{
			ClothingComboSelectedIndices.Add(0); // None
			new(SectionAssetSubmeshIndices) FClothAssetSubmeshIndex(-1, -1);
		}

		FStaticLODModel& LODModel = SkelMesh->GetImportedResource()->LODModels[0];
		// Current mappings for each section
		int32 NumNonClothingSections = LODModel.NumNonClothingSections();

		// Setup Asset's sub-mesh indices
		for( int32 SectionIdx=0; SectionIdx<NumNonClothingSections; SectionIdx++ )
		{
			int32 ClothSection = LODModel.Sections[SectionIdx].CorrespondClothSectionIndex;
			if( ClothSection >= 0 )
			{
				// Material index is Slot index
				int32 SlotIdx = LODModel.Sections[SectionIdx].MaterialIndex;
				check(SlotIdx < SectionAssetSubmeshIndices.Num());
				FSkelMeshSection& Section = LODModel.Sections[ClothSection];
				SectionAssetSubmeshIndices[SlotIdx] = FClothAssetSubmeshIndex(Section.CorrespondClothAssetIndex, Section.ClothAssetSubmeshIndex);
			}
		}

		for( int32 AssetIndex=0;AssetIndex<SkelMesh->ClothingAssets.Num();AssetIndex++ )
		{
			FClothingAssetData& ClothingAssetData = SkelMesh->ClothingAssets[AssetIndex];

			TArray<FSubmeshInfo> SubmeshInfos;
			// if failed to get sub-mesh info, then skip
			if(!ApexClothingUtils::GetSubmeshInfoFromApexAsset(ClothingAssetData.ApexClothingAsset->GetAsset(), 0, SubmeshInfos))
			{
				continue;
			}

			if( SubmeshInfos.Num() == 1 )
			{
				FString ComboString = ClothingAssetData.AssetName.ToString();
				ClothingComboStrings.Add(MakeShareable(new FString(ComboString)));
				FClothAssetSubmeshIndex& AssetSubmeshIndex = ClothingComboStringReverseLookup.Add(ComboString, FClothAssetSubmeshIndex(AssetIndex, SubmeshInfos[0].SubmeshIndex)); //submesh index is not always same as ordered index

				for( int32 SlotIdx=0;SlotIdx<NumMaterials;SlotIdx++ )
				{
					if( SectionAssetSubmeshIndices[SlotIdx] == AssetSubmeshIndex )
					{
						ClothingComboSelectedIndices[SlotIdx] = ClothingComboStrings.Num()-1;
					}
				}
			}
			else
			{
				for( int32 SubIdx=0;SubIdx<SubmeshInfos.Num();SubIdx++ )
				{			
					FString ComboString = FText::Format( LOCTEXT("Persona_ApexClothingAsset_Submesh", "{0} submesh {1}"), FText::FromName(ClothingAssetData.AssetName), FText::AsNumber(SubmeshInfos[SubIdx].SubmeshIndex) ).ToString(); //submesh index is not always same as SubIdx
					ClothingComboStrings.Add(MakeShareable(new FString(ComboString)));
					FClothAssetSubmeshIndex& AssetSubmeshIndex = ClothingComboStringReverseLookup.Add(ComboString, FClothAssetSubmeshIndex(AssetIndex, SubIdx));

					for( int32 SlotIdx=0;SlotIdx<NumMaterials;SlotIdx++ )
					{
						if( SectionAssetSubmeshIndices[SlotIdx] == AssetSubmeshIndex )
						{
							ClothingComboSelectedIndices[SlotIdx] = ClothingComboStrings.Num()-1;
						}
					}
				}
			}
		}
	}

	// Cause combo boxes to refresh their strings lists and selected value
	for(int32 i=0; i < ClothingComboBoxes.Num(); i++)
	{
		ClothingComboBoxes[i]->RefreshOptions();
		ClothingComboBoxes[i]->SetSelectedItem(ClothingComboSelectedIndices.IsValidIndex(i) && ClothingComboStrings.IsValidIndex(ClothingComboSelectedIndices[i]) ? ClothingComboStrings[ClothingComboSelectedIndices[i]] : ClothingComboStrings[0]);
	}
#endif
}

EVisibility FPersonaMeshDetails::IsClothingComboBoxVisible(int32 LODIndex, int32 SectionIndex) const
{
	// if this material doesn't have correspondence with any mesh section, hide this combo box 
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();

	// ClothingComboStrings.Num() should be greater than 1 because it is including "None"
	bool bExistClothingLOD = ClothingComboLODInfos.IsValidIndex(LODIndex) && ClothingComboLODInfos[LODIndex].ClothingComboStrings.Num() > 1;

	return SkelMesh && SkelMesh->ClothingAssets.Num() > 0 && bExistClothingLOD ? EVisibility::Visible : EVisibility::Collapsed;
}

FString FPersonaMeshDetails::HandleSectionsComboBoxGetRowText( TSharedPtr<FString> Section, int32 LODIndex, int32 SlotIndex)
{
	return *Section;
}

void FPersonaMeshDetails::HandleSectionsComboBoxSelectionChanged( TSharedPtr<FString> SelectedItem, ESelectInfo::Type SelectInfo, int32 LODIndex, int32 SectionIndex )
{		
	// Selection set by code shouldn't make any content changes.
	if( SelectInfo == ESelectInfo::Direct )
	{
		return;
	}

	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();

	FClothAssetSubmeshIndex* ComboItem;

	TArray<TSharedPtr< STextComboBox >>& ComboBoxes = ClothingComboLODInfos[LODIndex].ClothingComboBoxes;
	TArray<TSharedPtr<FString> >& ComboStrings = ClothingComboLODInfos[LODIndex].ClothingComboStrings;
	TMap<FString, FClothAssetSubmeshIndex>&	ComboStringReverseLookup = ClothingComboLODInfos[LODIndex].ClothingComboStringReverseLookup;
	TArray<int32>& ComboSelectedIndices = ClothingComboLODInfos[LODIndex].ClothingComboSelectedIndices;

	// Look up the item
	if (SelectedItem.IsValid() && (ComboItem = ComboStringReverseLookup.Find(*SelectedItem)) != NULL)
	{
		if( !ApexClothingUtils::ImportClothingSectionFromClothingAsset(
			SkelMesh,
			LODIndex,
			SectionIndex,
			ComboItem->AssetIndex,
			ComboItem->SubmeshIndex) )
		{
			// Failed.
			FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("ImportClothingSectionFailed", "An error occurred using this clothing asset for this skeletal mesh material section.") );
			// Change back to previously selected one.
			int32 SelectedIndex = ComboSelectedIndices.IsValidIndex(SectionIndex) ? ComboSelectedIndices[SectionIndex] : 0;
			ComboBoxes[SectionIndex]->SetSelectedItem(ComboStrings[SelectedIndex]);
		}
		else
		{
			// when clothing is mapped, need to re-prepare cloth morph targets
			if (GetPersonaToolkit()->GetPreviewMeshComponent())
			{
				GetPersonaToolkit()->GetPreviewMeshComponent()->bPreparedClothMorphTargets = false;
			}
		}
	}
	else
	{
		ApexClothingUtils::RestoreOriginalClothingSection(SkelMesh, LODIndex, SectionIndex);
	}
}

void FPersonaMeshDetails::UpdateClothPhysicsProperties(int32 AssetIndex)
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();
	FClothingAssetData& Asset = SkelMesh->ClothingAssets[AssetIndex];

	ApexClothingUtils::SetPhysicsPropertiesToApexAsset(Asset.ApexClothingAsset, Asset.PhysicsProperties);
	Asset.bClothPropertiesChanged = true;
}

#endif// #if WITH_APEX_CLOTHING

bool FPersonaMeshDetails::CanDeleteMaterialElement(int32 LODIndex, int32 SectionIndex) const
{
	// Only allow deletion of extra elements
	return SectionIndex != 0;
}

FReply FPersonaMeshDetails::OnDeleteButtonClicked(int32 LODIndex, int32 SectionIndex)
{
	ensure(SectionIndex != 0);

	int32 MaterialIndex = GetMaterialIndex(LODIndex, SectionIndex);

	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();

	// Move any mappings pointing to the requested material to point to the first
	// and decrement any above it
	if(SkelMesh)
	{

		const FScopedTransaction Transaction(LOCTEXT("DeleteMaterial", "Delete Material"));
		UProperty* MaterialProperty = FindField<UProperty>( USkeletalMesh::StaticClass(), "Materials" );
		SkelMesh->PreEditChange( MaterialProperty );

		// Patch up LOD mapping indices
		int32 NumLODInfos = SkelMesh->LODInfo.Num();
		for(int32 LODInfoIdx=0; LODInfoIdx < NumLODInfos; LODInfoIdx++)
		{
			for(auto LodMaterialIter = SkelMesh->LODInfo[LODInfoIdx].LODMaterialMap.CreateIterator() ; LodMaterialIter ; ++LodMaterialIter)
			{
				int32 CurrentMapping = *LodMaterialIter;

				if (CurrentMapping == MaterialIndex)
				{
					// Set to first material
					*LodMaterialIter = 0;
				}
				else if (CurrentMapping > MaterialIndex)
				{
					// Decrement to keep correct reference after removal
					*LodMaterialIter = CurrentMapping - 1;
				}
			}
		}
		
		// Patch up section indices
		for(auto ModelIter = SkelMesh->GetImportedResource()->LODModels.CreateIterator() ; ModelIter ; ++ModelIter)
		{
			FStaticLODModel& Model = *ModelIter;
			for(auto SectionIter = Model.Sections.CreateIterator() ; SectionIter ; ++SectionIter)
			{
				FSkelMeshSection& Section = *SectionIter;

				if (Section.MaterialIndex == MaterialIndex)
				{
					Section.MaterialIndex = 0;
				}
				else if (Section.MaterialIndex > MaterialIndex)
				{
					Section.MaterialIndex--;
				}
			}
		}

		SkelMesh->Materials.RemoveAt(MaterialIndex);

		// Notify the change in material
		FPropertyChangedEvent PropertyChangedEvent( MaterialProperty );
		SkelMesh->PostEditChangeProperty(PropertyChangedEvent);
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

