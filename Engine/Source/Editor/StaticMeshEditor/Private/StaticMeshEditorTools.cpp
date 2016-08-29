// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "StaticMeshEditorModule.h"
#include "StaticMeshEditorTools.h"
#include "RawMesh.h"
#include "MeshUtilities.h"
#include "TargetPlatform.h"
#include "StaticMeshResources.h"
#include "StaticMeshEditor.h"
#include "PropertyCustomizationHelpers.h"
#include "PhysicsEngine/BodySetup.h"
#include "FbxMeshUtils.h"
#include "SVectorInputBox.h"

#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "EngineAnalytics.h"
#include "STextComboBox.h"

const float MaxHullAccuracy = 1.f;
const float MinHullAccuracy = 0.f;
const float DefaultHullAccuracy = 0.5f;
const float HullAccuracyDelta = 0.01f;

const int32 MaxVertsPerHullCount = 32;
const int32 MinVertsPerHullCount = 6;
const int32 DefaultVertsPerHull = 16;

#define LOCTEXT_NAMESPACE "StaticMeshEditor"
DEFINE_LOG_CATEGORY_STATIC(LogStaticMeshEditorTools,Log,All);

FStaticMeshDetails::FStaticMeshDetails( class FStaticMeshEditor& InStaticMeshEditor )
	: StaticMeshEditor( InStaticMeshEditor )
{}

FStaticMeshDetails::~FStaticMeshDetails()
{
}

void FStaticMeshDetails::CustomizeDetails( class IDetailLayoutBuilder& DetailBuilder )
{
	IDetailCategoryBuilder& LODSettingsCategory = DetailBuilder.EditCategory( "LodSettings", LOCTEXT("LodSettingsCategory", "LOD Settings") );
	IDetailCategoryBuilder& StaticMeshCategory = DetailBuilder.EditCategory( "StaticMesh", LOCTEXT("StaticMeshGeneralSettings", "Static Mesh Settings") );
	IDetailCategoryBuilder& ImportCategory = DetailBuilder.EditCategory( "ImportSettings", LOCTEXT("ImportGeneralSettings", "Import Settings") );
	
	DetailBuilder.EditCategory( "Navigation", FText::GetEmpty(), ECategoryPriority::Uncommon );

	LevelOfDetailSettings = MakeShareable( new FLevelOfDetailSettingsLayout( StaticMeshEditor ) );

	LevelOfDetailSettings->AddToDetailsPanel( DetailBuilder );

	
	TSharedRef<IPropertyHandle> BodyProp = DetailBuilder.GetProperty("BodySetup");
	BodyProp->MarkHiddenByCustomization();

	static TArray<FName> HiddenBodyInstanceProps;

	if( HiddenBodyInstanceProps.Num() == 0 )
	{
		//HiddenBodyInstanceProps.Add("DefaultInstance");
		HiddenBodyInstanceProps.Add("BoneName");
		HiddenBodyInstanceProps.Add("PhysicsType");
		HiddenBodyInstanceProps.Add("bConsiderForBounds");
		HiddenBodyInstanceProps.Add("CollisionReponse");
	}

	uint32 NumChildren = 0;
	BodyProp->GetNumChildren( NumChildren );

	TSharedPtr<IPropertyHandle> BodyPropObject;

	if( NumChildren == 1 )
	{
		// This is an edit inline new property so the first child is the object instance for the edit inline new.  The instance contains the child we want to display
		BodyPropObject = BodyProp->GetChildHandle( 0 );

		NumChildren = 0;
		BodyPropObject->GetNumChildren( NumChildren );

		for( uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
		{
			TSharedPtr<IPropertyHandle> ChildProp = BodyPropObject->GetChildHandle(ChildIndex);
			if( ChildProp.IsValid() && ChildProp->GetProperty() && !HiddenBodyInstanceProps.Contains(ChildProp->GetProperty()->GetFName()) )
			{
				StaticMeshCategory.AddProperty( ChildProp );
			}
		}
	}
}

void SConvexDecomposition::Construct(const FArguments& InArgs)
{
	StaticMeshEditorPtr = InArgs._StaticMeshEditorPtr;

	CurrentHullAccuracy = DefaultHullAccuracy;
	CurrentMaxVertsPerHullCount = DefaultVertsPerHull;

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 16.0f, 0.0f, 8.0f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text( LOCTEXT("HullAccuracy_ConvexDecomp", "Accuracy") )
			]

			+SHorizontalBox::Slot()
			.FillWidth(3.0f)
			[
				SAssignNew(HullAccuracy, SSpinBox<float>)
				.MinValue(MinHullAccuracy)
				.MaxValue(MaxHullAccuracy)
				.Delta(HullAccuracyDelta)
				.Value( this, &SConvexDecomposition::GetHullAccuracy )
				.OnValueCommitted( this, &SConvexDecomposition::OnHullAccuracyCommitted )
				.OnValueChanged( this, &SConvexDecomposition::OnHullAccuracyChanged )
			]
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 8.0f, 0.0f, 16.0f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text( LOCTEXT("MaxHullVerts_ConvexDecomp", "Max Hull Verts") )
			]

			+SHorizontalBox::Slot()
				.FillWidth(3.0f)
				[
					SAssignNew(MaxVertsPerHull, SSpinBox<int32>)
					.MinValue(MinVertsPerHullCount)
					.MaxValue(MaxVertsPerHullCount)
					.Value( this, &SConvexDecomposition::GetVertsPerHullCount )
					.OnValueCommitted( this, &SConvexDecomposition::OnVertsPerHullCountCommitted )
					.OnValueChanged( this, &SConvexDecomposition::OnVertsPerHullCountChanged )
				]
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.Text( LOCTEXT("Apply_ConvexDecomp", "Apply") )
				.OnClicked(this, &SConvexDecomposition::OnApplyDecomp)
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.Text( LOCTEXT("Defaults_ConvexDecomp", "Defaults") )
				.OnClicked(this, &SConvexDecomposition::OnDefaults)
			]
		]
	];
}

bool FStaticMeshDetails::IsApplyNeeded() const
{
	return LevelOfDetailSettings.IsValid() && LevelOfDetailSettings->IsApplyNeeded();
}

void FStaticMeshDetails::ApplyChanges()
{
	if( LevelOfDetailSettings.IsValid() )
	{
		LevelOfDetailSettings->ApplyChanges();
	}
}

SConvexDecomposition::~SConvexDecomposition()
{

}

FReply SConvexDecomposition::OnApplyDecomp()
{
	StaticMeshEditorPtr.Pin()->DoDecomp(CurrentHullAccuracy, CurrentMaxVertsPerHullCount);

	return FReply::Handled();
}

FReply SConvexDecomposition::OnDefaults()
{
	CurrentHullAccuracy = DefaultHullAccuracy;
	CurrentMaxVertsPerHullCount = DefaultVertsPerHull;

	return FReply::Handled();
}

void SConvexDecomposition::OnHullAccuracyCommitted(float InNewValue, ETextCommit::Type CommitInfo)
{
	OnHullAccuracyChanged(InNewValue);
}

void SConvexDecomposition::OnHullAccuracyChanged(float InNewValue)
{
	CurrentHullAccuracy = InNewValue;
}

float SConvexDecomposition::GetHullAccuracy() const
{
	return CurrentHullAccuracy;
}
void SConvexDecomposition::OnVertsPerHullCountCommitted(int32 InNewValue,  ETextCommit::Type CommitInfo)
{
	OnVertsPerHullCountChanged(InNewValue);
}

void SConvexDecomposition::OnVertsPerHullCountChanged(int32 InNewValue)
{
	CurrentMaxVertsPerHullCount = InNewValue;
}

int32 SConvexDecomposition::GetVertsPerHullCount() const
{
	return CurrentMaxVertsPerHullCount;
}

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

static void FillEnumOptions(TArray<TSharedPtr<FString> >& OutStrings, UEnum& InEnum)
{
	for (int32 EnumIndex = 0; EnumIndex < InEnum.NumEnums() - 1; ++EnumIndex)
	{
		OutStrings.Add(MakeShareable(new FString(InEnum.GetEnumName(EnumIndex))));
	}
}

FMeshBuildSettingsLayout::FMeshBuildSettingsLayout( TSharedRef<FLevelOfDetailSettingsLayout> InParentLODSettings )
	: ParentLODSettings( InParentLODSettings )
{

}

FMeshBuildSettingsLayout::~FMeshBuildSettingsLayout()
{
}

void FMeshBuildSettingsLayout::GenerateHeaderRowContent( FDetailWidgetRow& NodeRow )
{
	NodeRow.NameContent()
	[
		SNew( STextBlock )
		.Text( LOCTEXT("MeshBuildSettings", "Build Settings") )
		.Font( IDetailLayoutBuilder::GetDetailFont() )
	];
}

FString FMeshBuildSettingsLayout::GetCurrentDistanceFieldReplacementMeshPath() const
{
	return BuildSettings.DistanceFieldReplacementMesh ? BuildSettings.DistanceFieldReplacementMesh->GetPathName() : FString("");
}

void FMeshBuildSettingsLayout::OnDistanceFieldReplacementMeshSelected(const FAssetData& AssetData)
{
	BuildSettings.DistanceFieldReplacementMesh = Cast<UStaticMesh>(AssetData.GetAsset());
}

