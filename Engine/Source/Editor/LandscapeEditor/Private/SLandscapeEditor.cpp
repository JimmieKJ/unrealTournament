// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorPrivatePCH.h"
#include "LandscapeEdMode.h"
#include "SLandscapeEditor.h"
#include "LandscapeEditorCommands.h"
#include "AssetThumbnail.h"
#include "LandscapeEditorObject.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "Editor/IntroTutorials/Public/IIntroTutorials.h"

#define LOCTEXT_NAMESPACE "LandscapeEditor"

void SLandscapeAssetThumbnail::Construct(const FArguments& InArgs, UObject* Asset, TSharedRef<FAssetThumbnailPool> ThumbnailPool)
{
	FIntPoint ThumbnailSize = InArgs._ThumbnailSize;

	AssetThumbnail = MakeShareable(new FAssetThumbnail(Asset, ThumbnailSize.X, ThumbnailSize.Y, ThumbnailPool));

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(ThumbnailSize.X)
		.HeightOverride(ThumbnailSize.Y)
		[
			AssetThumbnail->MakeThumbnailWidget()
		]
	];

	UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(Asset);
	if (MaterialInterface)
	{
		UMaterial::OnMaterialCompilationFinished().AddSP(this, &SLandscapeAssetThumbnail::OnMaterialCompilationFinished);
	}
}

SLandscapeAssetThumbnail::~SLandscapeAssetThumbnail()
{
	UMaterial::OnMaterialCompilationFinished().RemoveAll(this);
}

void SLandscapeAssetThumbnail::OnMaterialCompilationFinished(UMaterialInterface* MaterialInterface)
{
	UMaterialInterface* MaterialAsset = Cast<UMaterialInterface>(AssetThumbnail->GetAsset());
	if (MaterialAsset)
	{
		if (MaterialAsset->IsDependent(MaterialInterface))
		{
			// Refresh thumbnail
			AssetThumbnail->SetAsset(AssetThumbnail->GetAsset());
		}
	}
}

void SLandscapeAssetThumbnail::SetAsset(UObject* Asset)
{
	AssetThumbnail->SetAsset(Asset);
}

//////////////////////////////////////////////////////////////////////////

void FLandscapeToolKit::RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
}

void FLandscapeToolKit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
}

void FLandscapeToolKit::Init(const TSharedPtr< class IToolkitHost >& InitToolkitHost)
{
	LandscapeEditorWidgets = SNew(SLandscapeEditor, SharedThis(this));

	FModeToolkit::Init(InitToolkitHost);
}

FName FLandscapeToolKit::GetToolkitFName() const
{
	return FName("LandscapeEditor");
}

FText FLandscapeToolKit::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Landscape Editor");
}

