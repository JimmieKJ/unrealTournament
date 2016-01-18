// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SpeedTreeImporterPrivatePCH.h"
#include "SpeedTreeImportFactory.h"

#include "UnrealEd.h"

#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionSpeedTree.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTwoSidedSign.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunction.h"

#include "MainFrame.h"
#include "ModuleManager.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "AssetRegistryModule.h"
#include "../Private/GeomFitUtils.h"
#include "SpeedTreeWind.h"
#include "RawMesh.h"
#include "ComponentReregisterContext.h"

#if WITH_SPEEDTREE

#ifdef __clang__ // @todo: ThirdParty/SpeedTree/SpeedTreeSDK-v7.0/Include/Core/RenderState_inl.h:52:2: warning: field 'm_eHueVariation' will be initialized after field 'm_eShaderGenerationMode' [-Wreorder]
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wreorder"
#endif

#include "Core/Core.h"

#ifdef __clang__
	#pragma clang diagnostic pop
#endif

#endif // WITH_SPEEDTREE
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"

#define LOCTEXT_NAMESPACE "SpeedTreeImportFactory"

DEFINE_LOG_CATEGORY_STATIC(LogSpeedTreeImport, Log, All);

/** UI to pick options when importing  SpeedTree */
BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
class SSpeedTreeImportOptions : public SCompoundWidget
{
public:

	/** Geometry import type */
	enum EImportGeometryType
	{
		IGT_3D,
		IGT_Billboards,
		IGT_Both
	};
	EImportGeometryType				ImportGeometryType;

	/** Tree scale **/
	float							TreeScale;

	/** LOD type **/
	enum EImportLODType
	{
		ILT_PaintedFoliage,
		ILT_IndividualActors
	};
	EImportLODType					LODType;

	/** options */
	TSharedPtr<SCheckBox>			MakeMaterialsCheck;
	TSharedPtr<SCheckBox>			IncludeNormalMapCheck;
	TSharedPtr<SCheckBox>			IncludeDetailMapCheck;
	TSharedPtr<SCheckBox>			IncludeSpecularMapCheck;
	TSharedPtr<SCheckBox>			IncludeVertexProcessingCheck;
	TSharedPtr<SCheckBox>			IncludeWindCheck;
	TSharedPtr<SCheckBox>			IncludeSmoothLODCheck;
	TSharedPtr<SCheckBox>			IncludeCollision;
	TSharedPtr<SCheckBox>			IncludeBranchSeamSmoothing;
	TSharedPtr<SCheckBox>			IncludeSpeedTreeAO;
	TSharedPtr<SCheckBox>			IncludeColorAdjustment;

	/** Whether we should go ahead with import */
	bool							bImport;

	/** Window that owns us */
	TSharedPtr<SWindow>				WidgetWindow;

public:
	SLATE_BEGIN_ARGS(SSpeedTreeImportOptions) 
		: _WidgetWindow()
		{}

		SLATE_ARGUMENT(TSharedPtr<SWindow>, WidgetWindow)
	SLATE_END_ARGS()

	SSpeedTreeImportOptions() :
		ImportGeometryType(IGT_3D),
		TreeScale(30.48f),
		LODType(ILT_PaintedFoliage),
		bImport(false)
	{}

	TSharedRef<SWidget> CreateOption(TSharedPtr<SCheckBox>& CheckBox, const FText& NameText, bool bChecked)
	{
		return SAssignNew(CheckBox, SCheckBox)
		.IsChecked(bChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
		.OnCheckStateChanged(this, &SSpeedTreeImportOptions::OnOptionModified)
		.Content()
		[
			SNew(STextBlock).Text(NameText)
		];
	}

	TSharedRef<SWidget> CreateGeometryTypeCombo(const FText& NameText, bool bChecked, EImportGeometryType Type)
	{
		return SNew(SCheckBox)
		.Style(FEditorStyle::Get(), "RadioButton")
		.IsChecked(this, &SSpeedTreeImportOptions::IsGeometryTypeRadioChecked, Type)
		.OnCheckStateChanged(this, &SSpeedTreeImportOptions::OnGeometryTypeRadioChanged, Type)
		.Content()
		[
			SNew(STextBlock).Text(NameText)
		];
	}

	ECheckBoxState IsGeometryTypeRadioChecked(EImportGeometryType ButtonId) const
	{
		return (ImportGeometryType == ButtonId) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	void OnGeometryTypeRadioChanged(ECheckBoxState NewRadioState, EImportGeometryType RadioThatChanged )
	{
		if (NewRadioState == ECheckBoxState::Checked)
		{
			ImportGeometryType = RadioThatChanged;
		}
	}

	TSharedRef<SWidget> CreateLodTypeCombo(const FText& NameText, bool bChecked, EImportLODType Type)
	{
		return SNew(SCheckBox)
		.Style(FEditorStyle::Get(), "RadioButton")
		.IsChecked(this, &SSpeedTreeImportOptions::IsLodTypeRadioChecked, Type)
		.OnCheckStateChanged(this, &SSpeedTreeImportOptions::OnLodTypeRadioChanged, Type)
		.Content()
		[
			SNew(STextBlock).Text(NameText)
		];
	}

	ECheckBoxState IsLodTypeRadioChecked(EImportLODType ButtonId) const
	{
		return (LODType == ButtonId) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	void OnLodTypeRadioChanged(ECheckBoxState NewRadioState, EImportLODType RadioThatChanged)
	{
		if (NewRadioState == ECheckBoxState::Checked)
		{
			LODType = RadioThatChanged;
		}
	}

	void Construct(const FArguments& InArgs)
	{
		WidgetWindow = InArgs._WidgetWindow;

		// Create widget
		this->ChildSlot
		[
			SNew(SBorder)
			. BorderImage(FEditorStyle::GetBrush(TEXT("Menu.Background")))
			. Content()
			[
				SNew(SVerticalBox)

				// options
				+SVerticalBox::Slot().AutoHeight().Padding(5)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot().AutoHeight().Padding(5).HAlign(HAlign_Left)
					[
						SNew(STextBlock).Text(LOCTEXT("TreeScaleLabel", "Tree Scale"))
					]

					+SVerticalBox::Slot().AutoHeight().Padding(5)
					[
						SNew(SEditableTextBox)
						.SelectAllTextWhenFocused(true)
						.OnTextCommitted(this, &SSpeedTreeImportOptions::ScaleTextCommitted)
						.Text(FText::AsNumber(TreeScale))
					]
				]

				+SVerticalBox::Slot().AutoHeight().Padding(5)
				[
					SNew(SVerticalBox)

					+SVerticalBox::Slot().AutoHeight().Padding(5).HAlign(HAlign_Left)
					[
						SNew(STextBlock).Text(LOCTEXT("GeometryCategoryLabel", "Geometry"))
					]

					+SVerticalBox::Slot().AutoHeight().Padding(15, 5)
					[
						SNew(SVerticalBox)
					
						+SVerticalBox::Slot().AutoHeight().Padding(5)
						[
							CreateGeometryTypeCombo(LOCTEXT("3D_LODs", "3D LODs"), true, IGT_3D)
						]

						+SVerticalBox::Slot().AutoHeight().Padding(5)
						[
							CreateGeometryTypeCombo(LOCTEXT("Billboards", "Billboards"), false, IGT_Billboards)
						]

						+SVerticalBox::Slot().AutoHeight().Padding(5)
						[
							CreateGeometryTypeCombo(LOCTEXT("Both", "Both"), false, IGT_Both)
						]
					]
				]

				+SVerticalBox::Slot().AutoHeight().Padding(5)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot().AutoHeight().Padding(5).HAlign(HAlign_Left)
					[
						SNew(STextBlock).Text(LOCTEXT("LODSetupLabel", "LOD Setup"))
					]

					+SVerticalBox::Slot().AutoHeight().Padding(15, 5)
					[
						SNew(SVerticalBox)
						
						+SVerticalBox::Slot().AutoHeight().Padding(5)
						[
							CreateLodTypeCombo(LOCTEXT("PaintedFoliage", "Painted Foliage"), true, ILT_PaintedFoliage)
						]

						+SVerticalBox::Slot().AutoHeight().Padding(5)
						[
							CreateLodTypeCombo(LOCTEXT("IndividualActors", "Individual Actors"), false, ILT_IndividualActors)
						]
					]
				]

				+SVerticalBox::Slot().AutoHeight().Padding(5)
				[
					SNew(SVerticalBox)

					+SVerticalBox::Slot().AutoHeight().Padding(5)
					[
						CreateOption(IncludeCollision, LOCTEXT("SetupCollision", "Setup Collision"), true)
					]

					+SVerticalBox::Slot().AutoHeight().Padding(5)
					[
						CreateOption(MakeMaterialsCheck, LOCTEXT("CreateMaterials", "Create Materials"), true)
					]

					+SVerticalBox::Slot().AutoHeight().Padding(15, 5)
					[
						SNew(SVerticalBox)

						+SVerticalBox::Slot().AutoHeight().Padding(5)
						[
							CreateOption(IncludeNormalMapCheck, LOCTEXT("IncludeNormalMaps", "Include Normal Maps"), true)
						]

						+SVerticalBox::Slot().AutoHeight().Padding(5)
						[
							CreateOption(IncludeDetailMapCheck, LOCTEXT("IncludeDetailMaps", "Include Detail Maps"), true)
						]

						+SVerticalBox::Slot().AutoHeight().Padding(5)
						[
							CreateOption(IncludeSpecularMapCheck, LOCTEXT("IncludeSpecularMaps", "Include Specular Maps"), false)
						]

						+SVerticalBox::Slot().AutoHeight().Padding(5)
						[
							CreateOption(IncludeBranchSeamSmoothing, LOCTEXT("IncludeBranchSeamSmoothing", "Include Branch Seam Smoothing"), false)
						]

						+SVerticalBox::Slot().AutoHeight().Padding(5)
						[
							CreateOption(IncludeSpeedTreeAO, LOCTEXT("IncludeSpeedTreeAO", "Include SpeedTree AO"), true)
						]

						+ SVerticalBox::Slot().AutoHeight().Padding(5)
						[
							CreateOption(IncludeColorAdjustment, LOCTEXT("Include Random Color Variation", "Include Random Color Variation"), true)
						]

						+SVerticalBox::Slot().AutoHeight().Padding(5, 10, 5, 5)
						[
							CreateOption(IncludeVertexProcessingCheck, LOCTEXT("IncludeVertexProcessing", "Include Vertex Processing"), true)
						]

						+SVerticalBox::Slot().AutoHeight().Padding(15, 5)
						[
							SNew(SVerticalBox)

							+SVerticalBox::Slot().AutoHeight().Padding(5)
							[
								CreateOption(IncludeWindCheck, LOCTEXT("IncludeWind", "Include Wind"), true)
							]

							+SVerticalBox::Slot().AutoHeight().Padding(5)
							[
								CreateOption(IncludeSmoothLODCheck, LOCTEXT("IncludeSmoothLOD", "Include Smooth LOD"), true)
							]
						]
					]
				]				
				
				// Ok/Cancel
				+SVerticalBox::Slot().HAlign(HAlign_Center).Padding(5)
				[
					SNew(SUniformGridPanel).SlotPadding(3)
					+SUniformGridPanel::Slot(0,0)
					[
						SNew(SButton).HAlign(HAlign_Center)
						.Text(LOCTEXT("FbxOptionWindow_Import", "Import"))
						.OnClicked(this, &SSpeedTreeImportOptions::OnImport)
					]
					+SUniformGridPanel::Slot(1,0)
					[
						SNew(SButton).HAlign(HAlign_Center)
						.Text(LOCTEXT("FbxOptionWindow_Cancel", "Cancel"))
						.OnClicked(this, &SSpeedTreeImportOptions::OnCancel)
					]
				]
			]
		];
		
	}

	/** If we should import */
	bool ShouldImport()
	{
		return bImport;
	}

	/** Called when 'OK' button is pressed */
	FReply OnImport()
	{
		bImport = true;
		WidgetWindow->RequestDestroyWindow();
		return FReply::Handled();
	}

	/** Called when 'Cancel' button is pressed */
	FReply OnCancel()
	{
		bImport = false;
		WidgetWindow->RequestDestroyWindow();
		return FReply::Handled();
	}

	void OnOptionModified(const ECheckBoxState /*NewCheckedState*/)
	{
		bool bEnable = MakeMaterialsCheck->IsChecked();

		IncludeNormalMapCheck->SetEnabled(bEnable);
		IncludeDetailMapCheck->SetEnabled(bEnable);
		IncludeSpecularMapCheck->SetEnabled(bEnable);
		IncludeBranchSeamSmoothing->SetEnabled(bEnable);
		IncludeVertexProcessingCheck->SetEnabled(bEnable);
		IncludeSpeedTreeAO->SetEnabled(bEnable);
		IncludeColorAdjustment->SetEnabled(bEnable);

		bEnable &= IncludeVertexProcessingCheck->IsChecked();
		IncludeWindCheck->SetEnabled(bEnable);
		IncludeSmoothLODCheck->SetEnabled(bEnable);
	}

	void ScaleTextCommitted(const FText& CommentText, ETextCommit::Type CommitInfo)
	{
		TTypeFromString<float>::FromString(TreeScale, *(CommentText.ToString()));
	}
};
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

//////////////////////////////////////////////////////////////////////////

USpeedTreeImportFactory::USpeedTreeImportFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = false;
	bEditAfterNew = true;
	SupportedClass = UStaticMesh::StaticClass();