void FMeshBuildSettingsLayout::GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder )
{
	{
		ChildrenBuilder.AddChildContent( LOCTEXT("RecomputeNormals", "Recompute Normals") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("RecomputeNormals", "Recompute Normals"))
		
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshBuildSettingsLayout::ShouldRecomputeNormals)
			.OnCheckStateChanged(this, &FMeshBuildSettingsLayout::OnRecomputeNormalsChanged)
		];
	}

	{
		ChildrenBuilder.AddChildContent( LOCTEXT("RecomputeTangents", "Recompute Tangents") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("RecomputeTangents", "Recompute Tangents"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshBuildSettingsLayout::ShouldRecomputeTangents)
			.OnCheckStateChanged(this, &FMeshBuildSettingsLayout::OnRecomputeTangentsChanged)
		];
	}

	{
		ChildrenBuilder.AddChildContent( LOCTEXT("UseMikkTSpace", "Use MikkTSpace Tangent Space") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("UseMikkTSpace", "Use MikkTSpace Tangent Space"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshBuildSettingsLayout::ShouldUseMikkTSpace)
			.OnCheckStateChanged(this, &FMeshBuildSettingsLayout::OnUseMikkTSpaceChanged)
		];
	}

	{
		ChildrenBuilder.AddChildContent( LOCTEXT("RemoveDegenerates", "Remove Degenerates") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("RemoveDegenerates", "Remove Degenerates"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshBuildSettingsLayout::ShouldRemoveDegenerates)
			.OnCheckStateChanged(this, &FMeshBuildSettingsLayout::OnRemoveDegeneratesChanged)
		];
	}

	{
		ChildrenBuilder.AddChildContent( LOCTEXT("BuildAdjacencyBuffer", "Build Adjacency Buffer") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("BuildAdjacencyBuffer", "Build Adjacency Buffer"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshBuildSettingsLayout::ShouldBuildAdjacencyBuffer)
			.OnCheckStateChanged(this, &FMeshBuildSettingsLayout::OnBuildAdjacencyBufferChanged)
		];
	}

	{
		ChildrenBuilder.AddChildContent( LOCTEXT("BuildReversedIndexBuffer", "Build Reversed Index Buffer") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("BuildReversedIndexBuffer", "Build Reversed Index Buffer"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshBuildSettingsLayout::ShouldBuildReversedIndexBuffer)
			.OnCheckStateChanged(this, &FMeshBuildSettingsLayout::OnBuildReversedIndexBufferChanged)
		];
	}

	{
		ChildrenBuilder.AddChildContent(LOCTEXT("UseHighPrecisionTangentBasis", "Use High Precision Tangent Basis"))
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("UseHighPrecisionTangentBasis", "Use High Precision Tangent Basis"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshBuildSettingsLayout::ShouldUseHighPrecisionTangentBasis)
			.OnCheckStateChanged(this, &FMeshBuildSettingsLayout::OnUseHighPrecisionTangentBasisChanged)
		];
	}

	{
		ChildrenBuilder.AddChildContent( LOCTEXT("UseFullPrecisionUVs", "Use Full Precision UVs") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("UseFullPrecisionUVs", "Use Full Precision UVs"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshBuildSettingsLayout::ShouldUseFullPrecisionUVs)
			.OnCheckStateChanged(this, &FMeshBuildSettingsLayout::OnUseFullPrecisionUVsChanged)
		];
	}

	{
		ChildrenBuilder.AddChildContent( LOCTEXT("GenerateLightmapUVs", "Generate Lightmap UVs") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("GenerateLightmapUVs", "Generate Lightmap UVs"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshBuildSettingsLayout::ShouldGenerateLightmapUVs)
			.OnCheckStateChanged(this, &FMeshBuildSettingsLayout::OnGenerateLightmapUVsChanged)
		];
	}

	{
		ChildrenBuilder.AddChildContent( LOCTEXT("MinLightmapResolution", "Min Lightmap Resolution") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("MinLightmapResolution", "Min Lightmap Resolution"))
		]
		.ValueContent()
		[
			SNew(SSpinBox<int32>)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.MinValue(1)
			.MaxValue(2048)
			.Value(this, &FMeshBuildSettingsLayout::GetMinLightmapResolution)
			.OnValueChanged(this, &FMeshBuildSettingsLayout::OnMinLightmapResolutionChanged)
		];
	}

	{
		ChildrenBuilder.AddChildContent( LOCTEXT("SourceLightmapIndex", "Source Lightmap Index") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("SourceLightmapIndex", "Source Lightmap Index"))
		]
		.ValueContent()
		[
			SNew(SSpinBox<int32>)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.MinValue(0)
			.MaxValue(7)
			.Value(this, &FMeshBuildSettingsLayout::GetSrcLightmapIndex)
			.OnValueChanged(this, &FMeshBuildSettingsLayout::OnSrcLightmapIndexChanged)
		];
	}

	{
		ChildrenBuilder.AddChildContent( LOCTEXT("DestinationLightmapIndex", "Destination Lightmap Index") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("DestinationLightmapIndex", "Destination Lightmap Index"))
		]
		.ValueContent()
		[
			SNew(SSpinBox<int32>)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.MinValue(0)
			.MaxValue(7)
			.Value(this, &FMeshBuildSettingsLayout::GetDstLightmapIndex)
			.OnValueChanged(this, &FMeshBuildSettingsLayout::OnDstLightmapIndexChanged)
		];
	}

	{
		ChildrenBuilder.AddChildContent(LOCTEXT("BuildScale", "Build Scale"))
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("BuildScale", "Build Scale"))
			.ToolTipText( LOCTEXT("BuildScale_ToolTip", "The local scale applied when building the mesh") )
		]
		.ValueContent()
		.MinDesiredWidth(125.0f * 3.0f)
		.MaxDesiredWidth(125.0f * 3.0f)
		[
			SNew(SVectorInputBox)
			.X(this, &FMeshBuildSettingsLayout::GetBuildScaleX)
			.Y(this, &FMeshBuildSettingsLayout::GetBuildScaleY)
			.Z(this, &FMeshBuildSettingsLayout::GetBuildScaleZ)
			.bColorAxisLabels(false)
			.AllowResponsiveLayout(true)
			.OnXCommitted(this, &FMeshBuildSettingsLayout::OnBuildScaleXChanged)
			.OnYCommitted(this, &FMeshBuildSettingsLayout::OnBuildScaleYChanged)
			.OnZCommitted(this, &FMeshBuildSettingsLayout::OnBuildScaleZChanged)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];
	}

	{
		ChildrenBuilder.AddChildContent( LOCTEXT("DistanceFieldResolutionScale", "Distance Field Resolution Scale") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("DistanceFieldResolutionScale", "Distance Field Resolution Scale"))
		]
		.ValueContent()
		[
			SNew(SSpinBox<float>)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.MinValue(0.0f)
			.MaxValue(100.0f)
			.Value(this, &FMeshBuildSettingsLayout::GetDistanceFieldResolutionScale)
			.OnValueChanged(this, &FMeshBuildSettingsLayout::OnDistanceFieldResolutionScaleChanged)
			.OnValueCommitted(this, &FMeshBuildSettingsLayout::OnDistanceFieldResolutionScaleCommitted)
		];
	}
		
	{
		ChildrenBuilder.AddChildContent( LOCTEXT("GenerateDistanceFieldAsIfTwoSided", "Generate Distance Field as if TwoSided") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("GenerateDistanceFieldAsIfTwoSided", "Generate Distance Field as if TwoSided"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshBuildSettingsLayout::ShouldGenerateDistanceFieldAsIfTwoSided)
			.OnCheckStateChanged(this, &FMeshBuildSettingsLayout::OnGenerateDistanceFieldAsIfTwoSidedChanged)
		];
	}

	{
		TSharedRef<SWidget> PropWidget = SNew(SObjectPropertyEntryBox)
			.AllowedClass(UStaticMesh::StaticClass())
			.AllowClear(true)
			.ObjectPath(this, &FMeshBuildSettingsLayout::GetCurrentDistanceFieldReplacementMeshPath)
			.OnObjectChanged(this, &FMeshBuildSettingsLayout::OnDistanceFieldReplacementMeshSelected);

		ChildrenBuilder.AddChildContent( LOCTEXT("DistanceFieldReplacementMesh", "Distance Field Replacement Mesh") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("DistanceFieldReplacementMesh", "Distance Field Replacement Mesh"))
		]
		.ValueContent()
		[
			PropWidget
		];
	}

	{
		ChildrenBuilder.AddChildContent( LOCTEXT("ApplyChanges", "Apply Changes") )
		.ValueContent()
		.HAlign(HAlign_Left)
		[
			SNew(SButton)
			.OnClicked(this, &FMeshBuildSettingsLayout::OnApplyChanges)
			.IsEnabled(ParentLODSettings.Pin().ToSharedRef(), &FLevelOfDetailSettingsLayout::IsApplyNeeded)
			[
				SNew( STextBlock )
				.Text(LOCTEXT("ApplyChanges", "Apply Changes"))
				.Font( IDetailLayoutBuilder::GetDetailFont() )
			]
		];
	}
}

void FMeshBuildSettingsLayout::UpdateSettings(const FMeshBuildSettings& InSettings)
{
	BuildSettings = InSettings;
}

FReply FMeshBuildSettingsLayout::OnApplyChanges()
{
	if( ParentLODSettings.IsValid() )
	{
		ParentLODSettings.Pin()->ApplyChanges();
	}
	return FReply::Handled();
}