class FEdModeLandscape* FLandscapeToolKit::GetEditorMode() const
{
	return (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
}

TSharedPtr<SWidget> FLandscapeToolKit::GetInlineContent() const
{
	return LandscapeEditorWidgets;
}

void FLandscapeToolKit::NotifyToolChanged()
{
	LandscapeEditorWidgets->NotifyToolChanged();
}

void FLandscapeToolKit::NotifyBrushChanged()
{
	LandscapeEditorWidgets->NotifyBrushChanged();
}

//////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SLandscapeEditor::Construct(const FArguments& InArgs, TSharedRef<FLandscapeToolKit> InParentToolkit)
{
	CommandList = InParentToolkit->GetToolkitCommands();

	//FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	//TSharedPtr<SLevelEditor> LevelEditor = StaticCastSharedPtr<SLevelEditor>(LevelEditorModule.GetFirstLevelEditor());
	//CommandList = LevelEditor->GetLevelEditorActions();

	// Modes:
	CommandList->MapAction(FLandscapeEditorCommands::Get().ManageMode, FUIAction(FExecuteAction::CreateSP(this, &SLandscapeEditor::OnChangeMode, FName("ToolMode_Manage")), FCanExecuteAction::CreateSP(this, &SLandscapeEditor::IsModeEnabled, FName(TEXT("ToolMode_Manage"))), FIsActionChecked::CreateSP(this, &SLandscapeEditor::IsModeActive, FName(TEXT("ToolMode_Manage")))));
	CommandList->MapAction(FLandscapeEditorCommands::Get().SculptMode, FUIAction(FExecuteAction::CreateSP(this, &SLandscapeEditor::OnChangeMode, FName("ToolMode_Sculpt")), FCanExecuteAction::CreateSP(this, &SLandscapeEditor::IsModeEnabled, FName(TEXT("ToolMode_Sculpt"))), FIsActionChecked::CreateSP(this, &SLandscapeEditor::IsModeActive, FName(TEXT("ToolMode_Sculpt")))));
	CommandList->MapAction(FLandscapeEditorCommands::Get().PaintMode,  FUIAction(FExecuteAction::CreateSP(this, &SLandscapeEditor::OnChangeMode, FName("ToolMode_Paint" )), FCanExecuteAction::CreateSP(this, &SLandscapeEditor::IsModeEnabled, FName(TEXT("ToolMode_Paint" ))), FIsActionChecked::CreateSP(this, &SLandscapeEditor::IsModeActive, FName(TEXT("ToolMode_Paint" )))));

	FToolBarBuilder ModeSwitchButtons(CommandList, FMultiBoxCustomization::None);
	{
		ModeSwitchButtons.AddToolBarButton(FLandscapeEditorCommands::Get().ManageMode, NAME_None, LOCTEXT("Mode.Manage", "Manage"), LOCTEXT("Mode.Manage.Tooltip", "Contains tools to add a new landscape, import/export landscape, add/remove components and manage streaming"));
		ModeSwitchButtons.AddToolBarButton(FLandscapeEditorCommands::Get().SculptMode, NAME_None, LOCTEXT("Mode.Sculpt", "Sculpt"), LOCTEXT("Mode.Sculpt.Tooltip", "Contains tools that modify the shape of a landscape"));
		ModeSwitchButtons.AddToolBarButton(FLandscapeEditorCommands::Get().PaintMode,  NAME_None, LOCTEXT("Mode.Paint",  "Paint"),  LOCTEXT("Mode.Paint.Tooltip",  "Contains tools that paint materials on to a landscape"));
	}

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs(false, false, false,FDetailsViewArgs::HideNameArea);

	DetailsPanel = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsPanel->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SLandscapeEditor::GetIsPropertyVisible));

	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		DetailsPanel->SetObject(LandscapeEdMode->UISettings);
	}

	IIntroTutorials& IntroTutorials = FModuleManager::LoadModuleChecked<IIntroTutorials>(TEXT("IntroTutorials"));

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 5)
		[
			SAssignNew(Error, SErrorText)
		]
		+ SVerticalBox::Slot()
		.Padding(0)
		[
			SNew(SVerticalBox)
			.IsEnabled(this, &SLandscapeEditor::GetLandscapeEditorIsEnabled)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4, 0, 4, 5)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					.HAlign(HAlign_Center)
					[

						ModeSwitchButtons.MakeWidget()
					]
				]

				// Tutorial link
				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Bottom)
				.Padding(4)
				[
					IntroTutorials.CreateTutorialsWidget(TEXT("LandscapeMode"))
				]
			]
			+ SVerticalBox::Slot()
			.Padding(0)
			[
				DetailsPanel.ToSharedRef()
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

class FEdModeLandscape* SLandscapeEditor::GetEditorMode() const
{
	return (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
}

FText SLandscapeEditor::GetErrorText() const
{
	const FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	ELandscapeEditingState EditState = LandscapeEdMode->GetEditingState();
	switch (EditState)
	{
		case ELandscapeEditingState::SIEWorld:
		{

			if (LandscapeEdMode->NewLandscapePreviewMode != ENewLandscapePreviewMode::None)
			{
				return LOCTEXT("IsSimulatingError_create", "Can't create landscape while simulating!");
			}
			else
			{
				return LOCTEXT("IsSimulatingError_edit", "Can't edit landscape while simulating!");
			}
			break;
		}
		case ELandscapeEditingState::PIEWorld:
		{
			if (LandscapeEdMode->NewLandscapePreviewMode != ENewLandscapePreviewMode::None)
			{
				return LOCTEXT("IsPIEError_create", "Can't create landscape in PIE!");
			}
			else
			{
				return LOCTEXT("IsPIEError_edit", "Can't edit landscape in PIE!");
			}
			break;
		}
		case ELandscapeEditingState::BadFeatureLevel:
		{
			if (LandscapeEdMode->NewLandscapePreviewMode != ENewLandscapePreviewMode::None)
			{
				return LOCTEXT("IsFLError_create", "Can't create landscape with a feature level less than SM4!");
			}
			else
			{
				return LOCTEXT("IsFLError_edit", "Can't edit landscape with a feature level less than SM4!");
			}
			break;
		}
		case ELandscapeEditingState::NoLandscape:
		{
			return LOCTEXT("NoLandscapeError", "No Landscape!");
		}
		case ELandscapeEditingState::Enabled:
		{
			return FText::GetEmpty();
		}
		default:
			checkNoEntry();
	}

	return FText::GetEmpty();
}

bool SLandscapeEditor::GetLandscapeEditorIsEnabled() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		Error->SetError(GetErrorText());
		return LandscapeEdMode->GetEditingState() == ELandscapeEditingState::Enabled;
	}
	return false;
}