	bEditorImport = true;
	bText = false;

#if WITH_SPEEDTREE
	Formats.Add(TEXT("srt;SpeedTree"));
#endif
}

FText USpeedTreeImportFactory::GetDisplayName() const
{
	return LOCTEXT("SpeedTreeImportFactoryDescription", "SpeedTree");
}


#if WITH_SPEEDTREE

bool USpeedTreeImportFactory::DoesSupportClass(UClass * Class)
{
	return (Class == UStaticMesh::StaticClass());
}

UClass* USpeedTreeImportFactory::ResolveSupportedClass()
{
	return UStaticMesh::StaticClass();
}

UTexture* CreateSpeedTreeMaterialTexture(UObject* Parent, FString Filename, bool bNormalMap, TSet<UPackage*>& LoadedPackages)
{
	UTexture* UnrealTexture = NULL;

	if (Filename.IsEmpty( ))
	{
		return UnrealTexture;
	}

	FString Extension = FPaths::GetExtension(Filename).ToLower();
	FString TextureName = FPaths::GetBaseFilename(Filename) + TEXT("_Tex");
	TextureName = ObjectTools::SanitizeObjectName(TextureName);

	// set where to place the textures
	FString NewPackageName = FPackageName::GetLongPackagePath(Parent->GetOutermost()->GetName()) + TEXT("/") + TextureName;
	NewPackageName = PackageTools::SanitizePackageName(NewPackageName);
	UPackage* Package = CreatePackage(NULL, *NewPackageName);

	// find existing texture
	UnrealTexture = FindObject<UTexture>(Package, *TextureName);
	if (UnrealTexture != NULL)
	{
		if (!LoadedPackages.Contains(Package))
		{
			LoadedPackages.Add(Package);
			if (!FReimportManager::Instance()->Reimport(UnrealTexture, true))
			{
				UE_LOG(LogSpeedTreeImport, Warning, TEXT("Manual texture reimport and recompression may be needed for %s"), *TextureName);
			}
		}

		return UnrealTexture;
	}

	// try opening from absolute path
	Filename = FPaths::GetPath(UFactory::GetCurrentFilename()) + "/" + Filename;
	TArray<uint8> TextureData;
	if (!(FFileHelper::LoadFileToArray(TextureData, *Filename) && TextureData.Num() > 0))
	{
		UE_LOG(LogSpeedTreeImport, Warning, TEXT("Unable to find Texture file %s"), *Filename);
	}
	else
	{
		auto TextureFact = NewObject<UTextureFactory>();
		TextureFact->AddToRoot();
		TextureFact->SuppressImportOverwriteDialog();

		if (bNormalMap)
		{
			TextureFact->LODGroup = TEXTUREGROUP_WorldNormalMap;
			TextureFact->CompressionSettings = TC_Normalmap;
		}

		const uint8* PtrTexture = TextureData.GetData();
		UnrealTexture = (UTexture*)TextureFact->FactoryCreateBinary(UTexture2D::StaticClass(), Package, *TextureName, RF_Standalone|RF_Public, NULL, *Extension, PtrTexture, PtrTexture + TextureData.Num(), GWarn);
		if (UnrealTexture != NULL)
		{
			UnrealTexture->AssetImportData->Update(Filename);

			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(UnrealTexture);

			// Set the dirty flag so this package will get saved later
			Package->SetDirtyFlag(true);
			LoadedPackages.Add(Package);
		}

		TextureFact->RemoveFromRoot();
	}

	return UnrealTexture;
}