ECheckBoxState FMeshBuildSettingsLayout::ShouldRecomputeNormals() const
{
	return BuildSettings.bRecomputeNormals ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FMeshBuildSettingsLayout::ShouldRecomputeTangents() const
{
	return BuildSettings.bRecomputeTangents ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FMeshBuildSettingsLayout::ShouldUseMikkTSpace() const
{
	return BuildSettings.bUseMikkTSpace ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FMeshBuildSettingsLayout::ShouldRemoveDegenerates() const
{
	return BuildSettings.bRemoveDegenerates ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FMeshBuildSettingsLayout::ShouldBuildAdjacencyBuffer() const
{
	return BuildSettings.bBuildAdjacencyBuffer ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FMeshBuildSettingsLayout::ShouldBuildReversedIndexBuffer() const
{
	return BuildSettings.bBuildReversedIndexBuffer ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FMeshBuildSettingsLayout::ShouldUseHighPrecisionTangentBasis() const
{
	return BuildSettings.bUseHighPrecisionTangentBasis ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FMeshBuildSettingsLayout::ShouldUseFullPrecisionUVs() const
{
	return BuildSettings.bUseFullPrecisionUVs ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FMeshBuildSettingsLayout::ShouldGenerateLightmapUVs() const
{
	return BuildSettings.bGenerateLightmapUVs ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FMeshBuildSettingsLayout::ShouldGenerateDistanceFieldAsIfTwoSided() const
{
	return BuildSettings.bGenerateDistanceFieldAsIfTwoSided ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

int32 FMeshBuildSettingsLayout::GetMinLightmapResolution() const
{
	return BuildSettings.MinLightmapResolution;
}

int32 FMeshBuildSettingsLayout::GetSrcLightmapIndex() const
{
	return BuildSettings.SrcLightmapIndex;
}

int32 FMeshBuildSettingsLayout::GetDstLightmapIndex() const
{
	return BuildSettings.DstLightmapIndex;
}

TOptional<float> FMeshBuildSettingsLayout::GetBuildScaleX() const
{
	return BuildSettings.BuildScale3D.X;
}

TOptional<float> FMeshBuildSettingsLayout::GetBuildScaleY() const
{
	return BuildSettings.BuildScale3D.Y;
}

TOptional<float> FMeshBuildSettingsLayout::GetBuildScaleZ() const
{
	return BuildSettings.BuildScale3D.Z;
}

float FMeshBuildSettingsLayout::GetDistanceFieldResolutionScale() const
{
	return BuildSettings.DistanceFieldResolutionScale;
}

void FMeshBuildSettingsLayout::OnRecomputeNormalsChanged(ECheckBoxState NewState)
{
	const bool bRecomputeNormals = (NewState == ECheckBoxState::Checked) ? true : false;
	if (BuildSettings.bRecomputeNormals != bRecomputeNormals)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("bRecomputeNormals"), bRecomputeNormals ? TEXT("True") : TEXT("False"));
		}
		BuildSettings.bRecomputeNormals = bRecomputeNormals;
	}
}

void FMeshBuildSettingsLayout::OnRecomputeTangentsChanged(ECheckBoxState NewState)
{
	const bool bRecomputeTangents = (NewState == ECheckBoxState::Checked) ? true : false;
	if (BuildSettings.bRecomputeTangents != bRecomputeTangents)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("bRecomputeTangents"), bRecomputeTangents ? TEXT("True") : TEXT("False"));
		}
		BuildSettings.bRecomputeTangents = bRecomputeTangents;
	}
}

void FMeshBuildSettingsLayout::OnUseMikkTSpaceChanged(ECheckBoxState NewState)
{
	const bool bUseMikkTSpace = (NewState == ECheckBoxState::Checked) ? true : false;
	if (BuildSettings.bUseMikkTSpace != bUseMikkTSpace)
	{
		BuildSettings.bUseMikkTSpace = bUseMikkTSpace;
	}
}

void FMeshBuildSettingsLayout::OnRemoveDegeneratesChanged(ECheckBoxState NewState)
{
	const bool bRemoveDegenerates = (NewState == ECheckBoxState::Checked) ? true : false;
	if (BuildSettings.bRemoveDegenerates != bRemoveDegenerates)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("bRemoveDegenerates"), bRemoveDegenerates ? TEXT("True") : TEXT("False"));
		}
		BuildSettings.bRemoveDegenerates = bRemoveDegenerates;
	}
}

void FMeshBuildSettingsLayout::OnBuildAdjacencyBufferChanged(ECheckBoxState NewState)
{
	const bool bBuildAdjacencyBuffer = (NewState == ECheckBoxState::Checked) ? true : false;
	if (BuildSettings.bBuildAdjacencyBuffer != bBuildAdjacencyBuffer)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("bBuildAdjacencyBuffer"), bBuildAdjacencyBuffer ? TEXT("True") : TEXT("False"));
		}
		BuildSettings.bBuildAdjacencyBuffer = bBuildAdjacencyBuffer;
	}
}

void FMeshBuildSettingsLayout::OnBuildReversedIndexBufferChanged(ECheckBoxState NewState)
{
	const bool bBuildReversedIndexBuffer = (NewState == ECheckBoxState::Checked) ? true : false;
	if (BuildSettings.bBuildReversedIndexBuffer != bBuildReversedIndexBuffer)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("bBuildReversedIndexBuffer"), bBuildReversedIndexBuffer ? TEXT("True") : TEXT("False"));
		}
		BuildSettings.bBuildReversedIndexBuffer = bBuildReversedIndexBuffer;
	}
}

void FMeshBuildSettingsLayout::OnUseHighPrecisionTangentBasisChanged(ECheckBoxState NewState)
{
	const bool bUseHighPrecisionTangents = (NewState == ECheckBoxState::Checked) ? true : false;
	if (BuildSettings.bUseHighPrecisionTangentBasis != bUseHighPrecisionTangents)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("bUseHighPrecisionTangentBasis"), bUseHighPrecisionTangents ? TEXT("True") : TEXT("False"));
		}
		BuildSettings.bUseHighPrecisionTangentBasis = bUseHighPrecisionTangents;
	}
}

void FMeshBuildSettingsLayout::OnUseFullPrecisionUVsChanged(ECheckBoxState NewState)
{
	const bool bUseFullPrecisionUVs = (NewState == ECheckBoxState::Checked) ? true : false;
	if (BuildSettings.bUseFullPrecisionUVs != bUseFullPrecisionUVs)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("bUseFullPrecisionUVs"), bUseFullPrecisionUVs ? TEXT("True") : TEXT("False"));
		}
		BuildSettings.bUseFullPrecisionUVs = bUseFullPrecisionUVs;
	}
}

void FMeshBuildSettingsLayout::OnGenerateLightmapUVsChanged(ECheckBoxState NewState)
{
	const bool bGenerateLightmapUVs = (NewState == ECheckBoxState::Checked) ? true : false;
	if (BuildSettings.bGenerateLightmapUVs != bGenerateLightmapUVs)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("bGenerateLightmapUVs"), bGenerateLightmapUVs ? TEXT("True") : TEXT("False"));
		}
		BuildSettings.bGenerateLightmapUVs = bGenerateLightmapUVs;
	}
}

void FMeshBuildSettingsLayout::OnGenerateDistanceFieldAsIfTwoSidedChanged(ECheckBoxState NewState)
{
	const bool bGenerateDistanceFieldAsIfTwoSided = (NewState == ECheckBoxState::Checked) ? true : false;
	if (BuildSettings.bGenerateDistanceFieldAsIfTwoSided != bGenerateDistanceFieldAsIfTwoSided)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("bGenerateDistanceFieldAsIfTwoSided"), bGenerateDistanceFieldAsIfTwoSided ? TEXT("True") : TEXT("False"));
		}
		BuildSettings.bGenerateDistanceFieldAsIfTwoSided = bGenerateDistanceFieldAsIfTwoSided;
	}
}

void FMeshBuildSettingsLayout::OnMinLightmapResolutionChanged( int32 NewValue )
{
	if (BuildSettings.MinLightmapResolution != NewValue)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("MinLightmapResolution"), FString::Printf(TEXT("%i"), NewValue));
		}
		BuildSettings.MinLightmapResolution = NewValue;
	}
}

void FMeshBuildSettingsLayout::OnSrcLightmapIndexChanged( int32 NewValue )
{
	if (BuildSettings.SrcLightmapIndex != NewValue)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("SrcLightmapIndex"), FString::Printf(TEXT("%i"), NewValue));
		}
		BuildSettings.SrcLightmapIndex = NewValue;
	}
}

void FMeshBuildSettingsLayout::OnDstLightmapIndexChanged( int32 NewValue )
{
	if (BuildSettings.DstLightmapIndex != NewValue)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("DstLightmapIndex"), FString::Printf(TEXT("%i"), NewValue));
		}
		BuildSettings.DstLightmapIndex = NewValue;
	}
}

void FMeshBuildSettingsLayout::OnBuildScaleXChanged( float NewScaleX, ETextCommit::Type TextCommitType )
{
	if (!FMath::IsNearlyEqual(NewScaleX, 0.0f) && BuildSettings.BuildScale3D.X != NewScaleX)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("BuildScale3D.X"), FString::Printf(TEXT("%.3f"), NewScaleX));
		}
		BuildSettings.BuildScale3D.X = NewScaleX;
	}
}

void FMeshBuildSettingsLayout::OnBuildScaleYChanged( float NewScaleY, ETextCommit::Type TextCommitType )
{
	if (!FMath::IsNearlyEqual(NewScaleY, 0.0f) && BuildSettings.BuildScale3D.Y != NewScaleY)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("BuildScale3D.Y"), FString::Printf(TEXT("%.3f"), NewScaleY));
		}
		BuildSettings.BuildScale3D.Y = NewScaleY;
	}
}

void FMeshBuildSettingsLayout::OnBuildScaleZChanged( float NewScaleZ, ETextCommit::Type TextCommitType )
{
	if (!FMath::IsNearlyEqual(NewScaleZ, 0.0f) && BuildSettings.BuildScale3D.Z != NewScaleZ)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("BuildScale3D.Z"), FString::Printf(TEXT("%.3f"), NewScaleZ));
		}
		BuildSettings.BuildScale3D.Z = NewScaleZ;
	}
}

void FMeshBuildSettingsLayout::OnDistanceFieldResolutionScaleChanged(float NewValue)
{
	BuildSettings.DistanceFieldResolutionScale = NewValue;
}

void FMeshBuildSettingsLayout::OnDistanceFieldResolutionScaleCommitted(float NewValue, ETextCommit::Type TextCommitType)
{
	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("DistanceFieldResolutionScale"), FString::Printf(TEXT("%.3f"), NewValue));
	}
	OnDistanceFieldResolutionScaleChanged(NewValue);
}

FMeshReductionSettingsLayout::FMeshReductionSettingsLayout( TSharedRef<FLevelOfDetailSettingsLayout> InParentLODSettings )
	: ParentLODSettings( InParentLODSettings )
{
	FillEnumOptions(ImportanceOptions, GetFeatureImportanceEnum());
}

FMeshReductionSettingsLayout::~FMeshReductionSettingsLayout()
{
}