bool SLandscapeEditor::GetIsPropertyVisible(const FPropertyAndParent& PropertyAndParent) const
{
	const UProperty& Property = PropertyAndParent.Property;

	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL && LandscapeEdMode->CurrentTool != NULL)
	{
		if (Property.HasMetaData("ShowForMask"))
		{
			const bool bMaskEnabled = LandscapeEdMode->CurrentTool &&
				LandscapeEdMode->CurrentTool->SupportsMask() &&
				LandscapeEdMode->CurrentToolTarget.LandscapeInfo.IsValid() &&
				LandscapeEdMode->CurrentToolTarget.LandscapeInfo->SelectedRegion.Num() > 0;

			if (bMaskEnabled)
			{
				return true;
			}
		}
		if (Property.HasMetaData("ShowForTools"))
		{
			const FName CurrentToolName = LandscapeEdMode->CurrentTool->GetToolName();

			TArray<FString> ShowForTools;
			Property.GetMetaData("ShowForTools").ParseIntoArray(ShowForTools, TEXT(","), true);
			if (!ShowForTools.Contains(CurrentToolName.ToString()))
			{
				return false;
			}
		}
		if (Property.HasMetaData("ShowForBrushes"))
		{
			const FName CurrentBrushSetName = LandscapeEdMode->LandscapeBrushSets[LandscapeEdMode->CurrentBrushSetIndex].BrushSetName;
			// const FName CurrentBrushName = LandscapeEdMode->CurrentBrush->GetBrushName();

			TArray<FString> ShowForBrushes;
			Property.GetMetaData("ShowForBrushes").ParseIntoArray(ShowForBrushes, TEXT(","), true);
			if (!ShowForBrushes.Contains(CurrentBrushSetName.ToString()))
				//&& !ShowForBrushes.Contains(CurrentBrushName.ToString())
			{
				return false;
			}
		}
		if (Property.HasMetaData("ShowForTargetTypes"))
		{
			static const TCHAR* TargetTypeNames[] = { TEXT("Heightmap"), TEXT("Weightmap"), TEXT("Visibility") };

			TArray<FString> ShowForTargetTypes;
			Property.GetMetaData("ShowForTargetTypes").ParseIntoArray(ShowForTargetTypes, TEXT(","), true);

			const ELandscapeToolTargetType::Type CurrentTargetType = LandscapeEdMode->CurrentToolTarget.TargetType;
			if (CurrentTargetType == ELandscapeToolTargetType::Invalid ||
				ShowForTargetTypes.FindByKey(TargetTypeNames[CurrentTargetType]) == nullptr)
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

void SLandscapeEditor::OnChangeMode(FName ModeName)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		LandscapeEdMode->SetCurrentToolMode(ModeName);
	}
}

bool SLandscapeEditor::IsModeEnabled(FName ModeName) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		// Manage is the only mode enabled if we have no landscape
		if (ModeName == "ToolMode_Manage" || LandscapeEdMode->GetLandscapeList().Num() > 0)
		{
			return true;
		}
	}

	return false;
}

bool SLandscapeEditor::IsModeActive(FName ModeName) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentTool)
	{
		return LandscapeEdMode->CurrentToolMode->ToolModeName == ModeName;
	}

	return false;
}

void SLandscapeEditor::NotifyToolChanged()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		// Refresh details panel
		DetailsPanel->SetObject(LandscapeEdMode->UISettings, true);
	}
}

void SLandscapeEditor::NotifyBrushChanged()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		// Refresh details panel
		DetailsPanel->SetObject(LandscapeEdMode->UISettings, true);
	}
}

#undef LOCTEXT_NAMESPACE