void LayoutMaterial(UMaterialInterface* MaterialInterface)
{
	UMaterial* Material = MaterialInterface->GetMaterial();
	Material->EditorX = 0;
	Material->EditorY = 0;

	const int32 Height = 200;
	const int32 Width = 250;

	// layout X to make sure each input is 1 step further than output connection
	bool bContinue = true;
	while (bContinue)
	{
		bContinue = false;

		for (int32 ExpressionIndex = 0; ExpressionIndex < Material->Expressions.Num(); ++ExpressionIndex)
		{
			UMaterialExpression* Expression = Material->Expressions[ExpressionIndex];
			Expression->MaterialExpressionEditorX = FMath::Min(Expression->MaterialExpressionEditorX, -Width);

			TArray<FExpressionInput*> Inputs = Expression->GetInputs();
			for (int32 InputIndex = 0; InputIndex < Inputs.Num(); ++InputIndex)
			{
				UMaterialExpression* Input = Inputs[InputIndex]->Expression;
				if (Input != NULL)
				{
					if (Input->MaterialExpressionEditorX > Expression->MaterialExpressionEditorX - Width)
					{
						Input->MaterialExpressionEditorX = Expression->MaterialExpressionEditorX - Width;
						bContinue = true;
					}
				}
			}
		}
	}

	// run through each column of expressions, sort them by outputs, and layout Y
	bContinue = true;
	int32 Column = 1;
	while (bContinue)
	{
		TArray<UMaterialExpression*> ColumnExpressions;
		for (int32 ExpressionIndex = 0; ExpressionIndex < Material->Expressions.Num(); ++ExpressionIndex)
		{
			UMaterialExpression* Expression = Material->Expressions[ExpressionIndex];

			if (Expression->MaterialExpressionEditorX == -Width * Column)
			{
				Expression->MaterialExpressionEditorY = 0;
				int32 NumOutputs = 0;

				// all the connections to the material
				for (int32 MaterialPropertyIndex = 0; MaterialPropertyIndex < MP_MAX; ++MaterialPropertyIndex)
				{
					FExpressionInput* FirstLevelExpression = Material->GetExpressionInputForProperty(EMaterialProperty(MaterialPropertyIndex));
					if (FirstLevelExpression != NULL && FirstLevelExpression->Expression == Expression)
					{
						++NumOutputs;
						Expression->MaterialExpressionEditorY += MaterialPropertyIndex * 20;
					}
				}
				
				// all the outputs to other expressions
				for (int32 OtherExpressionIndex = 0; OtherExpressionIndex < Material->Expressions.Num(); ++OtherExpressionIndex)
				{
					UMaterialExpression* OtherExpression = Material->Expressions[OtherExpressionIndex];
					TArray<FExpressionInput*> Inputs = OtherExpression->GetInputs();
					for (int32 InputIndex = 0; InputIndex < Inputs.Num(); ++InputIndex)
					{
						if (Inputs[InputIndex]->Expression == Expression)
						{
							++NumOutputs;
							Expression->MaterialExpressionEditorY += OtherExpression->MaterialExpressionEditorY;
						}
					}
				}

				if (NumOutputs > 1)
				{
					Expression->MaterialExpressionEditorY /= NumOutputs;
				}

				ColumnExpressions.Add(Expression);
			}
		}

		struct FMaterialExpressionSorter
		{
			bool operator()(const UMaterialExpression& A, const UMaterialExpression& B) const
			{
				return (A.MaterialExpressionEditorY < B.MaterialExpressionEditorY);
			}
		};
		ColumnExpressions.Sort(FMaterialExpressionSorter());

		for (int32 ExpressionIndex = 0; ExpressionIndex < ColumnExpressions.Num(); ++ExpressionIndex)
		{
			UMaterialExpression* Expression = ColumnExpressions[ExpressionIndex];

			Expression->MaterialExpressionEditorY = ExpressionIndex * Height;
		}

		++Column;
		bContinue = (ColumnExpressions.Num() > 0);
	}
}