void FMeshReductionSettingsLayout::GenerateHeaderRowContent( FDetailWidgetRow& NodeRow  )
{
	NodeRow.NameContent()
	[
		SNew( STextBlock )
		.Text( LOCTEXT("MeshReductionSettings", "Reduction Settings") )
		.Font( IDetailLayoutBuilder::GetDetailFont() )
	];
}


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FMeshReductionSettingsLayout::GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder )
{
	{
		ChildrenBuilder.AddChildContent( LOCTEXT("PercentTriangles", "Percent Triangles") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("PercentTriangles", "Percent Triangles"))
		]
		.ValueContent()
		[
			SNew(SSpinBox<float>)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.MinValue(0.0f)
			.MaxValue(100.0f)
			.Value(this, &FMeshReductionSettingsLayout::GetPercentTriangles)
			.OnValueChanged(this, &FMeshReductionSettingsLayout::OnPercentTrianglesChanged)
			.OnValueCommitted(this, &FMeshReductionSettingsLayout::OnPercentTrianglesCommitted)
		];

	}

	{
		ChildrenBuilder.AddChildContent( LOCTEXT("MaxDeviation", "Max Deviation") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("MaxDeviation", "Max Deviation"))
		]
		.ValueContent()
		[
			SNew(SSpinBox<float>)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.MinValue(0.0f)
			.MaxValue(1000.0f)
			.Value(this, &FMeshReductionSettingsLayout::GetMaxDeviation)
			.OnValueChanged(this, &FMeshReductionSettingsLayout::OnMaxDeviationChanged)
			.OnValueCommitted(this, &FMeshReductionSettingsLayout::OnMaxDeviationCommitted)
		];

	}

	{
		ChildrenBuilder.AddChildContent( LOCTEXT("Silhouette_MeshSimplification", "Silhouette") )
		.NameContent()
		[
			SNew( STextBlock )
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text( LOCTEXT("Silhouette_MeshSimplification", "Silhouette") )
		]
		.ValueContent()
		[
			SAssignNew(SilhouetteCombo, STextComboBox)
			//.Font( IDetailLayoutBuilder::GetDetailFont() )
			.ContentPadding(0)
			.OptionsSource(&ImportanceOptions)
			.InitiallySelectedItem(ImportanceOptions[ReductionSettings.SilhouetteImportance])
			.OnSelectionChanged(this, &FMeshReductionSettingsLayout::OnSilhouetteImportanceChanged)
		];

	}

	{
		ChildrenBuilder.AddChildContent( LOCTEXT("Texture_MeshSimplification", "Texture") )
		.NameContent()
		[
			SNew( STextBlock )
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text( LOCTEXT("Texture_MeshSimplification", "Texture") )
		]
		.ValueContent()
		[
			SAssignNew( TextureCombo, STextComboBox )
			//.Font( IDetailLayoutBuilder::GetDetailFont() )
			.ContentPadding(0)
			.OptionsSource( &ImportanceOptions )
			.InitiallySelectedItem(ImportanceOptions[ReductionSettings.TextureImportance])
			.OnSelectionChanged(this, &FMeshReductionSettingsLayout::OnTextureImportanceChanged)
		];

	}

	{
		ChildrenBuilder.AddChildContent( LOCTEXT("Shading_MeshSimplification", "Shading") )
		.NameContent()
		[
			SNew( STextBlock )
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text( LOCTEXT("Shading_MeshSimplification", "Shading") )
		]
		.ValueContent()
		[
			SAssignNew( ShadingCombo, STextComboBox )
			//.Font( IDetailLayoutBuilder::GetDetailFont() )
			.ContentPadding(0)
			.OptionsSource( &ImportanceOptions )
			.InitiallySelectedItem(ImportanceOptions[ReductionSettings.ShadingImportance])
			.OnSelectionChanged(this, &FMeshReductionSettingsLayout::OnShadingImportanceChanged)
		];

	}

	{
		ChildrenBuilder.AddChildContent( LOCTEXT("WeldingThreshold", "Welding Threshold") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("WeldingThreshold", "Welding Threshold"))
		]
		.ValueContent()
		[
			SNew(SSpinBox<float>)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.MinValue(0.0f)
			.MaxValue(10.0f)
			.Value(this, &FMeshReductionSettingsLayout::GetWeldingThreshold)
			.OnValueChanged(this, &FMeshReductionSettingsLayout::OnWeldingThresholdChanged)
			.OnValueCommitted(this, &FMeshReductionSettingsLayout::OnWeldingThresholdCommitted)
		];

	}

	{
		ChildrenBuilder.AddChildContent( LOCTEXT("RecomputeNormals", "Recompute Normals") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("RecomputeNormals", "Recompute Normals"))

		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshReductionSettingsLayout::ShouldRecalculateNormals)
			.OnCheckStateChanged(this, &FMeshReductionSettingsLayout::OnRecalculateNormalsChanged)
		];
	}

	{
		ChildrenBuilder.AddChildContent( LOCTEXT("HardEdgeAngle", "Hard Edge Angle") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("HardEdgeAngle", "Hard Edge Angle"))
		]
		.ValueContent()
		[
			SNew(SSpinBox<float>)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.MinValue(0.0f)
			.MaxValue(180.0f)
			.Value(this, &FMeshReductionSettingsLayout::GetHardAngleThreshold)
			.OnValueChanged(this, &FMeshReductionSettingsLayout::OnHardAngleThresholdChanged)
			.OnValueCommitted(this, &FMeshReductionSettingsLayout::OnHardAngleThresholdCommitted)
		];

	}

	{
		ChildrenBuilder.AddChildContent( LOCTEXT("ApplyChanges", "Apply Changes") )
			.ValueContent()
			.HAlign(HAlign_Left)
			[
				SNew(SButton)
				.OnClicked(this, &FMeshReductionSettingsLayout::OnApplyChanges)
				.IsEnabled(ParentLODSettings.Pin().ToSharedRef(), &FLevelOfDetailSettingsLayout::IsApplyNeeded)
				[
					SNew( STextBlock )
					.Text(LOCTEXT("ApplyChanges", "Apply Changes"))
					.Font( IDetailLayoutBuilder::GetDetailFont() )
				]
			];
	}

	SilhouetteCombo->SetSelectedItem(ImportanceOptions[ReductionSettings.SilhouetteImportance]);
	TextureCombo->SetSelectedItem(ImportanceOptions[ReductionSettings.TextureImportance]);
	ShadingCombo->SetSelectedItem(ImportanceOptions[ReductionSettings.ShadingImportance]);
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

const FMeshReductionSettings& FMeshReductionSettingsLayout::GetSettings() const
{
	return ReductionSettings;
}

void FMeshReductionSettingsLayout::UpdateSettings(const FMeshReductionSettings& InSettings)
{
	ReductionSettings = InSettings;
}

FReply FMeshReductionSettingsLayout::OnApplyChanges()
{
	if( ParentLODSettings.IsValid() )
	{
		ParentLODSettings.Pin()->ApplyChanges();
	}
	return FReply::Handled();
}

float FMeshReductionSettingsLayout::GetPercentTriangles() const
{
	return ReductionSettings.PercentTriangles * 100.0f; // Display fraction as percentage.
}

float FMeshReductionSettingsLayout::GetMaxDeviation() const
{
	return ReductionSettings.MaxDeviation;
}

float FMeshReductionSettingsLayout::GetWeldingThreshold() const
{
	return ReductionSettings.WeldingThreshold;
}

ECheckBoxState FMeshReductionSettingsLayout::ShouldRecalculateNormals() const
{
	return ReductionSettings.bRecalculateNormals ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

float FMeshReductionSettingsLayout::GetHardAngleThreshold() const
{
	return ReductionSettings.HardAngleThreshold;
}

void FMeshReductionSettingsLayout::OnPercentTrianglesChanged(float NewValue)
{
	// Percentage -> fraction.
	ReductionSettings.PercentTriangles = NewValue * 0.01f;
}

void FMeshReductionSettingsLayout::OnPercentTrianglesCommitted(float NewValue, ETextCommit::Type TextCommitType)
{
	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.ReductionSettings"), TEXT("PercentTriangles"), FString::Printf(TEXT("%.1f"), NewValue));
	}
	OnPercentTrianglesChanged(NewValue);
}

void FMeshReductionSettingsLayout::OnMaxDeviationChanged(float NewValue)
{
	ReductionSettings.MaxDeviation = NewValue;
}

void FMeshReductionSettingsLayout::OnMaxDeviationCommitted(float NewValue, ETextCommit::Type TextCommitType)
{
	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.ReductionSettings"), TEXT("MaxDeviation"), FString::Printf(TEXT("%.1f"), NewValue));
	}
	OnMaxDeviationChanged(NewValue);
}

void FMeshReductionSettingsLayout::OnWeldingThresholdChanged(float NewValue)
{
	ReductionSettings.WeldingThreshold = NewValue;
}

void FMeshReductionSettingsLayout::OnWeldingThresholdCommitted(float NewValue, ETextCommit::Type TextCommitType)
{
	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.ReductionSettings"), TEXT("WeldingThreshold"), FString::Printf(TEXT("%.2f"), NewValue));
	}
	OnWeldingThresholdChanged(NewValue);
}

void FMeshReductionSettingsLayout::OnRecalculateNormalsChanged(ECheckBoxState NewValue)
{
	const bool bRecalculateNormals = NewValue == ECheckBoxState::Checked;
	if (ReductionSettings.bRecalculateNormals != bRecalculateNormals)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.ReductionSettings"), TEXT("bRecalculateNormals"), bRecalculateNormals ? TEXT("True") : TEXT("False"));
		}
		ReductionSettings.bRecalculateNormals = bRecalculateNormals;
	}
}

void FMeshReductionSettingsLayout::OnHardAngleThresholdChanged(float NewValue)
{
	ReductionSettings.HardAngleThreshold = NewValue;
}

void FMeshReductionSettingsLayout::OnHardAngleThresholdCommitted(float NewValue, ETextCommit::Type TextCommitType)
{
	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.ReductionSettings"), TEXT("HardAngleThreshold"), FString::Printf(TEXT("%.3f"), NewValue));
	}
	OnHardAngleThresholdChanged(NewValue);
}

void FMeshReductionSettingsLayout::OnSilhouetteImportanceChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	const EMeshFeatureImportance::Type SilhouetteImportance = (EMeshFeatureImportance::Type)ImportanceOptions.Find(NewValue);
	if (ReductionSettings.SilhouetteImportance != SilhouetteImportance)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.ReductionSettings"), TEXT("SilhouetteImportance"), *NewValue.Get());
		}
		ReductionSettings.SilhouetteImportance = SilhouetteImportance;
	}
}

void FMeshReductionSettingsLayout::OnTextureImportanceChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	const EMeshFeatureImportance::Type TextureImportance = (EMeshFeatureImportance::Type)ImportanceOptions.Find(NewValue);
	if (ReductionSettings.TextureImportance != TextureImportance)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.ReductionSettings"), TEXT("TextureImportance"), *NewValue.Get());
		}
		ReductionSettings.TextureImportance = TextureImportance;
	}
}

void FMeshReductionSettingsLayout::OnShadingImportanceChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	const EMeshFeatureImportance::Type ShadingImportance = (EMeshFeatureImportance::Type)ImportanceOptions.Find(NewValue);
	if (ReductionSettings.ShadingImportance != ShadingImportance)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.ReductionSettings"), TEXT("ShadingImportance"), *NewValue.Get());
		}
		ReductionSettings.ShadingImportance = ShadingImportance;
	}
}

FMeshSectionSettingsLayout::~FMeshSectionSettingsLayout()
{
}

UStaticMesh& FMeshSectionSettingsLayout::GetStaticMesh() const
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
	check(StaticMesh);
	return *StaticMesh;
}

void FMeshSectionSettingsLayout::AddToCategory( IDetailCategoryBuilder& CategoryBuilder )
{
	FMaterialListDelegates MaterialListDelegates; 
	MaterialListDelegates.OnGetMaterials.BindSP(this, &FMeshSectionSettingsLayout::GetMaterials);
	MaterialListDelegates.OnMaterialChanged.BindSP(this, &FMeshSectionSettingsLayout::OnMaterialChanged);
	MaterialListDelegates.OnGenerateCustomNameWidgets.BindSP( this, &FMeshSectionSettingsLayout::OnGenerateNameWidgetsForMaterial);
	MaterialListDelegates.OnGenerateCustomMaterialWidgets.BindSP(this, &FMeshSectionSettingsLayout::OnGenerateWidgetsForMaterial);
	MaterialListDelegates.OnResetMaterialToDefaultClicked.BindSP(this, &FMeshSectionSettingsLayout::OnResetMaterialToDefaultClicked);

	CategoryBuilder.AddCustomBuilder( MakeShareable( new FMaterialList( CategoryBuilder.GetParentLayout(), MaterialListDelegates ) ) );
}

void FMeshSectionSettingsLayout::GetMaterials(IMaterialListBuilder& ListBuilder)
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	FStaticMeshRenderData* RenderData = StaticMesh.RenderData;
	if (RenderData && RenderData->LODResources.IsValidIndex(LODIndex))
	{
		FStaticMeshLODResources& LOD = RenderData->LODResources[LODIndex];
		int32 NumSections = LOD.Sections.Num();
		for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
		{
			FMeshSectionInfo Info = StaticMesh.SectionInfoMap.Get(LODIndex, SectionIndex);
			UMaterialInterface* SectionMaterial = StaticMesh.GetMaterial(Info.MaterialIndex);
			if (SectionMaterial == NULL)
			{
				SectionMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
			}
			ListBuilder.AddMaterial(SectionIndex, SectionMaterial, /*bCanBeReplaced=*/ true);
		}
	}
}

void FMeshSectionSettingsLayout::OnMaterialChanged(UMaterialInterface* NewMaterial, UMaterialInterface* PrevMaterial, int32 SlotIndex, bool bReplaceAll)
{
	UStaticMesh& StaticMesh = GetStaticMesh();

	// flag the property (Materials) we're modifying so that not all of the object is rebuilt.
	UProperty* ChangedProperty = NULL;
	ChangedProperty = FindField<UProperty>( UStaticMesh::StaticClass(), "Materials" );
	check(ChangedProperty);
	StaticMesh.PreEditChange(ChangedProperty);
	
	check(StaticMesh.RenderData);
	FMeshSectionInfo Info = StaticMesh.SectionInfoMap.Get(LODIndex, SlotIndex);
	if (LODIndex == 0)
	{
		check(Info.MaterialIndex == SlotIndex);
		check(StaticMesh.Materials.IsValidIndex(SlotIndex));
		StaticMesh.Materials[SlotIndex] = NewMaterial;
	}
	else
	{
		int32 NumBaseSections = StaticMesh.RenderData->LODResources[0].Sections.Num();
		if (Info.MaterialIndex < NumBaseSections)
		{
			// The LOD's section was using the same material as in the base LOD (common case).
			Info.MaterialIndex = StaticMesh.Materials.Add(NewMaterial);
			StaticMesh.SectionInfoMap.Set(LODIndex, SlotIndex, Info);
		}
		else
		{
			// The LOD's section was already overriding the base LOD material.
			StaticMesh.Materials[Info.MaterialIndex] = NewMaterial;
		}
	}
	CallPostEditChange(ChangedProperty);
}

TSharedRef<SWidget> FMeshSectionSettingsLayout::OnGenerateNameWidgetsForMaterial(UMaterialInterface* Material, int32 SlotIndex)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshSectionSettingsLayout::IsSectionHighlighted, SlotIndex)
			.OnCheckStateChanged(this, &FMeshSectionSettingsLayout::OnSectionHighlightedChanged, SlotIndex)
			.ToolTipText(LOCTEXT("Highlight_ToolTip", "Highlights this section in the viewport"))
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity( FLinearColor( 0.4f, 0.4f, 0.4f, 1.0f) )
				.Text(LOCTEXT("Highlight", "Highlight"))

			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 0)
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshSectionSettingsLayout::IsSectionIsolatedEnabled, SlotIndex)
			.OnCheckStateChanged(this, &FMeshSectionSettingsLayout::OnSectionIsolatedChanged, SlotIndex)
			.ToolTipText(LOCTEXT("Isolate_ToolTip", "Isolates this section in the viewport"))
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f, 1.0f))
				.Text(LOCTEXT("Isolate", "Isolate"))

			]
		];
}

TSharedRef<SWidget> FMeshSectionSettingsLayout::OnGenerateWidgetsForMaterial(UMaterialInterface* Material, int32 SlotIndex)
{
	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SCheckBox)
				.IsChecked(this, &FMeshSectionSettingsLayout::DoesSectionCastShadow, SlotIndex)
				.OnCheckStateChanged(this, &FMeshSectionSettingsLayout::OnSectionCastShadowChanged, SlotIndex)
			[
				SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("StaticMeshEditor.NormalFont"))
					.Text(LOCTEXT("CastShadow", "Cast Shadow"))
			]
		]
		+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0,2,0,0)
		[
			SNew(SCheckBox)
				.IsEnabled(this, &FMeshSectionSettingsLayout::SectionCollisionEnabled)
				.ToolTipText(this, &FMeshSectionSettingsLayout::GetCollisionEnabledToolTip)
				.IsChecked(this, &FMeshSectionSettingsLayout::DoesSectionCollide, SlotIndex)
				.OnCheckStateChanged(this, &FMeshSectionSettingsLayout::OnSectionCollisionChanged, SlotIndex)
			[
				SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("StaticMeshEditor.NormalFont"))
					.Text(LOCTEXT("EnableCollision", "Enable Collision"))
			]
		];
}

void FMeshSectionSettingsLayout::OnResetMaterialToDefaultClicked(UMaterialInterface* Material, int32 SlotIndex)
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	if (LODIndex == 0)
	{
		check(StaticMesh.Materials.IsValidIndex(SlotIndex));
		StaticMesh.Materials[SlotIndex] = UMaterial::GetDefaultMaterial(MD_Surface);
	}
	else
	{
		//Use the LOD 0 with the pass Slot index to replace the material
		FMeshSectionInfo Lod0Info = StaticMesh.SectionInfoMap.Get(0, SlotIndex);
		FMeshSectionInfo Info = StaticMesh.SectionInfoMap.Get(LODIndex, SlotIndex);
		if (StaticMesh.Materials.IsValidIndex(Info.MaterialIndex) && StaticMesh.Materials.IsValidIndex(Lod0Info.MaterialIndex))
		{
			StaticMesh.Materials[Info.MaterialIndex] = StaticMesh.Materials[Lod0Info.MaterialIndex];
		}
	}
	CallPostEditChange();
}

ECheckBoxState FMeshSectionSettingsLayout::DoesSectionCastShadow(int32 SectionIndex) const
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	FMeshSectionInfo Info = StaticMesh.SectionInfoMap.Get(LODIndex, SectionIndex);
	return Info.bCastShadow ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FMeshSectionSettingsLayout::OnSectionCastShadowChanged(ECheckBoxState NewState, int32 SectionIndex)
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	FMeshSectionInfo Info = StaticMesh.SectionInfoMap.Get(LODIndex, SectionIndex);
	Info.bCastShadow = (NewState == ECheckBoxState::Checked) ? true : false;
	StaticMesh.SectionInfoMap.Set(LODIndex, SectionIndex, Info);
	CallPostEditChange();
}

bool FMeshSectionSettingsLayout::SectionCollisionEnabled() const
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	// Only enable 'Enable Collision' check box if this LOD is used for collision
	return (StaticMesh.LODForCollision == LODIndex);
}

FText FMeshSectionSettingsLayout::GetCollisionEnabledToolTip() const
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	
	// If using a different LOD for collision, disable the check box
	if (StaticMesh.LODForCollision != LODIndex)
	{
		return LOCTEXT("EnableCollisionToolTipDisabled", "This LOD is not used for collision, see the LODForCollision setting.");
	}
	// This LOD is used for collision, give info on what flag does
	else
	{
		return LOCTEXT("EnableCollisionToolTipEnabled", "Controls whether this section ever has per-poly collision. Disabling this where possible will lower memory usage for this mesh.");
	}
}

ECheckBoxState FMeshSectionSettingsLayout::DoesSectionCollide(int32 SectionIndex) const
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	FMeshSectionInfo Info = StaticMesh.SectionInfoMap.Get(LODIndex, SectionIndex);
	return Info.bEnableCollision ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FMeshSectionSettingsLayout::OnSectionCollisionChanged(ECheckBoxState NewState, int32 SectionIndex)
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	FMeshSectionInfo Info = StaticMesh.SectionInfoMap.Get(LODIndex, SectionIndex);
	Info.bEnableCollision = (NewState == ECheckBoxState::Checked) ? true : false;
	StaticMesh.SectionInfoMap.Set(LODIndex, SectionIndex, Info);
	CallPostEditChange();
}