UMaterialInterface* CreateSpeedTreeMaterial(UObject* Parent, FString MaterialFullName, const SpeedTree::SRenderState* RenderState, TSharedPtr<SSpeedTreeImportOptions> Options, ESpeedTreeWindType WindType, int NumBillboards, TSet<UPackage*>& LoadedPackages)
{
	// Make sure we have a parent
	if (!Options->MakeMaterialsCheck->IsChecked() || !ensure(Parent))
	{
		return UMaterial::GetDefaultMaterial(MD_Surface);
	}
	
	// set where to place the materials
	FString FixedMaterialName = MaterialFullName + TEXT("_Mat");
	FString NewPackageName = FPackageName::GetLongPackagePath(Parent->GetOutermost()->GetName()) + TEXT("/") + FixedMaterialName;
	NewPackageName = PackageTools::SanitizePackageName(NewPackageName);
	UPackage* Package = CreatePackage(NULL, *NewPackageName);
	
	// does not override existing materials
	UMaterialInterface* UnrealMaterialInterface = FindObject<UMaterialInterface>(Package, *FixedMaterialName);
	if (UnrealMaterialInterface != NULL)
	{
		// touch the textures anyway to make sure they reload if necessary
		UTexture* DiffuseTexture = CreateSpeedTreeMaterialTexture(Parent, ANSI_TO_TCHAR(RenderState->m_apTextures[SpeedTree::TL_DIFFUSE]), false, LoadedPackages);
		if (DiffuseTexture)
		{
			if (RenderState->m_bBranchesPresent && Options->IncludeDetailMapCheck->IsChecked())
			{
				UTexture* DetailTexture = CreateSpeedTreeMaterialTexture(Parent, ANSI_TO_TCHAR(RenderState->m_apTextures[SpeedTree::TL_DETAIL_DIFFUSE]), false, LoadedPackages);
			}
		}
		if (Options->IncludeSpecularMapCheck->IsChecked())
		{
			UTexture* SpecularTexture = CreateSpeedTreeMaterialTexture(Parent, ANSI_TO_TCHAR(RenderState->m_apTextures[SpeedTree::TL_SPECULAR_MASK]), false, LoadedPackages);
		}
		if (Options->IncludeNormalMapCheck->IsChecked())
		{
			UTexture* NormalTexture = CreateSpeedTreeMaterialTexture(Parent, ANSI_TO_TCHAR(RenderState->m_apTextures[SpeedTree::TL_NORMAL]), true, LoadedPackages);
		}

		return UnrealMaterialInterface;
	}
	
	// create an unreal material asset
	auto MaterialFactory = NewObject<UMaterialFactoryNew>();
	UMaterial* UnrealMaterial = (UMaterial*)MaterialFactory->FactoryCreateNew(UMaterial::StaticClass(), Package, *FixedMaterialName, RF_Standalone|RF_Public, NULL, GWarn);
	FAssetRegistryModule::AssetCreated(UnrealMaterial);
	Package->SetDirtyFlag(true);

	if (!RenderState->m_bDiffuseAlphaMaskIsOpaque && !RenderState->m_bBranchesPresent && !RenderState->m_bRigidMeshesPresent)
	{
		UnrealMaterial->BlendMode = BLEND_Masked;
		UnrealMaterial->SetCastShadowAsMasked(true);
		UnrealMaterial->TwoSided = !(RenderState->m_bHorzBillboard || RenderState->m_bVertBillboard);
	}

	UMaterialExpressionClamp* BranchSeamAmount = NULL;
	if (Options->IncludeBranchSeamSmoothing->IsChecked() && RenderState->m_bBranchesPresent && RenderState->m_eBranchSeamSmoothing != SpeedTree::EFFECT_OFF)
	{
		UMaterialExpressionTextureCoordinate* SeamTexcoordExpression = NewObject<UMaterialExpressionTextureCoordinate>(UnrealMaterial);
		SeamTexcoordExpression->CoordinateIndex = 4;
		UnrealMaterial->Expressions.Add(SeamTexcoordExpression);

		UMaterialExpressionComponentMask* ComponentMaskExpression = NewObject<UMaterialExpressionComponentMask>(UnrealMaterial);
		ComponentMaskExpression->R = 0;
		ComponentMaskExpression->G = 1;
		ComponentMaskExpression->B = 0;
		ComponentMaskExpression->A = 0;
		ComponentMaskExpression->Input.Expression = SeamTexcoordExpression;
		UnrealMaterial->Expressions.Add(ComponentMaskExpression);

		UMaterialExpressionPower* PowerExpression = NewObject<UMaterialExpressionPower>(UnrealMaterial);
		PowerExpression->Base.Expression = ComponentMaskExpression;
		PowerExpression->ConstExponent = RenderState->m_fBranchSeamWeight;
		UnrealMaterial->Expressions.Add(PowerExpression);

		BranchSeamAmount = NewObject<UMaterialExpressionClamp>(UnrealMaterial);
		BranchSeamAmount->Input.Expression = PowerExpression;
		UnrealMaterial->Expressions.Add(BranchSeamAmount);
	}

	// textures and properties
	UTexture* DiffuseTexture = CreateSpeedTreeMaterialTexture(Parent, ANSI_TO_TCHAR(RenderState->m_apTextures[SpeedTree::TL_DIFFUSE]), false, LoadedPackages);
	if (DiffuseTexture)
	{
		// make texture sampler
		UMaterialExpressionTextureSample* TextureExpression = NewObject<UMaterialExpressionTextureSample>(UnrealMaterial);
		TextureExpression->Texture = DiffuseTexture;
		TextureExpression->SamplerType = SAMPLERTYPE_Color;
		UnrealMaterial->Expressions.Add(TextureExpression);

		// hook to the material diffuse/mask
		UnrealMaterial->BaseColor.Expression = TextureExpression;
		UnrealMaterial->OpacityMask.Expression = TextureExpression;
		UnrealMaterial->OpacityMask.Mask = UnrealMaterial->OpacityMask.Expression->GetOutputs()[0].Mask;
		UnrealMaterial->OpacityMask.MaskR = 0;
		UnrealMaterial->OpacityMask.MaskG = 0;
		UnrealMaterial->OpacityMask.MaskB = 0;
		UnrealMaterial->OpacityMask.MaskA = 1;

		if (BranchSeamAmount != NULL)
		{
			// perform branch seam smoothing
			UMaterialExpressionTextureCoordinate* SeamTexcoordExpression = NewObject<UMaterialExpressionTextureCoordinate>(UnrealMaterial);
			SeamTexcoordExpression->CoordinateIndex = 6;
			UnrealMaterial->Expressions.Add(SeamTexcoordExpression);

			UMaterialExpressionTextureSample* SeamTextureExpression = NewObject<UMaterialExpressionTextureSample>(UnrealMaterial);
			SeamTextureExpression->Texture = DiffuseTexture;
			SeamTextureExpression->SamplerType = SAMPLERTYPE_Color;
			SeamTextureExpression->Coordinates.Expression = SeamTexcoordExpression;
			UnrealMaterial->Expressions.Add(SeamTextureExpression);

			UMaterialExpressionLinearInterpolate* InterpolateExpression = NewObject<UMaterialExpressionLinearInterpolate>(UnrealMaterial);
			InterpolateExpression->A.Expression = SeamTextureExpression;
			InterpolateExpression->B.Expression = TextureExpression;
			InterpolateExpression->Alpha.Expression = BranchSeamAmount;
			UnrealMaterial->Expressions.Add(InterpolateExpression);

			UnrealMaterial->BaseColor.Expression = InterpolateExpression;
		}

		if (RenderState->m_bBranchesPresent && Options->IncludeDetailMapCheck->IsChecked())
		{
			UTexture* DetailTexture = CreateSpeedTreeMaterialTexture(Parent, ANSI_TO_TCHAR(RenderState->m_apTextures[SpeedTree::TL_DETAIL_DIFFUSE]), false, LoadedPackages);
			if (DetailTexture)
			{
				// add/find UVSet
				UMaterialExpressionTextureCoordinate* DetailTexcoordExpression = NewObject<UMaterialExpressionTextureCoordinate>(UnrealMaterial);
				DetailTexcoordExpression->CoordinateIndex = 5;
				UnrealMaterial->Expressions.Add(DetailTexcoordExpression);
				
				// make texture sampler
				UMaterialExpressionTextureSample* DetailTextureExpression = NewObject<UMaterialExpressionTextureSample>(UnrealMaterial);
				DetailTextureExpression->Texture = DetailTexture;
				DetailTextureExpression->SamplerType = SAMPLERTYPE_Color;
				DetailTextureExpression->Coordinates.Expression = DetailTexcoordExpression;
				UnrealMaterial->Expressions.Add(DetailTextureExpression);

				// interpolate the detail
				UMaterialExpressionLinearInterpolate* InterpolateExpression = NewObject<UMaterialExpressionLinearInterpolate>(UnrealMaterial);
				InterpolateExpression->A.Expression = UnrealMaterial->BaseColor.Expression;
				InterpolateExpression->B.Expression = DetailTextureExpression;
				InterpolateExpression->Alpha.Expression = DetailTextureExpression;
				InterpolateExpression->Alpha.Mask = DetailTextureExpression->GetOutputs()[0].Mask;
				InterpolateExpression->Alpha.MaskR = 0;
				InterpolateExpression->Alpha.MaskG = 0;
				InterpolateExpression->Alpha.MaskB = 0;
				InterpolateExpression->Alpha.MaskA = 1;
				UnrealMaterial->Expressions.Add(InterpolateExpression);

				// hook final to diffuse
				UnrealMaterial->BaseColor.Expression = InterpolateExpression;
			}
		}
	}

	bool bMadeSpecular = false;
	if (Options->IncludeSpecularMapCheck->IsChecked())
	{
		UTexture* SpecularTexture = CreateSpeedTreeMaterialTexture(Parent, ANSI_TO_TCHAR(RenderState->m_apTextures[SpeedTree::TL_SPECULAR_MASK]), false, LoadedPackages);
		if (SpecularTexture)
		{
			// make texture sampler
			UMaterialExpressionTextureSample* TextureExpression = NewObject<UMaterialExpressionTextureSample>(UnrealMaterial);
			TextureExpression->Texture = SpecularTexture;
			TextureExpression->SamplerType = SAMPLERTYPE_Color;
			
			UnrealMaterial->Expressions.Add(TextureExpression);
			UnrealMaterial->Specular.Expression = TextureExpression;
			bMadeSpecular = true;
		}
	}

	if (!bMadeSpecular)
	{
		UMaterialExpressionConstant* ZeroExpression = NewObject<UMaterialExpressionConstant>(UnrealMaterial);
		ZeroExpression->R = 0.0f;
		UnrealMaterial->Expressions.Add(ZeroExpression);
		UnrealMaterial->Specular.Expression = ZeroExpression;
	}

	if (Options->IncludeNormalMapCheck->IsChecked())
	{
		UTexture* NormalTexture = CreateSpeedTreeMaterialTexture(Parent, ANSI_TO_TCHAR(RenderState->m_apTextures[SpeedTree::TL_NORMAL]), true, LoadedPackages);
		if (NormalTexture)
		{
			// make texture sampler
			UMaterialExpressionTextureSample* TextureExpression = NewObject<UMaterialExpressionTextureSample>(UnrealMaterial);
			TextureExpression->Texture = NormalTexture;
			TextureExpression->SamplerType = SAMPLERTYPE_Normal;
			
			UnrealMaterial->Expressions.Add(TextureExpression);
			UnrealMaterial->Normal.Expression = TextureExpression;

			if (BranchSeamAmount != NULL)
			{
				// perform branch seam smoothing
				UMaterialExpressionTextureCoordinate* SeamTexcoordExpression = NewObject<UMaterialExpressionTextureCoordinate>(UnrealMaterial);
				SeamTexcoordExpression->CoordinateIndex = 6;
				UnrealMaterial->Expressions.Add(SeamTexcoordExpression);

				UMaterialExpressionTextureSample* SeamTextureExpression = NewObject<UMaterialExpressionTextureSample>(UnrealMaterial);
				SeamTextureExpression->Texture = NormalTexture;
				SeamTextureExpression->SamplerType = SAMPLERTYPE_Normal;
				SeamTextureExpression->Coordinates.Expression = SeamTexcoordExpression;
				UnrealMaterial->Expressions.Add(SeamTextureExpression);

				UMaterialExpressionLinearInterpolate* InterpolateExpression = NewObject<UMaterialExpressionLinearInterpolate>(UnrealMaterial);
				InterpolateExpression->A.Expression = SeamTextureExpression;
				InterpolateExpression->B.Expression = TextureExpression;
				InterpolateExpression->Alpha.Expression = BranchSeamAmount;
				UnrealMaterial->Expressions.Add(InterpolateExpression);

				UnrealMaterial->Normal.Expression = InterpolateExpression;
			}
		}
	}

	if (Options->IncludeVertexProcessingCheck->IsChecked() && !RenderState->m_bRigidMeshesPresent)
	{
		UMaterialExpressionSpeedTree* SpeedTreeExpression = NewObject<UMaterialExpressionSpeedTree>(UnrealMaterial);
	
		SpeedTreeExpression->LODType = (Options->IncludeSmoothLODCheck->IsChecked() ? STLOD_Smooth : STLOD_Pop);
		SpeedTreeExpression->WindType = WindType;

		float BillboardThreshold = FMath::Clamp((float)(NumBillboards - 8) / 16.0f, 0.0f, 1.0f);
		SpeedTreeExpression->BillboardThreshold = 0.9f - BillboardThreshold * 0.8f;

		if (RenderState->m_bBranchesPresent)
			SpeedTreeExpression->GeometryType = STG_Branch;
		else if (RenderState->m_bFrondsPresent)
			SpeedTreeExpression->GeometryType = STG_Frond;
		else if (RenderState->m_bHorzBillboard || RenderState->m_bVertBillboard)
			SpeedTreeExpression->GeometryType = STG_Billboard;
		else if (RenderState->m_bLeavesPresent)
			SpeedTreeExpression->GeometryType = STG_Leaf;
		else
			SpeedTreeExpression->GeometryType = STG_FacingLeaf;

		UnrealMaterial->Expressions.Add(SpeedTreeExpression);
		UnrealMaterial->WorldPositionOffset.Expression = SpeedTreeExpression;
	}

	if (Options->IncludeSpeedTreeAO->IsChecked() && 
		!(RenderState->m_bVertBillboard || RenderState->m_bHorzBillboard))
	{
		UMaterialExpressionVertexColor* VertexColor = NewObject<UMaterialExpressionVertexColor>(UnrealMaterial);
		UnrealMaterial->Expressions.Add(VertexColor);
		UnrealMaterial->AmbientOcclusion.Expression = VertexColor;
		UnrealMaterial->AmbientOcclusion.Mask = VertexColor->GetOutputs()[0].Mask;
		UnrealMaterial->AmbientOcclusion.MaskR = 1;
		UnrealMaterial->AmbientOcclusion.MaskG = 0;
		UnrealMaterial->AmbientOcclusion.MaskB = 0;
		UnrealMaterial->AmbientOcclusion.MaskA = 0;
	}

	// UE4 flips normals for two-sided materials. SpeedTrees don't need that
	if (UnrealMaterial->TwoSided)
	{
		UMaterialExpressionTwoSidedSign* TwoSidedSign = NewObject<UMaterialExpressionTwoSidedSign>(UnrealMaterial);
		UnrealMaterial->Expressions.Add(TwoSidedSign);

		auto Multiply = NewObject<UMaterialExpressionMultiply>(UnrealMaterial);
		UnrealMaterial->Expressions.Add(Multiply);
		Multiply->A.Expression = TwoSidedSign;

		if (UnrealMaterial->Normal.Expression == NULL)
		{
			auto VertexNormalExpression = NewObject<UMaterialExpressionConstant3Vector>(UnrealMaterial);
			UnrealMaterial->Expressions.Add(VertexNormalExpression);
			VertexNormalExpression->Constant = FLinearColor(0.0f, 0.0f, 1.0f);

			Multiply->B.Expression = VertexNormalExpression;
		}
		else
		{
			Multiply->B.Expression = UnrealMaterial->Normal.Expression;
		}

		UnrealMaterial->Normal.Expression = Multiply;
	}

	if (Options->IncludeColorAdjustment->IsChecked() && UnrealMaterial->BaseColor.Expression != NULL && 
		(RenderState->m_bLeavesPresent || RenderState->m_bFacingLeavesPresent || RenderState->m_bVertBillboard || RenderState->m_bHorzBillboard))
	{
		UMaterialFunction* ColorVariationFunction = LoadObject<UMaterialFunction>(NULL, TEXT("/Engine/Functions/Engine_MaterialFunctions01/SpeedTree/SpeedTreeColorVariation.SpeedTreeColorVariation"), NULL, LOAD_None, NULL);
		if (ColorVariationFunction)
		{
			UMaterialExpressionMaterialFunctionCall* ColorVariation = NewObject<UMaterialExpressionMaterialFunctionCall>(UnrealMaterial);
			UnrealMaterial->Expressions.Add(ColorVariation);

			ColorVariation->MaterialFunction = ColorVariationFunction;
			ColorVariation->UpdateFromFunctionResource( );

			ColorVariation->GetInput(0)->Expression = UnrealMaterial->BaseColor.Expression;
			ColorVariation->GetInput(0)->Mask = UnrealMaterial->BaseColor.Expression->GetOutputs()[0].Mask;
			ColorVariation->GetInput(0)->MaskR = 1;
			ColorVariation->GetInput(0)->MaskG = 1;
			ColorVariation->GetInput(0)->MaskB = 1;
			ColorVariation->GetInput(0)->MaskA = 0;

			UnrealMaterial->BaseColor.Expression = ColorVariation;
		}
	}
	
	LayoutMaterial(UnrealMaterial);

	// make sure that any static meshes, etc using this material will stop using the FMaterialResource of the original 
	// material, and will use the new FMaterialResource created when we make a new UMaterial in place
	FGlobalComponentReregisterContext RecreateComponents;
	
	// let the material update itself if necessary
	UnrealMaterial->PreEditChange(NULL);
	UnrealMaterial->PostEditChange();
	
	return UnrealMaterial;
}