ECheckBoxState FMeshSectionSettingsLayout::IsSectionHighlighted(int32 SectionIndex) const
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	UStaticMeshComponent* Component = StaticMeshEditor.GetStaticMeshComponent();
	if (Component)
	{
		State = Component->SelectedEditorSection == SectionIndex ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return State;
}

void FMeshSectionSettingsLayout::OnSectionHighlightedChanged(ECheckBoxState NewState, int32 SectionIndex)
{
	UStaticMeshComponent* Component = StaticMeshEditor.GetStaticMeshComponent();
	if (Component)
	{
		if (NewState == ECheckBoxState::Checked)
		{
			Component->SelectedEditorSection = SectionIndex;
			if (Component->SectionIndexPreview != SectionIndex)
			{
				// Unhide all mesh sections
				Component->SetSectionPreview(INDEX_NONE);
			}
		}
		else if (NewState == ECheckBoxState::Unchecked)
		{
			Component->SelectedEditorSection = INDEX_NONE;
		}
		Component->MarkRenderStateDirty();
		StaticMeshEditor.RefreshViewport();
	}
}

ECheckBoxState FMeshSectionSettingsLayout::IsSectionIsolatedEnabled(int32 SectionIndex) const
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	const UStaticMeshComponent* Component = StaticMeshEditor.GetStaticMeshComponent();
	if (Component)
	{
		State = Component->SectionIndexPreview == SectionIndex ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return State;
}

void FMeshSectionSettingsLayout::OnSectionIsolatedChanged(ECheckBoxState NewState, int32 SectionIndex)
{
	UStaticMeshComponent* Component = StaticMeshEditor.GetStaticMeshComponent();
	if (Component)
	{
		if (NewState == ECheckBoxState::Checked)
		{
			Component->SetSectionPreview(SectionIndex);
			if (Component->SelectedEditorSection != SectionIndex)
			{
				Component->SelectedEditorSection = INDEX_NONE;
			}
		}
		else if (NewState == ECheckBoxState::Unchecked)
		{
			Component->SetSectionPreview(INDEX_NONE);
		}
		Component->MarkRenderStateDirty();
		StaticMeshEditor.RefreshViewport();
	}
}

void FMeshSectionSettingsLayout::CallPostEditChange(UProperty* PropertyChanged/*=nullptr*/)
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	if( PropertyChanged )
	{
		FPropertyChangedEvent PropertyUpdateStruct(PropertyChanged);
		StaticMesh.PostEditChangeProperty(PropertyUpdateStruct);
	}
	else
	{
		StaticMesh.Modify();
		StaticMesh.PostEditChange();
	}
	if(StaticMesh.BodySetup)
	{
		StaticMesh.BodySetup->CreatePhysicsMeshes();
	}
	StaticMeshEditor.RefreshViewport();
}

/////////////////////////////////
// FLevelOfDetailSettingsLayout
/////////////////////////////////

FLevelOfDetailSettingsLayout::FLevelOfDetailSettingsLayout( FStaticMeshEditor& InStaticMeshEditor )
	: StaticMeshEditor( InStaticMeshEditor )
{
	LODGroupNames.Reset();
	UStaticMesh::GetLODGroups(LODGroupNames);
	for (int32 GroupIndex = 0; GroupIndex < LODGroupNames.Num(); ++GroupIndex)
	{
		LODGroupOptions.Add(MakeShareable(new FString(LODGroupNames[GroupIndex].GetPlainNameString())));
	}

	for (int32 i = 0; i < MAX_STATIC_MESH_LODS; ++i)
	{
		bBuildSettingsExpanded[i] = false;
		bReductionSettingsExpanded[i] = false;
		bSectionSettingsExpanded[i] = (i == 0);

		LODScreenSizes[i] = 0.0f;
	}

	LODCount = StaticMeshEditor.GetStaticMesh()->GetNumLODs();

	UpdateLODNames();
}

/** Returns true if automatic mesh reduction is available. */
static bool IsAutoMeshReductionAvailable()
{
	static bool bAutoMeshReductionAvailable = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities").GetMeshReductionInterface() != NULL;
	return bAutoMeshReductionAvailable;
}

void FLevelOfDetailSettingsLayout::AddToDetailsPanel( IDetailLayoutBuilder& DetailBuilder )
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();

	IDetailCategoryBuilder& LODSettingsCategory =
		DetailBuilder.EditCategory( "LodSettings", LOCTEXT("LodSettingsCategory", "LOD Settings") );

	int32 LODGroupIndex = LODGroupNames.Find(StaticMesh->LODGroup);
	check(LODGroupIndex == INDEX_NONE || LODGroupIndex < LODGroupOptions.Num());

	LODSettingsCategory.AddCustomRow( LOCTEXT("LODGroup", "LOD Group") )
	.NameContent()
	[
		SNew(STextBlock)
		.Font( IDetailLayoutBuilder::GetDetailFont() )
		.Text(LOCTEXT("LODGroup", "LOD Group"))
	]
	.ValueContent()
	[
		SAssignNew(LODGroupComboBox, STextComboBox)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.OptionsSource(&LODGroupOptions)
		.InitiallySelectedItem(LODGroupOptions[(LODGroupIndex == INDEX_NONE) ? 0 : LODGroupIndex])
		.OnSelectionChanged(this, &FLevelOfDetailSettingsLayout::OnLODGroupChanged)
	];
	
	LODSettingsCategory.AddCustomRow( LOCTEXT("LODImport", "LOD Import") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("LODImport", "LOD Import"))
		]
	.ValueContent()
		[
			SNew(STextComboBox)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.OptionsSource(&LODNames)
			.InitiallySelectedItem(LODNames[0])
			.OnSelectionChanged(this, &FLevelOfDetailSettingsLayout::OnImportLOD)
		];

	LODSettingsCategory.AddCustomRow( LOCTEXT("MinLOD", "Minimum LOD") )
	.NameContent()
	[
		SNew(STextBlock)
		.Font( IDetailLayoutBuilder::GetDetailFont() )
		.Text(LOCTEXT("MinLOD", "Minimum LOD"))
	]
	.ValueContent()
	[
		SNew(SSpinBox<int32>)
		.Font( IDetailLayoutBuilder::GetDetailFont() )
		.Value(this, &FLevelOfDetailSettingsLayout::GetMinLOD)
		.OnValueChanged(this, &FLevelOfDetailSettingsLayout::OnMinLODChanged)
		.OnValueCommitted(this, &FLevelOfDetailSettingsLayout::OnMinLODCommitted)
		.MinValue(0)
		.MaxValue(MAX_STATIC_MESH_LODS)
		.ToolTipText(this, &FLevelOfDetailSettingsLayout::GetMinLODTooltip)
		.IsEnabled(FLevelOfDetailSettingsLayout::GetLODCount() > 1)
	];

	// Add Number of LODs slider.
	const int32 MinAllowedLOD = 1;
	LODSettingsCategory.AddCustomRow( LOCTEXT("NumberOfLODs", "Number of LODs") )
	.NameContent()
	[
		SNew(STextBlock)
		.Font( IDetailLayoutBuilder::GetDetailFont() )
		.Text(LOCTEXT("NumberOfLODs", "Number of LODs"))
	]
	.ValueContent()
	[
		SNew(SSpinBox<int32>)
		.Font( IDetailLayoutBuilder::GetDetailFont() )
		.Value(this, &FLevelOfDetailSettingsLayout::GetLODCount)
		.OnValueChanged(this, &FLevelOfDetailSettingsLayout::OnLODCountChanged)
		.OnValueCommitted(this, &FLevelOfDetailSettingsLayout::OnLODCountCommitted)
		.MinValue(MinAllowedLOD)
		.MaxValue(MAX_STATIC_MESH_LODS)
		.ToolTipText(this, &FLevelOfDetailSettingsLayout::GetLODCountTooltip)
		.IsEnabled(IsAutoMeshReductionAvailable())
	];

	// Auto LOD distance check box.
	LODSettingsCategory.AddCustomRow( LOCTEXT("AutoComputeLOD", "Auto Compute LOD Distances") )
	.NameContent()
	[
		SNew(STextBlock)
		.Font( IDetailLayoutBuilder::GetDetailFont() )
		.Text(LOCTEXT("AutoComputeLOD", "Auto Compute LOD Distances"))
	]
	.ValueContent()
	[
		SNew(SCheckBox)
		.IsChecked(this, &FLevelOfDetailSettingsLayout::IsAutoLODChecked)
		.OnCheckStateChanged(this, &FLevelOfDetailSettingsLayout::OnAutoLODChanged)
	];

	LODSettingsCategory.AddCustomRow( LOCTEXT("ApplyChanges", "Apply Changes") )
	.ValueContent()
	.HAlign(HAlign_Left)
	[
		SNew(SButton)
		.OnClicked(this, &FLevelOfDetailSettingsLayout::OnApply)
		.IsEnabled(this, &FLevelOfDetailSettingsLayout::IsApplyNeeded)
		[
			SNew( STextBlock )
			.Text(LOCTEXT("ApplyChanges", "Apply Changes"))
			.Font( DetailBuilder.GetDetailFont() )
		]
	];

	bool bAdvanced = true;
	// Allowed pixel error.
	LODSettingsCategory.AddCustomRow( LOCTEXT("AllowedPixelError", "Auto Distance Error"), bAdvanced )
	.NameContent()
	[
		SNew(STextBlock)
		.Font(FEditorStyle::GetFontStyle("StaticMeshEditor.NormalFont"))
		.Text(LOCTEXT("AllowedPixelError", "Auto Distance Error"))
	]
	.ValueContent()
	[
		SNew(SSpinBox<float>)
		.Font(FEditorStyle::GetFontStyle("StaticMeshEditor.NormalFont"))
		.MinValue(1.0f)
		.MaxValue(100.0f)
		.MinSliderValue(1.0f)
		.MaxSliderValue(5.0f)
		.Value(this, &FLevelOfDetailSettingsLayout::GetPixelError)
		.OnValueChanged(this, &FLevelOfDetailSettingsLayout::OnPixelErrorChanged)
		.IsEnabled(this, &FLevelOfDetailSettingsLayout::IsAutoLODEnabled)
	];

	AddLODLevelCategories( DetailBuilder );
}