static void CopySpeedTreeWind(const SpeedTree::CWind* Wind, TSharedPtr<FSpeedTreeWind> SpeedTreeWind)
{
	SpeedTree::CWind::SParams OrigParams = Wind->GetParams();
	FSpeedTreeWind::SParams NewParams;

	#define COPY_PARAM(name) NewParams.name = OrigParams.name;
	#define COPY_CURVE(name) for (int32 CurveIndex = 0; CurveIndex < FSpeedTreeWind::NUM_WIND_POINTS_IN_CURVE; ++CurveIndex) { NewParams.name[CurveIndex] = OrigParams.name[CurveIndex]; }

	COPY_PARAM(m_fStrengthResponse);
	COPY_PARAM(m_fDirectionResponse);
	
	COPY_PARAM(m_fAnchorOffset);
	COPY_PARAM(m_fAnchorDistanceScale);
	
	for (int32 OscIndex = 0; OscIndex < FSpeedTreeWind::NUM_OSC_COMPONENTS; ++OscIndex)
	{
		COPY_CURVE(m_afFrequencies[OscIndex]);
	}
	
	COPY_PARAM(m_fGlobalHeight);
	COPY_PARAM(m_fGlobalHeightExponent);
	COPY_CURVE(m_afGlobalDistance);
	COPY_CURVE(m_afGlobalDirectionAdherence);
	
	for (int32 BranchIndex = 0; BranchIndex < FSpeedTreeWind::NUM_BRANCH_LEVELS; ++BranchIndex)
	{
		COPY_CURVE(m_asBranch[BranchIndex].m_afDistance);
		COPY_CURVE(m_asBranch[BranchIndex].m_afDirectionAdherence);
		COPY_CURVE(m_asBranch[BranchIndex].m_afWhip);
		COPY_PARAM(m_asBranch[BranchIndex].m_fTurbulence);
		COPY_PARAM(m_asBranch[BranchIndex].m_fTwitch);
		COPY_PARAM(m_asBranch[BranchIndex].m_fTwitchFreqScale);
	}

	for (int32 LeafIndex = 0; LeafIndex < FSpeedTreeWind::NUM_LEAF_GROUPS; ++LeafIndex)
	{
		COPY_CURVE(m_asLeaf[LeafIndex].m_afRippleDistance);
		COPY_CURVE(m_asLeaf[LeafIndex].m_afTumbleFlip);
		COPY_CURVE(m_asLeaf[LeafIndex].m_afTumbleTwist);
		COPY_CURVE(m_asLeaf[LeafIndex].m_afTumbleDirectionAdherence);
		COPY_CURVE(m_asLeaf[LeafIndex].m_afTwitchThrow);
		COPY_PARAM(m_asLeaf[LeafIndex].m_fTwitchSharpness);
		COPY_PARAM(m_asLeaf[LeafIndex].m_fRollMaxScale);
		COPY_PARAM(m_asLeaf[LeafIndex].m_fRollMinScale);
		COPY_PARAM(m_asLeaf[LeafIndex].m_fRollSpeed);
		COPY_PARAM(m_asLeaf[LeafIndex].m_fRollSeparation);
		COPY_PARAM(m_asLeaf[LeafIndex].m_fLeewardScalar);
	}
	
	COPY_CURVE(m_afFrondRippleDistance);
	COPY_PARAM(m_fFrondRippleTile);
	COPY_PARAM(m_fFrondRippleLightingScalar);
	
	COPY_PARAM(m_fGustFrequency);
	COPY_PARAM(m_fGustStrengthMin);
	COPY_PARAM(m_fGustStrengthMax);
	COPY_PARAM(m_fGustDurationMin);
	COPY_PARAM(m_fGustDurationMax);
	COPY_PARAM(m_fGustRiseScalar);
	COPY_PARAM(m_fGustFallScalar);

	SpeedTreeWind->SetParams(NewParams);

	for (int32 OptionIndex = 0; OptionIndex < FSpeedTreeWind::NUM_WIND_OPTIONS; ++OptionIndex)
	{
		SpeedTreeWind->SetOption((FSpeedTreeWind::EOptions)OptionIndex, Wind->IsOptionEnabled((SpeedTree::CWind::EOptions)OptionIndex));
	}

	const SpeedTree::st_float32* BranchAnchor = Wind->GetBranchAnchor();
	SpeedTreeWind->SetTreeValues(FVector(BranchAnchor[0], BranchAnchor[1], BranchAnchor[2]), Wind->GetMaxBranchLength());

	SpeedTreeWind->SetNeedsReload(true);
}


static void MakeBodyFromCollisionObjects(UStaticMesh* StaticMesh, const SpeedTree::SCollisionObject* CollisionObjects, int32 NumCollisionObjects)
{
	StaticMesh->CreateBodySetup();
	FKAggregateGeom& AggGeo = StaticMesh->BodySetup->AggGeom;

	for (int32 CollisionObjectIndex = 0; CollisionObjectIndex < NumCollisionObjects; ++CollisionObjectIndex)
	{
		const SpeedTree::SCollisionObject& CollisionObject = CollisionObjects[CollisionObjectIndex];
		const FVector Pos1(-CollisionObject.m_vCenter1.x, CollisionObject.m_vCenter1.y, CollisionObject.m_vCenter1.z);
		const FVector Pos2(-CollisionObject.m_vCenter2.x, CollisionObject.m_vCenter2.y, CollisionObject.m_vCenter2.z);

		if (Pos1 == Pos2)
		{
			// sphere object
			FKSphereElem SphereElem;
			FMemory::Memzero(SphereElem);
			SphereElem.Radius = CollisionObject.m_fRadius;
			SphereElem.Center = Pos1;
			AggGeo.SphereElems.Add(SphereElem);
		}
		else
		{
			// capsule/sphyll object
			FKSphylElem SphylElem;
			FMemory::Memzero(SphylElem);
			SphylElem.Radius = CollisionObject.m_fRadius;
			FVector UpDir = Pos2 - Pos1;
			SphylElem.Length = UpDir.Size();
			if (SphylElem.Length != 0.0f)
				UpDir /= SphylElem.Length;			
			SphylElem.SetTransform( FTransform( FQuat::FindBetween( FVector( 0.0f, 0.0f, 1.0f ), UpDir ), ( Pos1 + Pos2 ) * 0.5f ) );
			AggGeo.SphylElems.Add(SphylElem);
		}
	}

	StaticMesh->BodySetup->ClearPhysicsMeshes();
	StaticMesh->BodySetup->InvalidatePhysicsData();
	RefreshCollisionChange(StaticMesh);
}