void FLevelOfDetailSettingsLayout::AddLODLevelCategories( IDetailLayoutBuilder& DetailBuilder )
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
	
	if( StaticMesh )
	{
		const int32 StaticMeshLODCount = StaticMesh->GetNumLODs();
		FStaticMeshRenderData* RenderData = StaticMesh->RenderData;


		// Create information panel for each LOD level.
		for(int32 LODIndex = 0; LODIndex < StaticMeshLODCount; ++LODIndex)
		{
			if (IsAutoMeshReductionAvailable())
			{
				ReductionSettingsWidgets[LODIndex] = MakeShareable( new FMeshReductionSettingsLayout( AsShared() ) );
			}

			if (LODIndex < StaticMesh->SourceModels.Num())
			{
				FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[LODIndex];
				if (ReductionSettingsWidgets[LODIndex].IsValid())
				{
					ReductionSettingsWidgets[LODIndex]->UpdateSettings(SrcModel.ReductionSettings);
				}

				if (SrcModel.RawMeshBulkData->IsEmpty() == false)
				{
					BuildSettingsWidgets[LODIndex] = MakeShareable( new FMeshBuildSettingsLayout( AsShared() ) );
					BuildSettingsWidgets[LODIndex]->UpdateSettings(SrcModel.BuildSettings);
				}

				LODScreenSizes[LODIndex] = SrcModel.ScreenSize;
			}
			else if (LODIndex > 0)
			{
				if (ReductionSettingsWidgets[LODIndex].IsValid() && ReductionSettingsWidgets[LODIndex-1].IsValid())
				{
					FMeshReductionSettings ReductionSettings = ReductionSettingsWidgets[LODIndex-1]->GetSettings();
					// By default create LODs with half the triangles of the previous LOD.
					ReductionSettings.PercentTriangles *= 0.5f;
					ReductionSettingsWidgets[LODIndex]->UpdateSettings(ReductionSettings);
				}

				if(LODScreenSizes[LODIndex] >= LODScreenSizes[LODIndex-1])
				{
					const float DefaultScreenSizeDifference = 0.01f;
					LODScreenSizes[LODIndex] = LODScreenSizes[LODIndex-1] - DefaultScreenSizeDifference;
				}
			}

			FString CategoryName = FString(TEXT("LOD"));
			CategoryName.AppendInt( LODIndex );

			FText LODLevelString = FText::Format( LOCTEXT("LODLevel", "LOD{0}"), FText::AsNumber( LODIndex ) );

			IDetailCategoryBuilder& LODCategory = DetailBuilder.EditCategory( *CategoryName, LODLevelString, ECategoryPriority::Important );

			LODCategory.HeaderContent
			(
				SNew( SBox )
				.HAlign( HAlign_Right )
				[
					SNew( SHorizontalBox )
					+ SHorizontalBox::Slot()
					.Padding(FMargin(5.0f, 0.0f))
					.AutoWidth()
					[
						SNew(STextBlock)
						.Font(FEditorStyle::GetFontStyle("StaticMeshEditor.NormalFont"))
						.Text(this, &FLevelOfDetailSettingsLayout::GetLODScreenSizeTitle, LODIndex)
						.Visibility( LODIndex > 0 ? EVisibility::Visible : EVisibility::Collapsed )
					]
					+ SHorizontalBox::Slot()
					.Padding( FMargin( 5.0f, 0.0f ) )
					.AutoWidth()
					[
						SNew(STextBlock)
						.Font(FEditorStyle::GetFontStyle("StaticMeshEditor.NormalFont"))
						.Text( FText::Format( LOCTEXT("Triangles_MeshSimplification", "Triangles: {0}"), FText::AsNumber( StaticMeshEditor.GetNumTriangles(LODIndex) ) ) )
					]
					+ SHorizontalBox::Slot()
					.Padding( FMargin( 5.0f, 0.0f ) )
					.AutoWidth()
					[
						SNew(STextBlock)
						.Font(FEditorStyle::GetFontStyle("StaticMeshEditor.NormalFont"))
						.Text( FText::Format( LOCTEXT("Vertices_MeshSimplification", "Vertices: {0}"), FText::AsNumber( StaticMeshEditor.GetNumVertices(LODIndex) ) ) )
					]
				]
			);
			
			
			SectionSettingsWidgets[ LODIndex ] = MakeShareable( new FMeshSectionSettingsLayout( StaticMeshEditor, LODIndex ) );
			SectionSettingsWidgets[ LODIndex ]->AddToCategory( LODCategory );

			LODCategory.AddCustomRow(( LOCTEXT("ScreenSizeRow", "ScreenSize")))
			.NameContent()
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(LOCTEXT("ScreenSizeName", "Screen Size"))
			]
			.ValueContent()
			[
				SNew(SSpinBox<float>)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.MinValue(0.0f)
				.MaxValue(WORLD_MAX)
				.SliderExponent(2.0f)
				.Value(this, &FLevelOfDetailSettingsLayout::GetLODScreenSize, LODIndex)
				.OnValueChanged(this, &FLevelOfDetailSettingsLayout::OnLODScreenSizeChanged, LODIndex)
				.OnValueCommitted(this, &FLevelOfDetailSettingsLayout::OnLODScreenSizeCommitted, LODIndex)
				.IsEnabled(this, &FLevelOfDetailSettingsLayout::CanChangeLODScreenSize)
			];

			if (BuildSettingsWidgets[LODIndex].IsValid())
			{
				LODCategory.AddCustomBuilder( BuildSettingsWidgets[LODIndex].ToSharedRef() );
			}

			if( ReductionSettingsWidgets[LODIndex].IsValid() )
			{
				LODCategory.AddCustomBuilder( ReductionSettingsWidgets[LODIndex].ToSharedRef() );
			}

		}
	}

}


FLevelOfDetailSettingsLayout::~FLevelOfDetailSettingsLayout()
{
}

int32 FLevelOfDetailSettingsLayout::GetLODCount() const
{
	return LODCount;
}

float FLevelOfDetailSettingsLayout::GetLODScreenSize( int32 LODIndex ) const
{
	check(LODIndex < MAX_STATIC_MESH_LODS);
	UStaticMesh* Mesh = StaticMeshEditor.GetStaticMesh();
	float ScreenSize = LODScreenSizes[FMath::Clamp(LODIndex, 0, MAX_STATIC_MESH_LODS-1)];
	if(Mesh->bAutoComputeLODScreenSize)
	{
		ScreenSize = Mesh->RenderData->ScreenSize[LODIndex];
	}
	else if(Mesh->SourceModels.IsValidIndex(LODIndex))
	{
		ScreenSize = Mesh->SourceModels[LODIndex].ScreenSize;
	}
	return ScreenSize;
}

FText FLevelOfDetailSettingsLayout::GetLODScreenSizeTitle( int32 LODIndex ) const
{
	return FText::Format( LOCTEXT("ScreenSize_MeshSimplification", "Screen Size: {0}"), FText::AsNumber(GetLODScreenSize(LODIndex)));
}

bool FLevelOfDetailSettingsLayout::CanChangeLODScreenSize() const
{
	return !IsAutoLODEnabled();
}

void FLevelOfDetailSettingsLayout::OnLODScreenSizeChanged( float NewValue, int32 LODIndex )
{
	check(LODIndex < MAX_STATIC_MESH_LODS);
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
	if (!StaticMesh->bAutoComputeLODScreenSize)
	{
		// First propagate any changes from the source models to our local scratch.
		for (int32 i = 0; i < StaticMesh->SourceModels.Num(); ++i)
		{
			LODScreenSizes[i] = StaticMesh->SourceModels[i].ScreenSize;
		}

		// Update Display factors for further LODs
		const float MinimumDifferenceInScreenSize = KINDA_SMALL_NUMBER;
		LODScreenSizes[LODIndex] = NewValue;
		// Make sure we aren't trying to ovelap or have more than one LOD for a value
		for (int32 i = 1; i < MAX_STATIC_MESH_LODS; ++i)
		{
			float MaxValue = FMath::Clamp(LODScreenSizes[i-1] - MinimumDifferenceInScreenSize, 0.0f, 1.0f);
			LODScreenSizes[i] = FMath::Min(LODScreenSizes[i], MaxValue);
		}

		// Push changes immediately.
		for (int32 i = 0; i < MAX_STATIC_MESH_LODS; ++i)
		{
			if (StaticMesh->SourceModels.IsValidIndex(i))
			{
				StaticMesh->SourceModels[i].ScreenSize = LODScreenSizes[i];
			}
			if (StaticMesh->RenderData
				&& StaticMesh->RenderData->LODResources.IsValidIndex(i))
			{
				StaticMesh->RenderData->ScreenSize[i] = LODScreenSizes[i];
			}
		}

		// Reregister static mesh components using this mesh.
		{
			FStaticMeshComponentRecreateRenderStateContext ReregisterContext(StaticMesh,false);
			StaticMesh->Modify();
		}

		StaticMeshEditor.RefreshViewport();
	}
}

void FLevelOfDetailSettingsLayout::OnLODScreenSizeCommitted( float NewValue, ETextCommit::Type CommitType, int32 LODIndex )
{
	OnLODScreenSizeChanged(NewValue, LODIndex);
}

void FLevelOfDetailSettingsLayout::UpdateLODNames()
{
	LODNames.Empty();
	LODNames.Add( MakeShareable( new FString( LOCTEXT("BaseLOD", "Base LOD").ToString() ) ) );
	for(int32 LODLevelID = 1; LODLevelID < LODCount; ++LODLevelID)
	{
		LODNames.Add( MakeShareable( new FString( FText::Format( NSLOCTEXT("LODSettingsLayout", "LODLevel_Reimport", "Reimport LOD Level {0}"), FText::AsNumber( LODLevelID ) ).ToString() ) ) );
	}
	LODNames.Add( MakeShareable( new FString( FText::Format( NSLOCTEXT("LODSettingsLayout", "LODLevel_Import", "Import LOD Level {0}"), FText::AsNumber( LODCount ) ).ToString() ) ) );
}

void FLevelOfDetailSettingsLayout::OnBuildSettingsExpanded(bool bIsExpanded, int32 LODIndex)
{
	check(LODIndex >= 0 && LODIndex < MAX_STATIC_MESH_LODS);
	bBuildSettingsExpanded[LODIndex] = bIsExpanded;
}