void ProcessTriangleCorner( SpeedTree::CCore& SpeedTree, const int32 TriangleIndex, const int32 Corner, const SpeedTree::SDrawCall* DrawCall, const SpeedTree::st_uint32* Indices32, const SpeedTree::st_uint16* Indices16, FRawMesh& RawMesh, const int32 IndexOffset, const int32 NumUVs, const SpeedTree::SRenderState* RenderState )
{
	SpeedTree::st_float32 Data[ 4 ];

	// flip the triangle winding
	int32 Index = TriangleIndex * 3 + Corner;

	int32 VertexIndex = DrawCall->m_b32BitIndices ? Indices32[ Index ] : Indices16[ Index ];
	RawMesh.WedgeIndices.Add( VertexIndex + IndexOffset );

	// tangents
	DrawCall->GetProperty( SpeedTree::VERTEX_PROPERTY_NORMAL, VertexIndex, Data );
	FVector Normal( -Data[ 0 ], Data[ 1 ], Data[ 2 ] );
	DrawCall->GetProperty( SpeedTree::VERTEX_PROPERTY_TANGENT, VertexIndex, Data );
	FVector Tangent( -Data[ 0 ], Data[ 1 ], Data[ 2 ] );
	RawMesh.WedgeTangentX.Add( Tangent );
	RawMesh.WedgeTangentY.Add( Normal ^ Tangent );
	RawMesh.WedgeTangentZ.Add( Normal );

	// ao
	DrawCall->GetProperty( SpeedTree::VERTEX_PROPERTY_AMBIENT_OCCLUSION, VertexIndex, Data );
	uint8 AO = Data[ 0 ] * 255.0f;
	RawMesh.WedgeColors.Add( FColor( AO, AO, AO, 255 ) );

	// keep texcoords padded to align indices
	int32 BaseTexcoordIndex = RawMesh.WedgeTexCoords[ 0 ].Num();
	for( int32 PadIndex = 0; PadIndex < NumUVs; ++PadIndex )
	{
		RawMesh.WedgeTexCoords[ PadIndex ].AddUninitialized( 1 );
	}

	// All texcoords are packed into 4 float4 vertex attributes
	// Data is as follows								

	//		Branches			Fronds				Leaves				Billboards
	//
	// 0	Diffuse				Diffuse				Diffuse				Diffuse			
	// 1	Lightmap UV			Lightmap UV			Lightmap UV			Lightmap UV	(same as diffuse)		
	// 2	Branch Wind XY		Branch Wind XY		Branch Wind XY			
	// 3	LOD XY				LOD XY				LOD XY				
	// 4	LOD Z, Seam Amount	LOD Z, 0			LOD Z, Anchor X		
	// 5	Detail UV			Frond Wind XY		Anchor YZ	
	// 6	Seam UV				Frond Wind Z, 0		Leaf Wind XY
	// 7	0					0					Leaf Wind Z, Leaf Group


	// diffuse
	DrawCall->GetProperty( SpeedTree::VERTEX_PROPERTY_DIFFUSE_TEXCOORDS, VertexIndex, Data );
	RawMesh.WedgeTexCoords[ 0 ].Top() = FVector2D( Data[ 0 ], Data[ 1 ] );

	// lightmap
	DrawCall->GetProperty( SpeedTree::VERTEX_PROPERTY_LIGHTMAP_TEXCOORDS, VertexIndex, Data );
	RawMesh.WedgeTexCoords[ 1 ].Top() = FVector2D( Data[ 0 ], Data[ 1 ] );

	// branch wind
	DrawCall->GetProperty( SpeedTree::VERTEX_PROPERTY_WIND_BRANCH_DATA, VertexIndex, Data );
	RawMesh.WedgeTexCoords[ 2 ].Top() = FVector2D( Data[ 0 ], Data[ 1 ] );

	// lod
	if( RenderState->m_bFacingLeavesPresent )
	{
		DrawCall->GetProperty( SpeedTree::VERTEX_PROPERTY_LEAF_CARD_LOD_SCALAR, VertexIndex, Data );
		RawMesh.WedgeTexCoords[ 3 ].Top() = FVector2D( Data[ 0 ], 0.0f );
		RawMesh.WedgeTexCoords[ 4 ].Top() = FVector2D( 0.0f, 0.0f );
	}
	else
	{
		DrawCall->GetProperty( SpeedTree::VERTEX_PROPERTY_LOD_POSITION, VertexIndex, Data );
		RawMesh.WedgeTexCoords[ 3 ].Top() = FVector2D( -Data[ 0 ], Data[ 1 ] );
		RawMesh.WedgeTexCoords[ 4 ].Top() = FVector2D( Data[ 2 ], 0.0f );
	}

	// other
	if( RenderState->m_bBranchesPresent )
	{
		// detail
		DrawCall->GetProperty( SpeedTree::VERTEX_PROPERTY_DETAIL_TEXCOORDS, VertexIndex, Data );
		RawMesh.WedgeTexCoords[ 5 ].Top() = FVector2D( Data[ 0 ], Data[ 1 ] );

		// branch seam
		DrawCall->GetProperty( SpeedTree::VERTEX_PROPERTY_BRANCH_SEAM_DIFFUSE, VertexIndex, Data );
		RawMesh.WedgeTexCoords[ 6 ].Top() = FVector2D( Data[ 0 ], Data[ 1 ] );
		RawMesh.WedgeTexCoords[ 4 ].Top().Y = Data[ 2 ];
	}
	else if( RenderState->m_bFrondsPresent )
	{
		// frond wind
		DrawCall->GetProperty( SpeedTree::VERTEX_PROPERTY_WIND_EXTRA_DATA, VertexIndex, Data );
		RawMesh.WedgeTexCoords[ 5 ].Top() = FVector2D( Data[ 0 ], Data[ 1 ] );
		RawMesh.WedgeTexCoords[ 6 ].Top() = FVector2D( Data[ 2 ], 0.0f );
	}
	else if( RenderState->m_bLeavesPresent || RenderState->m_bFacingLeavesPresent )
	{
		// anchor
		if( RenderState->m_bFacingLeavesPresent )
		{
			DrawCall->GetProperty( SpeedTree::VERTEX_PROPERTY_POSITION, VertexIndex, Data );
		}
		else
		{
			DrawCall->GetProperty( SpeedTree::VERTEX_PROPERTY_LEAF_ANCHOR_POINT, VertexIndex, Data );
		}
		RawMesh.WedgeTexCoords[ 4 ].Top().Y = -Data[ 0 ];
		RawMesh.WedgeTexCoords[ 5 ].Top() = FVector2D( Data[ 1 ], Data[ 2 ] );

		// leaf wind
		DrawCall->GetProperty( SpeedTree::VERTEX_PROPERTY_WIND_EXTRA_DATA, VertexIndex, Data );
		RawMesh.WedgeTexCoords[ 6 ].Top() = FVector2D( Data[ 0 ], Data[ 1 ] );
		RawMesh.WedgeTexCoords[ 7 ].Top().X = Data[ 2 ];
		DrawCall->GetProperty( SpeedTree::VERTEX_PROPERTY_WIND_FLAGS, VertexIndex, Data );
		RawMesh.WedgeTexCoords[ 7 ].Top().Y = Data[ 0 ];
	}
}