void FLevelOfDetailSettingsLayout::OnReductionSettingsExpanded(bool bIsExpanded, int32 LODIndex)
{
	check(LODIndex >= 0 && LODIndex < MAX_STATIC_MESH_LODS);
	bReductionSettingsExpanded[LODIndex] = bIsExpanded;
}

void FLevelOfDetailSettingsLayout::OnSectionSettingsExpanded(bool bIsExpanded, int32 LODIndex)
{
	check(LODIndex >= 0 && LODIndex < MAX_STATIC_MESH_LODS);
	bSectionSettingsExpanded[LODIndex] = bIsExpanded;
}

void FLevelOfDetailSettingsLayout::OnLODGroupChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
	check(StaticMesh);
	int32 GroupIndex = LODGroupOptions.Find(NewValue);
	FName NewGroup = LODGroupNames[GroupIndex];
	if (StaticMesh->LODGroup != NewGroup)
	{
		EAppReturnType::Type DialogResult = FMessageDialog::Open(
			EAppMsgType::YesNo,
			FText::Format( LOCTEXT("ApplyDefaultLODSettings", "Changing LOD group will overwrite the current settings with the defaults from LOD group '{0}'. Do you wish to continue?"), FText::FromString( **NewValue ) )
			);
		if (DialogResult == EAppReturnType::Yes)
		{
			StaticMesh->Modify();
			StaticMesh->LODGroup = NewGroup;

			const ITargetPlatform* Platform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
			check(Platform);
			const FStaticMeshLODGroup& GroupSettings = Platform->GetStaticMeshLODSettings().GetLODGroup(NewGroup);

			// Set the number of LODs to at least the default. If there are already LODs they will be preserved, with default settings of the new LOD group.
			int32 DefaultLODCount = GroupSettings.GetDefaultNumLODs();

			while (StaticMesh->SourceModels.Num() < DefaultLODCount)
			{
				new(StaticMesh->SourceModels) FStaticMeshSourceModel();
			}
			LODCount = DefaultLODCount;

			// Set reduction settings to the defaults.
			for (int32 LODIndex = 0; LODIndex < LODCount; ++LODIndex)
			{
				StaticMesh->SourceModels[LODIndex].ReductionSettings = GroupSettings.GetDefaultSettings(LODIndex);
			}
			StaticMesh->bAutoComputeLODScreenSize = true;
			StaticMesh->LightMapResolution = GroupSettings.GetDefaultLightMapResolution();
			StaticMesh->PostEditChange();
			StaticMeshEditor.RefreshTool();
		}
		else
		{
			// Overriding the selection; ensure that the widget correctly reflects the property value
			int32 Index = LODGroupNames.Find(StaticMesh->LODGroup);
			check(Index != INDEX_NONE);
			LODGroupComboBox->SetSelectedItem(LODGroupOptions[Index]);
		}
	}
}

bool FLevelOfDetailSettingsLayout::IsAutoLODEnabled() const
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
	check(StaticMesh);
	return StaticMesh->bAutoComputeLODScreenSize;
}

ECheckBoxState FLevelOfDetailSettingsLayout::IsAutoLODChecked() const
{
	return IsAutoLODEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FLevelOfDetailSettingsLayout::OnAutoLODChanged(ECheckBoxState NewState)
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
	check(StaticMesh);
	StaticMesh->Modify();
	StaticMesh->bAutoComputeLODScreenSize = (NewState == ECheckBoxState::Checked) ? true : false;
	if (!StaticMesh->bAutoComputeLODScreenSize)
	{
		if (StaticMesh->SourceModels.IsValidIndex(0))
		{
			StaticMesh->SourceModels[0].ScreenSize = 1.0f;
		}
		for (int32 LODIndex = 1; LODIndex < StaticMesh->SourceModels.Num(); ++LODIndex)
		{
			StaticMesh->SourceModels[LODIndex].ScreenSize = StaticMesh->RenderData->ScreenSize[LODIndex];
		}
	}
	StaticMesh->PostEditChange();
	StaticMeshEditor.RefreshTool();
}

float FLevelOfDetailSettingsLayout::GetPixelError() const
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
	check(StaticMesh);
	return StaticMesh->AutoLODPixelError;
}

void FLevelOfDetailSettingsLayout::OnPixelErrorChanged(float NewValue)
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
	check(StaticMesh);
	{
		FStaticMeshComponentRecreateRenderStateContext ReregisterContext(StaticMesh,false);
		StaticMesh->AutoLODPixelError = NewValue;
		StaticMesh->RenderData->ResolveSectionInfo(StaticMesh);
		StaticMesh->Modify();
	}
	StaticMeshEditor.RefreshViewport();
}

void FLevelOfDetailSettingsLayout::OnImportLOD(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	int32 LODIndex = 0;
	if( LODNames.Find(NewValue, LODIndex) && LODIndex > 0 )
	{
		UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
		check(StaticMesh);
		FbxMeshUtils::ImportMeshLODDialog(StaticMesh,LODIndex);
		StaticMesh->PostEditChange();
		StaticMeshEditor.RefreshTool();
	}

}

bool FLevelOfDetailSettingsLayout::IsApplyNeeded() const
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
	check(StaticMesh);

	if (StaticMesh->SourceModels.Num() != LODCount)
	{
		return true;
	}

	for (int32 LODIndex = 0; LODIndex < LODCount; ++LODIndex)
	{
		FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[LODIndex];
		if (BuildSettingsWidgets[LODIndex].IsValid()
			&& SrcModel.BuildSettings != BuildSettingsWidgets[LODIndex]->GetSettings())
		{
			return true;
		}
		if (ReductionSettingsWidgets[LODIndex].IsValid()
			&& SrcModel.ReductionSettings != ReductionSettingsWidgets[LODIndex]->GetSettings())
		{
			return true;
		}
	}

	return false;
}

void FLevelOfDetailSettingsLayout::ApplyChanges()
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
	check(StaticMesh);

	// Calling Begin and EndSlowTask are rather dangerous because they tick
	// Slate. Call them here and flush rendering commands to be sure!.

	FFormatNamedArguments Args;
	Args.Add( TEXT("StaticMeshName"), FText::FromString( StaticMesh->GetName() ) );
	GWarn->BeginSlowTask( FText::Format( LOCTEXT("ApplyLODChanges", "Applying changes to {StaticMeshName}..."), Args ), true );
	FlushRenderingCommands();

	StaticMesh->Modify();
	if (StaticMesh->SourceModels.Num() > LODCount)
	{
		int32 NumToRemove = StaticMesh->SourceModels.Num() - LODCount;
		StaticMesh->SourceModels.RemoveAt(LODCount, NumToRemove);
	}
	while (StaticMesh->SourceModels.Num() < LODCount)
	{
		new(StaticMesh->SourceModels) FStaticMeshSourceModel();
	}
	check(StaticMesh->SourceModels.Num() == LODCount);

	for (int32 LODIndex = 0; LODIndex < LODCount; ++LODIndex)
	{
		FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[LODIndex];
		if (BuildSettingsWidgets[LODIndex].IsValid())
		{
			SrcModel.BuildSettings = BuildSettingsWidgets[LODIndex]->GetSettings();
		}
		if (ReductionSettingsWidgets[LODIndex].IsValid())
		{
			SrcModel.ReductionSettings = ReductionSettingsWidgets[LODIndex]->GetSettings();
		}

		if (LODIndex == 0)
		{
			SrcModel.ScreenSize = 1.0f;
		}
		else
		{
			SrcModel.ScreenSize = LODScreenSizes[LODIndex];
			FStaticMeshSourceModel& PrevModel = StaticMesh->SourceModels[LODIndex-1];
			if(SrcModel.ScreenSize >= PrevModel.ScreenSize)
			{
				const float DefaultScreenSizeDifference = 0.01f;
				LODScreenSizes[LODIndex] = LODScreenSizes[LODIndex-1] - DefaultScreenSizeDifference;

				// Make sure there are no incorrectly overlapping values
				SrcModel.ScreenSize = 1.0f - 0.01f * LODIndex;
			}
		}
	}
	StaticMesh->PostEditChange();

	GWarn->EndSlowTask();

	StaticMeshEditor.RefreshTool();
}

FReply FLevelOfDetailSettingsLayout::OnApply()
{
	ApplyChanges();
	return FReply::Handled();
}

void FLevelOfDetailSettingsLayout::OnLODCountChanged(int32 NewValue)
{
	LODCount = FMath::Clamp<int32>(NewValue, 1, MAX_STATIC_MESH_LODS);

	UpdateLODNames();
}

void FLevelOfDetailSettingsLayout::OnLODCountCommitted(int32 InValue, ETextCommit::Type CommitInfo)
{
	OnLODCountChanged(InValue);
}

FText FLevelOfDetailSettingsLayout::GetLODCountTooltip() const
{
	if(IsAutoMeshReductionAvailable())
	{
		return LOCTEXT("LODCountTooltip", "The number of LODs for this static mesh. If auto mesh reduction is available, setting this number will determine the number of LOD levels to auto generate.");
	}

	return LOCTEXT("LODCountTooltip_Disabled", "Auto mesh reduction is unavailable! Please provide a mesh reduction interface such as Simplygon to use this feature or manually import LOD levels.");
}

int32 FLevelOfDetailSettingsLayout::GetMinLOD() const
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
	check(StaticMesh);

	return StaticMesh->MinLOD;
}

void FLevelOfDetailSettingsLayout::OnMinLODChanged(int32 NewValue)
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
	check(StaticMesh);

	{
		FStaticMeshComponentRecreateRenderStateContext ReregisterContext(StaticMesh,false);
		StaticMesh->MinLOD = FMath::Clamp<int32>(NewValue, 0, MAX_STATIC_MESH_LODS - 1);
		StaticMesh->Modify();
	}
	StaticMeshEditor.RefreshViewport();
}

void FLevelOfDetailSettingsLayout::OnMinLODCommitted(int32 InValue, ETextCommit::Type CommitInfo)
{
	OnMinLODChanged(InValue);
}

FText FLevelOfDetailSettingsLayout::GetMinLODTooltip() const
{
	return LOCTEXT("MinLODTooltip", "The minimum LOD to use for rendering.  This can be overridden in components.");
}

#undef LOCTEXT_NAMESPACE