UObject* USpeedTreeImportFactory::FactoryCreateBinary( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn )
{
	FEditorDelegates::OnAssetPreImport.Broadcast(this, InClass, InParent, InName, Type);

	TSharedPtr<SWindow> ParentWindow;
	// Check if the main frame is loaded.  When using the old main frame it may not be.
	if( FModuleManager::Get().IsModuleLoaded( "MainFrame" ) )
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" );
		ParentWindow = MainFrame.GetParentWindow();
	}

	TSharedPtr<SSpeedTreeImportOptions> Options;

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "SpeedTree Options" ))
			.SizingRule( ESizingRule::Autosized );

	Window->SetContent(SAssignNew(Options, SSpeedTreeImportOptions).WidgetWindow(Window));

	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

	UStaticMesh* StaticMesh = NULL;

	if (Options->ShouldImport())
	{
#ifdef SPEEDTREE_KEY
		SpeedTree::CCore::Authorize(PREPROCESSOR_TO_STRING(SPEEDTREE_KEY));
#endif
		
		SpeedTree::CCore SpeedTree;
		if (!SpeedTree.LoadTree(Buffer, BufferEnd - Buffer, false, false, Options->TreeScale))
		{
			UE_LOG(LogSpeedTreeImport, Error, TEXT("%s"), ANSI_TO_TCHAR(SpeedTree.GetError( )));
		}
		else
		{
			const SpeedTree::SGeometry* SpeedTreeGeometry = SpeedTree.GetGeometry();
			if ((Options->ImportGeometryType == SSpeedTreeImportOptions::IGT_Billboards && SpeedTreeGeometry->m_sVertBBs.m_nNumBillboards == 0) ||
				(Options->ImportGeometryType == SSpeedTreeImportOptions::IGT_3D && SpeedTreeGeometry->m_nNumLods == 0))
			{
				UE_LOG(LogSpeedTreeImport, Error, TEXT("Tree contains no useable geometry"));
			}
			else
			{
				LoadedPackages.Empty( );

				// make static mesh object
				FString MeshName = ObjectTools::SanitizeObjectName(InName.ToString());
				FString NewPackageName = FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetName()) + TEXT("/") + MeshName;
				NewPackageName = PackageTools::SanitizePackageName(NewPackageName);
				UPackage* Package = CreatePackage(NULL, *NewPackageName);
				
				// clear out old mesh
				TArray<UMaterialInterface*> OldMaterials;
				UStaticMesh* ExistingMesh = FindObject<UStaticMesh>(Package, *MeshName);
				FGlobalComponentReregisterContext RecreateComponents;

				if (ExistingMesh)
				{
					OldMaterials = ExistingMesh->Materials;
					for (int32 i = 0; i < OldMaterials.Num(); ++i)
					{
						OldMaterials[i]->PreEditChange(NULL);
						OldMaterials[i]->PostEditChange();
					}

					// Free any RHI resources for existing mesh before we re-create in place.
					ExistingMesh->PreEditChange(NULL);
				}
				
				StaticMesh = NewObject<UStaticMesh>(Package, FName(*MeshName), Flags | RF_Public);

				// @todo AssetImportData make a data class for speed tree assets
				StaticMesh->AssetImportData->Update(UFactory::GetCurrentFilename());
				
				// clear out any old data
				StaticMesh->SourceModels.Empty();
				StaticMesh->SectionInfoMap.Clear();
				StaticMesh->Materials.Empty();

				// Lightmap data
				StaticMesh->LightingGuid = FGuid::NewGuid();
				StaticMesh->LightMapResolution = 128;
				StaticMesh->LightMapCoordinateIndex = 1;

				// set up SpeedTree wind data
				if (!StaticMesh->SpeedTreeWind.IsValid())
					StaticMesh->SpeedTreeWind = TSharedPtr<FSpeedTreeWind>(new FSpeedTreeWind);
				const SpeedTree::CWind* Wind = &(SpeedTree.GetWind());
				CopySpeedTreeWind(Wind, StaticMesh->SpeedTreeWind);

				// choose wind type based on options enabled
				ESpeedTreeWindType WindType = STW_None;
				if (Options->IncludeWindCheck->IsChecked() && Wind->IsOptionEnabled(SpeedTree::CWind::GLOBAL_WIND))
				{
					WindType = STW_Fastest;

					if (Wind->IsOptionEnabled(SpeedTree::CWind::BRANCH_DIRECTIONAL_FROND_1))
					{
						WindType = STW_Palm;
					}
					else if (Wind->IsOptionEnabled(SpeedTree::CWind::LEAF_TUMBLE_1))
					{
						WindType = STW_Best;
					}
					else if (Wind->IsOptionEnabled(SpeedTree::CWind::BRANCH_SIMPLE_1))
					{
						WindType = STW_Better;
					}
					else if (Wind->IsOptionEnabled(SpeedTree::CWind::LEAF_RIPPLE_VERTEX_NORMAL_1))
					{
						WindType = STW_Fast;
					}
				}

				// Force LOD code out of the shaders if we only have one LOD
				if (Options->IncludeSmoothLODCheck->IsChecked( ))
				{
					int32 TotalLODs = 0;
					if (Options->ImportGeometryType != SSpeedTreeImportOptions::IGT_Billboards)
					{
						TotalLODs += SpeedTreeGeometry->m_nNumLods;
					}
					if (Options->ImportGeometryType != SSpeedTreeImportOptions::IGT_3D && SpeedTreeGeometry->m_sVertBBs.m_nNumBillboards > 0)
					{
						++TotalLODs;
					}
					if (TotalLODs < 2)
					{
						Options->IncludeSmoothLODCheck->ToggleCheckedState( );
					}
				}

				// make geometry LODs
				if (Options->ImportGeometryType != SSpeedTreeImportOptions::IGT_Billboards)
				{
					int32 BranchMaterialsMade = 0;
					int32 FrondMaterialsMade = 0;
					int32 LeafMaterialsMade = 0;
					int32 FacingLeafMaterialsMade = 0;
					int32 MeshMaterialsMade = 0;
					TMap<int32, int32> RenderStateIndexToStaticMeshIndex;
				
					for (int32 LODIndex = 0; LODIndex < SpeedTreeGeometry->m_nNumLods; ++LODIndex)
					{
						const SpeedTree::SLod* TreeLOD = &SpeedTreeGeometry->m_pLods[LODIndex];
						FRawMesh RawMesh;

						// compute the number of texcoords we need so we can pad when necessary
						int32 NumUVs = 7; // static meshes have fewer, but they are so rare, we shouldn't complicate things for them
						for (int32 DrawCallIndex = 0; DrawCallIndex < TreeLOD->m_nNumDrawCalls; ++DrawCallIndex)
						{
							const SpeedTree::SDrawCall* DrawCall = &TreeLOD->m_pDrawCalls[DrawCallIndex];
							const SpeedTree::SRenderState* RenderState = DrawCall->m_pRenderState;
							if (RenderState->m_bLeavesPresent || RenderState->m_bFacingLeavesPresent)
							{
								NumUVs = FMath::Max(8, NumUVs);
							}
						}

						for (int32 DrawCallIndex = 0; DrawCallIndex < TreeLOD->m_nNumDrawCalls; ++DrawCallIndex)
						{
							SpeedTree::st_float32 Data[4];
							const SpeedTree::SDrawCall* DrawCall = &TreeLOD->m_pDrawCalls[DrawCallIndex];
							const SpeedTree::SRenderState* RenderState = DrawCall->m_pRenderState;

							// make material for this render state, if needed
							int32 MaterialIndex = -1;
							int32* OldMaterial = RenderStateIndexToStaticMeshIndex.Find(DrawCall->m_nRenderStateIndex);
							if (OldMaterial == NULL)
							{
								FString MaterialName = MeshName;

								if (RenderState->m_bBranchesPresent)
								{
									MaterialName += "_Branches";
									if (BranchMaterialsMade > 0)
										MaterialName += FString::Printf(TEXT("_%d"), BranchMaterialsMade + 1);
									++BranchMaterialsMade;
								}
								else if (RenderState->m_bFrondsPresent)
								{
									MaterialName += "_Fronds";
									if (FrondMaterialsMade > 0)
										MaterialName += FString::Printf(TEXT("_%d"), FrondMaterialsMade + 1);
									++FrondMaterialsMade;
								}
								else if (RenderState->m_bFacingLeavesPresent)
								{
									MaterialName += "_FacingLeaves";
									if (FacingLeafMaterialsMade > 0)
										MaterialName += FString::Printf(TEXT("_%d"), FacingLeafMaterialsMade + 1);
									++FacingLeafMaterialsMade;
								}
								else if (RenderState->m_bLeavesPresent)
								{
									MaterialName += "_Leaves";
									if (LeafMaterialsMade > 0)
										MaterialName += FString::Printf(TEXT("_%d"), LeafMaterialsMade + 1);
									++LeafMaterialsMade;
								}
								else if (RenderState->m_bRigidMeshesPresent)
								{
									MaterialName += "_Meshes";
									if (MeshMaterialsMade > 0)
										MaterialName += FString::Printf(TEXT("_%d"), MeshMaterialsMade + 1);
									++MeshMaterialsMade;
								}
								else if (RenderState->m_bHorzBillboard || RenderState->m_bVertBillboard)
								{
									MaterialName += "_Billboards";
								}

								MaterialName = ObjectTools::SanitizeObjectName(MaterialName);

								UMaterialInterface* Material = CreateSpeedTreeMaterial(InParent, MaterialName, RenderState, Options, WindType, SpeedTreeGeometry->m_sVertBBs.m_nNumBillboards, LoadedPackages);
								
								RenderStateIndexToStaticMeshIndex.Add(DrawCall->m_nRenderStateIndex, StaticMesh->Materials.Num());
								MaterialIndex = StaticMesh->Materials.Num();
								StaticMesh->Materials.Add(Material);
							}
							else
							{
								MaterialIndex = *OldMaterial;
							}

							int32 IndexOffset = RawMesh.VertexPositions.Num();

							for (int32 VertexIndex = 0; VertexIndex < DrawCall->m_nNumVertices; ++VertexIndex)
							{
								// position
								DrawCall->GetProperty(SpeedTree::VERTEX_PROPERTY_POSITION, VertexIndex, Data);

								if (RenderState->m_bFacingLeavesPresent)
								{
									SpeedTree::st_float32 Data2[4];
									DrawCall->GetProperty(SpeedTree::VERTEX_PROPERTY_LEAF_CARD_CORNER, VertexIndex, Data2);
									Data[0] -= Data2[0];
									Data[1] += Data2[1];
									Data[2] += Data2[2];                                    
								}

								RawMesh.VertexPositions.Add(FVector(-Data[0], Data[1], Data[2]));
							}

							const SpeedTree::st_byte* pIndexData = &*DrawCall->m_pIndexData;
							const SpeedTree::st_uint32* Indices32 = (SpeedTree::st_uint32*)pIndexData;
							const SpeedTree::st_uint16* Indices16 = (SpeedTree::st_uint16*)pIndexData;

							int32 TriangleCount = DrawCall->m_nNumIndices / 3;
							int32 ExistingTris = RawMesh.WedgeIndices.Num() / 3;

							for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
							{
								RawMesh.FaceMaterialIndices.Add(MaterialIndex);
								RawMesh.FaceSmoothingMasks.Add(0);

#if 0
								float LeafLODScalar = 1.0f;
								if (RenderState->m_bLeavesPresent)
								{
									// reconstruct a single leaf scalar so we don't need the lod position
									// This is not 100% like the Modeler for frond-based leaves, but it reduces the amount of VB data needed
									int32 Index = TriangleIndex * 3;
									int32 VertexIndex1 = DrawCall->m_b32BitIndices ? Indices32[Index + 0] : Indices16[Index + 0];
									int32 VertexIndex2 = DrawCall->m_b32BitIndices ? Indices32[Index + 1] : Indices16[Index + 1];
									int32 VertexIndex3 = DrawCall->m_b32BitIndices ? Indices32[Index + 2] : Indices16[Index + 2];

									SpeedTree::st_float32 Data[4];
									DrawCall->GetProperty(SpeedTree::VERTEX_PROPERTY_POSITION, VertexIndex1, Data);
									FVector Corner1(Data[0], Data[1], Data[2]);
									DrawCall->GetProperty(SpeedTree::VERTEX_PROPERTY_POSITION, VertexIndex2, Data);
									FVector Corner2(Data[0], Data[1], Data[2]);
									DrawCall->GetProperty(SpeedTree::VERTEX_PROPERTY_POSITION, VertexIndex3, Data);
									FVector Corner3(Data[0], Data[1], Data[2]);

									DrawCall->GetProperty(SpeedTree::VERTEX_PROPERTY_LOD_POSITION, VertexIndex1, Data);
									FVector LODCorner1(Data[0], Data[1], Data[2]);
									DrawCall->GetProperty(SpeedTree::VERTEX_PROPERTY_LOD_POSITION, VertexIndex2, Data);
									FVector LODCorner2(Data[0], Data[1], Data[2]);
									DrawCall->GetProperty(SpeedTree::VERTEX_PROPERTY_LOD_POSITION, VertexIndex3, Data);
									FVector LODCorner3(Data[0], Data[1], Data[2]);

									float OrigArea = ((Corner2 - Corner1) ^ (Corner3 - Corner1)).Size();
									float LODArea = ((LODCorner2 - LODCorner1) ^ (LODCorner3 - LODCorner1)).Size();

									LeafLODScalar = FMath::Sqrt(LODArea / OrigArea);
								}
#endif

								for (int32 Corner = 0; Corner < 3; ++Corner)
								{
									ProcessTriangleCorner( SpeedTree, TriangleIndex, Corner, DrawCall, Indices32, Indices16, RawMesh, IndexOffset, NumUVs, RenderState );
								}
							}							
						}

						FStaticMeshSourceModel* LODModel = new (StaticMesh->SourceModels) FStaticMeshSourceModel();
						LODModel->BuildSettings.bRecomputeNormals = false;
						LODModel->BuildSettings.bRecomputeTangents = false;
						LODModel->BuildSettings.bRemoveDegenerates = true;
						LODModel->BuildSettings.bUseFullPrecisionUVs = false;	
						LODModel->BuildSettings.bGenerateLightmapUVs = false;
						LODModel->ScreenSize = 0.1f / FMath::Pow(2.0f, StaticMesh->SourceModels.Num() - 1);
						LODModel->RawMeshBulkData->SaveRawMesh(RawMesh);

						for (int32 MaterialIndex = 0; MaterialIndex < StaticMesh->Materials.Num(); ++MaterialIndex)
						{
							FMeshSectionInfo Info = StaticMesh->SectionInfoMap.Get(LODIndex, MaterialIndex);
							Info.MaterialIndex = MaterialIndex;
							StaticMesh->SectionInfoMap.Set(LODIndex, MaterialIndex, Info);
						}
					}
				}

				// make billboard LOD
				if (Options->ImportGeometryType != SSpeedTreeImportOptions::IGT_3D && SpeedTreeGeometry->m_sVertBBs.m_nNumBillboards > 0)
				{
					UMaterialInterface* Material = CreateSpeedTreeMaterial(InParent, MeshName + "_Billboard", &SpeedTreeGeometry->m_aBillboardRenderStates[SpeedTree::RENDER_PASS_MAIN], Options, WindType, SpeedTreeGeometry->m_sVertBBs.m_nNumBillboards, LoadedPackages);
					int32 MaterialIndex = StaticMesh->Materials.Num();
					StaticMesh->Materials.Add(Material);

					FRawMesh RawMesh;
					
					// fill out triangles
					float BillboardWidth = SpeedTreeGeometry->m_sVertBBs.m_fWidth;
					float BillboardBottom = SpeedTreeGeometry->m_sVertBBs.m_fBottomPos;
					float BillboardTop = SpeedTreeGeometry->m_sVertBBs.m_fTopPos;
					float BillboardHeight = BillboardTop - BillboardBottom;

					// data for a regular billboard quad
					const SpeedTree::st_uint16 BillboardQuadIndices[] = { 0, 1, 2, 0, 2, 3 };
					const SpeedTree::st_float32 BillboardQuadVertices[] = 
					{
						1.0f, 1.0f,
						0.0f, 1.0f,
						0.0f, 0.0f, 
						1.0f, 0.0f
					};

					// choose between quad or compiler-generated cutout
					int32 NumVertices = SpeedTreeGeometry->m_sVertBBs.m_nNumCutoutVertices;
					const SpeedTree::st_float32* Vertices = SpeedTreeGeometry->m_sVertBBs.m_pCutoutVertices;
					int32 NumIndices = SpeedTreeGeometry->m_sVertBBs.m_nNumCutoutIndices;
					const SpeedTree::st_uint16* Indices = SpeedTreeGeometry->m_sVertBBs.m_pCutoutIndices;
					if (NumIndices == 0)
					{
						NumVertices = 4;
						Vertices = BillboardQuadVertices;
						NumIndices = 6;
						Indices = BillboardQuadIndices;
					}

					// make the billboards
					for (int32 BillboardIndex = 0; BillboardIndex < SpeedTreeGeometry->m_sVertBBs.m_nNumBillboards; ++BillboardIndex)
					{
						FRotator Facing(0, 90.0f - 360.0f * (float)BillboardIndex / (float)SpeedTreeGeometry->m_sVertBBs.m_nNumBillboards, 0);
						FRotationMatrix BillboardRotate(Facing);

						FVector TangentX = BillboardRotate.TransformVector(FVector(1.0f, 0.0f, 0.0f));
						FVector TangentY = BillboardRotate.TransformVector(FVector(0.0f, 0.0f, -1.0f));
						FVector TangentZ = BillboardRotate.TransformVector(FVector(0.0f, 1.0f, 0.0f));
	
						const float* TexCoords = &SpeedTreeGeometry->m_sVertBBs.m_pTexCoords[BillboardIndex * 4];
						bool bRotated = (SpeedTreeGeometry->m_sVertBBs.m_pRotated[BillboardIndex] == 1);

						int32 IndexOffset = RawMesh.VertexPositions.Num();
					
						// position
						for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
						{
							const SpeedTree::st_float32* Vertex = &Vertices[VertexIndex * 2];
							FVector Position = BillboardRotate.TransformVector(FVector(Vertex[0] * BillboardWidth - BillboardWidth * 0.5f, 0.0f, Vertex[1] * BillboardHeight + BillboardBottom));
							RawMesh.VertexPositions.Add(Position);
						}

						// other data
						int32 NumTriangles = NumIndices / 3;
						for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; ++TriangleIndex)
						{
							RawMesh.FaceMaterialIndices.Add(MaterialIndex);
							RawMesh.FaceSmoothingMasks.Add(0);

							for (int32 Corner = 0; Corner < 3; ++Corner)
							{
								int32 Index = Indices[TriangleIndex * 3 + Corner];
								const SpeedTree::st_float32* Vertex = &Vertices[Index * 2];

								RawMesh.WedgeIndices.Add(Index + IndexOffset);

								RawMesh.WedgeTangentX.Add(TangentX);
								RawMesh.WedgeTangentY.Add(TangentY);
								RawMesh.WedgeTangentZ.Add(TangentZ);

								if (bRotated)
								{
									RawMesh.WedgeTexCoords[0].Add(FVector2D(TexCoords[0] + Vertex[1] * TexCoords[2], TexCoords[1] + Vertex[0] * TexCoords[3]));
								}
								else
								{
									RawMesh.WedgeTexCoords[0].Add(FVector2D(TexCoords[0] + Vertex[0] * TexCoords[2], TexCoords[1] + Vertex[1] * TexCoords[3]));
								}

								// lightmap coord
								RawMesh.WedgeTexCoords[1].Add(RawMesh.WedgeTexCoords[0].Top());
							}
						}
					}

					FStaticMeshSourceModel* LODModel = new (StaticMesh->SourceModels) FStaticMeshSourceModel();
					LODModel->BuildSettings.bRecomputeNormals = false;
					LODModel->BuildSettings.bRecomputeTangents = false;
					LODModel->BuildSettings.bRemoveDegenerates = true;
					LODModel->BuildSettings.bUseFullPrecisionUVs = false;
					LODModel->BuildSettings.bGenerateLightmapUVs = false;
					LODModel->ScreenSize = 0.1f / FMath::Pow(2.0f, StaticMesh->SourceModels.Num() - 1);
					LODModel->RawMeshBulkData->SaveRawMesh(RawMesh);
				}

				if (OldMaterials.Num() == StaticMesh->Materials.Num())
				{
					StaticMesh->Materials = OldMaterials;
				}

				StaticMesh->Build();

				if (Options->IncludeCollision->IsChecked())
				{
					int32 NumCollisionObjects = 0;
					const SpeedTree::SCollisionObject* CollisionObjects = SpeedTree.GetCollisionObjects(NumCollisionObjects);
					if (CollisionObjects != NULL && NumCollisionObjects > 0)
					{
						MakeBodyFromCollisionObjects(StaticMesh, CollisionObjects, NumCollisionObjects);
					}
				}

				// make better LOD info for SpeedTrees
				if (Options->LODType == SSpeedTreeImportOptions::ILT_IndividualActors)
				{
					StaticMesh->bAutoComputeLODScreenSize = false;
				}
				StaticMesh->bRequiresLODDistanceConversion = false;
			}
		}
	}

	FEditorDelegates::OnAssetPostImport.Broadcast(this, StaticMesh);

	return StaticMesh;
}

#endif // WITH_SPEEDTREE

#undef LOCTEXT_NAMESPACE
