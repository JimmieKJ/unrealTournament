// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorPrivatePCH.h"
#include "LandscapeEdMode.h"
#include "LandscapeEditorObject.h"
#include "LandscapeEditorDetails.h"
#include "LandscapeEditorDetailCustomizations.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "PropertyHandle.h"
#include "PropertyCustomizationHelpers.h"

#include "SLandscapeEditor.h"
#include "DlgPickAssetPath.h"
#include "PackageTools.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "DesktopPlatformModule.h"
#include "MainFrame.h"
#include "AssetRegistryModule.h"

#include "LandscapeRender.h"
#include "LandscapeLayerInfoObject.h"

#define LOCTEXT_NAMESPACE "LandscapeEditor.TargetLayers"

TSharedRef<IDetailCustomization> FLandscapeEditorDetailCustomization_TargetLayers::MakeInstance()
{
	return MakeShareable(new FLandscapeEditorDetailCustomization_TargetLayers);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FLandscapeEditorDetailCustomization_TargetLayers::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TSharedRef<IPropertyHandle> PropertyHandle_PaintingRestriction = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, PaintingRestriction));

	if (!ShouldShowTargetLayers())
	{
		PropertyHandle_PaintingRestriction->MarkHiddenByCustomization();

		return;
	}

	IDetailCategoryBuilder& TargetsCategory = DetailBuilder.EditCategory("Target Layers");

	TargetsCategory.AddProperty(PropertyHandle_PaintingRestriction)
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateStatic(&FLandscapeEditorDetailCustomization_TargetLayers::GetVisibility_PaintingRestriction)));

	TargetsCategory.AddCustomBuilder(MakeShareable(new FLandscapeEditorCustomNodeBuilder_TargetLayers(DetailBuilder.GetThumbnailPool().ToSharedRef())));
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

bool FLandscapeEditorDetailCustomization_TargetLayers::ShouldShowTargetLayers()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentToolMode)
	{
		//bool bSupportsHeightmap = (LandscapeEdMode->CurrentToolMode->SupportedTargetTypes & ELandscapeToolTargetTypeMask::Heightmap) != 0;
		//bool bSupportsWeightmap = (LandscapeEdMode->CurrentToolMode->SupportedTargetTypes & ELandscapeToolTargetTypeMask::Weightmap) != 0;
		//bool bSupportsVisibility = (LandscapeEdMode->CurrentToolMode->SupportedTargetTypes & ELandscapeToolTargetTypeMask::Visibility) != 0;

		//// Visible if there are possible choices
		//if (bSupportsWeightmap || bSupportsHeightmap || bSupportsVisibility)
		if (LandscapeEdMode->CurrentToolMode->SupportedTargetTypes != 0)
		{
			return true;
		}
	}

	return false;
}

bool FLandscapeEditorDetailCustomization_TargetLayers::ShouldShowPaintingRestriction()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		if (LandscapeEdMode->CurrentToolTarget.TargetType == ELandscapeToolTargetType::Weightmap ||
			LandscapeEdMode->CurrentToolTarget.TargetType == ELandscapeToolTargetType::Visibility)
		{
			return true;
		}
	}

	return false;
}

EVisibility FLandscapeEditorDetailCustomization_TargetLayers::GetVisibility_PaintingRestriction()
{
	return ShouldShowPaintingRestriction() ? EVisibility::Visible : EVisibility::Collapsed;
}

//////////////////////////////////////////////////////////////////////////

FEdModeLandscape* FLandscapeEditorCustomNodeBuilder_TargetLayers::GetEditorMode()
{
	return (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
}

FLandscapeEditorCustomNodeBuilder_TargetLayers::FLandscapeEditorCustomNodeBuilder_TargetLayers(TSharedRef<FAssetThumbnailPool> InThumbnailPool)
	: ThumbnailPool(InThumbnailPool)
{
}

FLandscapeEditorCustomNodeBuilder_TargetLayers::~FLandscapeEditorCustomNodeBuilder_TargetLayers()
{
	FEdModeLandscape::TargetsListUpdated.RemoveAll(this);
}

void FLandscapeEditorCustomNodeBuilder_TargetLayers::SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren)
{
	FEdModeLandscape::TargetsListUpdated.RemoveAll(this);
	if (InOnRegenerateChildren.IsBound())
	{
		FEdModeLandscape::TargetsListUpdated.Add(InOnRegenerateChildren);
	}
}

void FLandscapeEditorCustomNodeBuilder_TargetLayers::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{

}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FLandscapeEditorCustomNodeBuilder_TargetLayers::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		const TArray<TSharedRef<FLandscapeTargetListInfo>>& TargetsList = LandscapeEdMode->GetTargetList();

		for (int32 i = 0; i < TargetsList.Num(); i++)
		{
			const TSharedRef<FLandscapeTargetListInfo>& Target = TargetsList[i];
			GenerateRow(ChildrenBuilder, Target);
		}
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FLandscapeEditorCustomNodeBuilder_TargetLayers::GenerateRow(IDetailChildrenBuilder& ChildrenBuilder, const TSharedRef<FLandscapeTargetListInfo> Target)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		if ((LandscapeEdMode->CurrentTool->GetSupportedTargetTypes() & LandscapeEdMode->CurrentToolMode->SupportedTargetTypes & ELandscapeToolTargetTypeMask::FromType(Target->TargetType)) == 0)
		{
			return;
		}
	}
	
	if (Target->TargetType != ELandscapeToolTargetType::Weightmap)
	{
		ChildrenBuilder.AddChildContent(Target->TargetName)
		[
			SNew(SLandscapeEditorSelectableBorder)
			.Padding(0)
			.VAlign(VAlign_Center)
			.OnContextMenuOpening_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerContextMenuOpening, Target)
			.OnSelected_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetSelectionChanged, Target)
			.IsSelected_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerIsSelected, Target)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(2))
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush(Target->TargetType == ELandscapeToolTargetType::Heightmap ? TEXT("LandscapeEditor.Target_Heightmap") : TEXT("LandscapeEditor.Target_Visibility")))
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(4, 0)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Center)
					.Padding(0, 2)
					[
						SNew(STextBlock)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.Text(Target->TargetName)
						.ShadowOffset(FVector2D::UnitVector)
					]
				]
			]
		];
	}
	else
	{
		ChildrenBuilder.AddChildContent(Target->TargetName)
		[
			SNew(SLandscapeEditorSelectableBorder)
			.Padding(0)
			.VAlign(VAlign_Center)
			.OnContextMenuOpening_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerContextMenuOpening, Target)
			.OnSelected_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetSelectionChanged, Target)
			.IsSelected_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerIsSelected, Target)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(2))
				[
					(Target->bValid)
					? (TSharedRef<SWidget>)(
					SNew(SLandscapeAssetThumbnail, Target->ThumbnailMIC.Get(), ThumbnailPool)
					.ThumbnailSize(FIntPoint(48, 48))
					)
					: (TSharedRef<SWidget>)(
					SNew(SImage)
					.Image(FEditorStyle::GetBrush(TEXT("LandscapeEditor.Target_Invalid")))
					)
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(4, 0)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Center)
					.Padding(0, 2, 0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						[
							SNew(STextBlock)
							.Font(IDetailLayoutBuilder::GetDetailFont())
							.Text(Target->TargetName)
							.ShadowOffset(FVector2D::UnitVector)
						]
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Right)
						[
							SNew(STextBlock)
							.Visibility((Target->LayerInfoObj.IsValid() && Target->LayerInfoObj->bNoWeightBlend) ? EVisibility::Visible : EVisibility::Collapsed)
							.Font(IDetailLayoutBuilder::GetDetailFont())
							.Text(LOCTEXT("NoWeightBlend", "No Weight-Blend"))
							.ShadowOffset(FVector2D::UnitVector)
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)
						.Visibility_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerInfoSelectorVisibility, Target)
						+ SHorizontalBox::Slot()
						.FillWidth(1)
						.VAlign(VAlign_Center)
						[
							SNew(SObjectPropertyEntryBox)
							.IsEnabled((bool)Target->bValid)
							.ObjectPath(Target->LayerInfoObj != NULL ? Target->LayerInfoObj->GetPathName() : FString())
							.AllowedClass(ULandscapeLayerInfoObject::StaticClass())
							.OnObjectChanged_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerSetObject, Target)
							.OnShouldFilterAsset_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::ShouldFilterLayerInfo, Target->LayerName)
							.AllowClear(false)
							//.DisplayThumbnail(false)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SComboButton)
							.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
							.HasDownArrow(false)
							.ContentPadding(4.0f)
							.ForegroundColor(FSlateColor::UseForeground())
							.IsFocusable(false)
							.ToolTipText(LOCTEXT("Tooltip_Create", "Create Layer Info"))
							.Visibility_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerCreateVisibility, Target)
							.OnGetMenuContent_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnGetTargetLayerCreateMenu, Target)
							.ButtonContent()
							[
								SNew(SImage)
								.Image(FEditorStyle::GetBrush("LandscapeEditor.Target_Create"))
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
							.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
							.ContentPadding(4.0f)
							.ForegroundColor(FSlateColor::UseForeground())
							.IsFocusable(false)
							.ToolTipText(LOCTEXT("Tooltip_MakePublic", "Make Layer Public (move layer info into asset file)"))
							.Visibility_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerMakePublicVisibility, Target)
							.OnClicked_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerMakePublicClicked, Target)
							[
								SNew(SImage)
								.Image(FEditorStyle::GetBrush("LandscapeEditor.Target_MakePublic"))
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
							.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
							.ContentPadding(4.0f)
							.ForegroundColor(FSlateColor::UseForeground())
							.IsFocusable(false)
							.ToolTipText(LOCTEXT("Tooltip_Delete", "Delete Layer"))
							.Visibility_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerDeleteVisibility, Target)
							.OnClicked_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerDeleteClicked, Target)
							[
								SNew(SImage)
								.Image(FEditorStyle::GetBrush("LandscapeEditor.Target_Delete"))
							]
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						.Visibility_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::GetDebugModeColorChannelVisibility, Target)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0, 2, 2, 2)
						[
							SNew(SCheckBox)
							.IsChecked_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::DebugModeColorChannelIsChecked, Target, 0)
							.OnCheckStateChanged_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnDebugModeColorChannelChanged, Target, 0)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ViewMode.Debug_None", "None"))
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2)
						[
							SNew(SCheckBox)
							.IsChecked_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::DebugModeColorChannelIsChecked, Target, 1)
							.OnCheckStateChanged_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnDebugModeColorChannelChanged, Target, 1)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ViewMode.Debug_R", "R"))
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2)
						[
							SNew(SCheckBox)
							.IsChecked_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::DebugModeColorChannelIsChecked, Target, 2)
							.OnCheckStateChanged_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnDebugModeColorChannelChanged, Target, 2)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ViewMode.Debug_G", "G"))
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2)
						[
							SNew(SCheckBox)
							.IsChecked_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::DebugModeColorChannelIsChecked, Target, 4)
							.OnCheckStateChanged_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnDebugModeColorChannelChanged, Target, 4)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ViewMode.Debug_B", "B"))
							]
						]
					]
				]
			]
		];
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

bool FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerIsSelected(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		return
			LandscapeEdMode->CurrentToolTarget.TargetType == Target->TargetType &&
			LandscapeEdMode->CurrentToolTarget.LayerName == Target->LayerName &&
			LandscapeEdMode->CurrentToolTarget.LayerInfo == Target->LayerInfoObj; // may be null
	}

	return false;
}

void FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetSelectionChanged(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		LandscapeEdMode->CurrentToolTarget.TargetType = Target->TargetType;
		if (Target->TargetType == ELandscapeToolTargetType::Heightmap)
		{
			checkSlow(Target->LayerInfoObj == NULL);
			LandscapeEdMode->CurrentToolTarget.LayerInfo = NULL;
			LandscapeEdMode->CurrentToolTarget.LayerName = NAME_None;
		}
		else
		{
			LandscapeEdMode->CurrentToolTarget.LayerInfo = Target->LayerInfoObj;
			LandscapeEdMode->CurrentToolTarget.LayerName = Target->LayerName;
		}
	}
}

TSharedPtr<SWidget> FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerContextMenuOpening(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	if (Target->TargetType == ELandscapeToolTargetType::Heightmap || Target->LayerInfoObj != NULL)
	{
		FMenuBuilder MenuBuilder(true, NULL);

		MenuBuilder.BeginSection("LandscapeEditorLayerActions", LOCTEXT("LayerContextMenu.Heading", "Layer Actions"));
		{
			// Export
			FUIAction ExportAction = FUIAction(FExecuteAction::CreateStatic(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnExportLayer, Target));
			MenuBuilder.AddMenuEntry(LOCTEXT("LayerContextMenu.Export", "Export to file"), FText(), FSlateIcon(), ExportAction);

			// Import
			FUIAction ImportAction = FUIAction(FExecuteAction::CreateStatic(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnImportLayer, Target));
			MenuBuilder.AddMenuEntry(LOCTEXT("LayerContextMenu.Import", "Import from file"), FText(), FSlateIcon(), ImportAction);

			// Reimport
			const FString& ReimportPath = Target->ReimportFilePath();

			if (!ReimportPath.IsEmpty())
			{
				FUIAction ReImportAction = FUIAction(FExecuteAction::CreateStatic(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnReimportLayer, Target));
				MenuBuilder.AddMenuEntry(FText::Format(LOCTEXT("LayerContextMenu.ReImport", "Reimport from {0}"), FText::FromString(ReimportPath)), FText(), FSlateIcon(), ReImportAction);
			}
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

	return NULL;
}

void FLandscapeEditorCustomNodeBuilder_TargetLayers::OnExportLayer(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
		check(MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid());
		void* ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();

		ULandscapeInfo* LandscapeInfo = Target->LandscapeInfo.Get();
		ULandscapeLayerInfoObject* LayerInfoObj = Target->LayerInfoObj.Get(); // NULL for heightmaps

		// Prompt for filename
		FString SaveDialogTitle;
		FString DefaultFileName;
		FString FileTypes;

		if (Target->TargetType == ELandscapeToolTargetType::Heightmap)
		{
			SaveDialogTitle = *LOCTEXT("ExportHeightmap", "Export Landscape Heightmap").ToString();
			DefaultFileName = TEXT("Heightmap.png");
			FileTypes = TEXT("Heightmap png files|*.png|Heightmap .raw files|*.raw|Heightmap .r16 files|*.r16|All files|*.*");
		}
		else //if (Target->TargetType == ELandscapeToolTargetType::Weightmap)
		{
			SaveDialogTitle = *FText::Format(LOCTEXT("ExportLayer", "Export Landscape Layer: {0}"), FText::FromName(LayerInfoObj->LayerName)).ToString();
			DefaultFileName = *FString::Printf(TEXT("%s.png"), *(LayerInfoObj->LayerName.ToString()));
			FileTypes = TEXT("Layer png files|*.png|Layer .raw files|*.raw|Layer .r8 files|*.r8|All files|*.*");
		}

		// Prompt the user for the filenames
		TArray<FString> SaveFilenames;
		bool bOpened = DesktopPlatform->SaveFileDialog(
			ParentWindowWindowHandle,
			*SaveDialogTitle,
			*LandscapeEdMode->UISettings->LastImportPath,
			*DefaultFileName,
			*FileTypes,
			EFileDialogFlags::None,
			SaveFilenames
			);

		if (bOpened)
		{
			const FString& SaveFilename(SaveFilenames[0]);
			LandscapeEdMode->UISettings->LastImportPath = FPaths::GetPath(SaveFilename);

			// Actually do the export
			if (Target->TargetType == ELandscapeToolTargetType::Heightmap)
			{
				LandscapeInfo->ExportHeightmap(SaveFilename);
			}
			else //if (Target->TargetType == ELandscapeToolTargetType::Weightmap)
			{
				LandscapeInfo->ExportLayer(LayerInfoObj, SaveFilename);
			}

			Target->ReimportFilePath() = SaveFilename;
		}
	}
}

void FLandscapeEditorCustomNodeBuilder_TargetLayers::OnImportLayer(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
		check(MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid());
		void* ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();

		ULandscapeInfo* LandscapeInfo = Target->LandscapeInfo.Get();
		ULandscapeLayerInfoObject* LayerInfoObj = Target->LayerInfoObj.Get(); // NULL for heightmaps

		// Prompt for filename
		FString OpenDialogTitle;
		FString DefaultFileName;
		FString FileTypes;

		if (Target->TargetType == ELandscapeToolTargetType::Heightmap)
		{
			OpenDialogTitle = *LOCTEXT("ImportHeightmap", "Import Landscape Heightmap").ToString();
			DefaultFileName = TEXT("Heightmap.png");
			FileTypes = TEXT("All Heightmap files|*.png;*.raw;*.r16|Heightmap png files|*.png|Heightmap .raw/.r16 files|*.raw;*.r16|All files|*.*");
		}
		else //if (Target->TargetType == ELandscapeToolTargetType::Weightmap)
		{
			OpenDialogTitle = *FText::Format(LOCTEXT("ImportLayer", "Import Landscape Layer: {0}"), FText::FromName(LayerInfoObj->LayerName)).ToString();
			DefaultFileName = *FString::Printf(TEXT("%s.png"), *(LayerInfoObj->LayerName.ToString()));
			FileTypes = TEXT("All Layer files|*.png;*.raw;*.r8|Layer png files|*.png|Layer .raw/.r8 files|*.raw;*.r8|All files|*.*");
		}

		// Prompt the user for the filenames
		TArray<FString> OpenFilenames;
		bool bOpened = DesktopPlatform->OpenFileDialog(
			ParentWindowWindowHandle,
			*OpenDialogTitle,
			*LandscapeEdMode->UISettings->LastImportPath,
			*DefaultFileName,
			*FileTypes,
			EFileDialogFlags::None,
			OpenFilenames
			);

		if (bOpened)
		{
			const FString& OpenFilename(OpenFilenames[0]);
			LandscapeEdMode->UISettings->LastImportPath = FPaths::GetPath(OpenFilename);

			// Actually do the Import
			LandscapeEdMode->ImportData(*Target, OpenFilename);

			Target->ReimportFilePath() = OpenFilename;
		}
	}
}

void FLandscapeEditorCustomNodeBuilder_TargetLayers::OnReimportLayer(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		LandscapeEdMode->ReimportData(*Target);
	}
}

bool FLandscapeEditorCustomNodeBuilder_TargetLayers::ShouldFilterLayerInfo(const FAssetData& AssetData, FName LayerName)
{
	const FString* const LayerNameMetaData = AssetData.TagsAndValues.Find("LayerName");
	if (LayerNameMetaData && !LayerNameMetaData->IsEmpty())
	{
		return FName(**LayerNameMetaData) != LayerName;
	}

	ULandscapeLayerInfoObject* LayerInfo = CastChecked<ULandscapeLayerInfoObject>(AssetData.GetAsset());
	return LayerInfo->LayerName != LayerName;
}

void FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerSetObject(const FAssetData& AssetData, const TSharedRef<FLandscapeTargetListInfo> Target)
{
	// Can't assign null to a layer
	UObject* Object = AssetData.GetAsset();
	if (Object == NULL)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("Undo_UseExisting", "Assigning Layer to Landscape"));

	ULandscapeLayerInfoObject* SelectedLayerInfo = const_cast<ULandscapeLayerInfoObject*>(CastChecked<ULandscapeLayerInfoObject>(Object));

	if (SelectedLayerInfo != Target->LayerInfoObj.Get())
	{
		if (ensure(SelectedLayerInfo->LayerName == Target->GetLayerName()))
		{
			ULandscapeInfo* LandscapeInfo = Target->LandscapeInfo.Get();
			LandscapeInfo->Modify();
			if (Target->LayerInfoObj.IsValid())
			{
				int32 Index = LandscapeInfo->GetLayerInfoIndex(Target->LayerInfoObj.Get(), Target->Owner.Get());
				if (ensure(Index != INDEX_NONE))
				{
					FLandscapeInfoLayerSettings& LayerSettings = LandscapeInfo->Layers[Index];

					LandscapeInfo->ReplaceLayer(LayerSettings.LayerInfoObj, SelectedLayerInfo);

					LayerSettings.LayerInfoObj = SelectedLayerInfo;
				}
			}
			else
			{
				int32 Index = LandscapeInfo->GetLayerInfoIndex(Target->LayerName, Target->Owner.Get());
				if (ensure(Index != INDEX_NONE))
				{
					FLandscapeInfoLayerSettings& LayerSettings = LandscapeInfo->Layers[Index];
					LayerSettings.LayerInfoObj = SelectedLayerInfo;

					Target->LandscapeInfo->CreateLayerEditorSettingsFor(SelectedLayerInfo);
				}
			}

			FEdModeLandscape* LandscapeEdMode = GetEditorMode();
			if (LandscapeEdMode)
			{
				if (LandscapeEdMode->CurrentToolTarget.LayerName == Target->LayerName
					&& LandscapeEdMode->CurrentToolTarget.LayerInfo == Target->LayerInfoObj)
				{
					LandscapeEdMode->CurrentToolTarget.LayerInfo = SelectedLayerInfo;
				}
				LandscapeEdMode->UpdateTargetList();
			}
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_LayerNameMismatch", "Can't use this layer info because the layer name does not match"));
		}
	}
}

EVisibility FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerInfoSelectorVisibility(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	if (Target->TargetType == ELandscapeToolTargetType::Weightmap)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

EVisibility FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerCreateVisibility(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	if (!Target->LayerInfoObj.IsValid())
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

EVisibility FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerMakePublicVisibility(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	if (Target->bValid && Target->LayerInfoObj.IsValid() && Target->LayerInfoObj->GetOutermost()->ContainsMap())
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

EVisibility FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerDeleteVisibility(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	if (!Target->bValid)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

TSharedRef<SWidget> FLandscapeEditorCustomNodeBuilder_TargetLayers::OnGetTargetLayerCreateMenu(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	FMenuBuilder MenuBuilder(true, NULL);

	MenuBuilder.AddMenuEntry(LOCTEXT("Menu_Create_Blended", "Weight-Blended Layer (normal)"), FText(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerCreateClicked, Target, false)));

	MenuBuilder.AddMenuEntry(LOCTEXT("Menu_Create_NoWeightBlend", "Non Weight-Blended Layer"), FText(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerCreateClicked, Target, true)));

	return MenuBuilder.MakeWidget();
}

void FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerCreateClicked(const TSharedRef<FLandscapeTargetListInfo> Target, bool bNoWeightBlend)
{
	check(!Target->LayerInfoObj.IsValid());

	FScopedTransaction Transaction(LOCTEXT("Undo_Create", "Creating New Landscape Layer"));

	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		FName LayerName = Target->GetLayerName();
		ULevel* Level = Target->Owner->GetLevel();

		// Build default layer object name and package name
		FName LayerObjectName = FName(*FString::Printf(TEXT("%s_LayerInfo"), *LayerName.ToString()));
		FString Path = Level->GetOutermost()->GetName() + TEXT("_sharedassets/");
		if (Path.StartsWith("/Temp/"))
		{
			Path = FString("/Game/") + Path.RightChop(FString("/Temp/").Len());
		}
		FString PackageName = Path + LayerObjectName.ToString();

		TSharedRef<SDlgPickAssetPath> NewLayerDlg =
			SNew(SDlgPickAssetPath)
			.Title(LOCTEXT("CreateNewLayerInfo", "Create New Landscape Layer Info Object"))
			.DefaultAssetPath(FText::FromString(PackageName));

		if (NewLayerDlg->ShowModal() != EAppReturnType::Cancel)
		{
			PackageName = NewLayerDlg->GetFullAssetPath().ToString();
			LayerObjectName = FName(*NewLayerDlg->GetAssetName().ToString());

			UPackage* Package = CreatePackage(NULL, *PackageName);
			ULandscapeLayerInfoObject* LayerInfo = NewObject<ULandscapeLayerInfoObject>(Package, LayerObjectName, RF_Public | RF_Standalone | RF_Transactional);
			LayerInfo->LayerName = LayerName;
			LayerInfo->bNoWeightBlend = bNoWeightBlend;

			ULandscapeInfo* LandscapeInfo = Target->LandscapeInfo.Get();
			LandscapeInfo->Modify();
			int32 Index = LandscapeInfo->GetLayerInfoIndex(LayerName, Target->Owner.Get());
			if (Index == INDEX_NONE)
			{
				LandscapeInfo->Layers.Add(FLandscapeInfoLayerSettings(LayerInfo, Target->Owner.Get()));
			}
			else
			{
				LandscapeInfo->Layers[Index].LayerInfoObj = LayerInfo;
			}

			if (LandscapeEdMode->CurrentToolTarget.LayerName == Target->LayerName
				&& LandscapeEdMode->CurrentToolTarget.LayerInfo == Target->LayerInfoObj)
			{
				LandscapeEdMode->CurrentToolTarget.LayerInfo = LayerInfo;
			}

			Target->LayerInfoObj = LayerInfo;
			Target->LandscapeInfo->CreateLayerEditorSettingsFor(LayerInfo);

			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(LayerInfo);

			// Mark the package dirty...
			Package->MarkPackageDirty();

			// Show in the content browser
			TArray<UObject*> Objects;
			Objects.Add(LayerInfo);
			GEditor->SyncBrowserToObjects(Objects);
			
			LandscapeEdMode->TargetsListUpdated.Broadcast();
		}
	}
}

FReply FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerMakePublicClicked(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	FScopedTransaction Transaction(LOCTEXT("Undo_MakePublic", "Make Layer Public"));
	TArray<UObject*> Objects;
	Objects.Add(Target->LayerInfoObj.Get());

	FString Path = Target->Owner->GetOutermost()->GetName() + TEXT("_sharedassets");
	bool bSucceed = ObjectTools::RenameObjects(Objects, false, TEXT(""), Path);
	if (bSucceed)
	{
		FEdModeLandscape* LandscapeEdMode = GetEditorMode();
		if (LandscapeEdMode)
		{
			LandscapeEdMode->UpdateTargetList();
		}
	}
	else
	{
		Transaction.Cancel();
	}

	return FReply::Handled();
}

FReply FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerDeleteClicked(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	check(Target->LayerInfoObj.IsValid());
	check(Target->LandscapeInfo.IsValid());

	if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("Prompt_DeleteLayer", "Are you sure you want to delete this layer?")) == EAppReturnType::Yes)
	{
		FScopedTransaction Transaction(LOCTEXT("Undo_Delete", "Delete Layer"));

		Target->LandscapeInfo->DeleteLayer(Target->LayerInfoObj.Get());

		FEdModeLandscape* LandscapeEdMode = GetEditorMode();
		if (LandscapeEdMode)
		{
			LandscapeEdMode->UpdateTargetList();
		}
	}

	return FReply::Handled();
}

EVisibility FLandscapeEditorCustomNodeBuilder_TargetLayers::GetDebugModeColorChannelVisibility(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	if (GLandscapeViewMode == ELandscapeViewMode::DebugLayer && Target->TargetType != ELandscapeToolTargetType::Heightmap && Target->LayerInfoObj.IsValid())
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

ECheckBoxState FLandscapeEditorCustomNodeBuilder_TargetLayers::DebugModeColorChannelIsChecked(const TSharedRef<FLandscapeTargetListInfo> Target, int32 Channel)
{
	if (Target->DebugColorChannel == Channel)
	{
		return ECheckBoxState::Checked;
	}

	return ECheckBoxState::Unchecked;
}

void FLandscapeEditorCustomNodeBuilder_TargetLayers::OnDebugModeColorChannelChanged(ECheckBoxState NewCheckedState, const TSharedRef<FLandscapeTargetListInfo> Target, int32 Channel)
{
	if (NewCheckedState == ECheckBoxState::Checked)
	{
		// Enable on us and disable colour channel on other targets
		if (ensure(Target->LayerInfoObj != NULL))
		{
			ULandscapeInfo* LandscapeInfo = Target->LandscapeInfo.Get();
			int32 Index = LandscapeInfo->GetLayerInfoIndex(Target->LayerInfoObj.Get(), Target->Owner.Get());
			if (ensure(Index != INDEX_NONE))
			{
				for (auto It = LandscapeInfo->Layers.CreateIterator(); It; It++)
				{
					FLandscapeInfoLayerSettings& LayerSettings = *It;
					if (It.GetIndex() == Index)
					{
						LayerSettings.DebugColorChannel = Channel;
					}
					else
					{
						LayerSettings.DebugColorChannel &= ~Channel;
					}
				}
				LandscapeInfo->UpdateDebugColorMaterial();

				FEdModeLandscape* LandscapeEdMode = GetEditorMode();
				if (LandscapeEdMode)
				{
					LandscapeEdMode->UpdateTargetList();
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void SLandscapeEditorSelectableBorder::Construct(const FArguments& InArgs)
{
	SBorder::Construct(
		SBorder::FArguments()
		.HAlign(InArgs._HAlign)
		.VAlign(InArgs._VAlign)
		.Padding(InArgs._Padding)
		.BorderImage(this, &SLandscapeEditorSelectableBorder::GetBorder)
		.Content()
		[
			InArgs._Content.Widget
		]
	);

	OnContextMenuOpening = InArgs._OnContextMenuOpening;
	OnSelected = InArgs._OnSelected;
	IsSelected = InArgs._IsSelected;
}

FReply SLandscapeEditorSelectableBorder::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MyGeometry.IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton &&
			OnSelected.IsBound())
		{
			OnSelected.Execute();

			return FReply::Handled().ReleaseMouseCapture();
		}
		else if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton &&
			OnContextMenuOpening.IsBound())
		{
			TSharedPtr<SWidget> Content = OnContextMenuOpening.Execute();
			if (Content.IsValid())
			{
				FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();

				FSlateApplication::Get().PushMenu(SharedThis(this), WidgetPath, Content.ToSharedRef(), MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
			}

			return FReply::Handled().ReleaseMouseCapture();
		}
	}

	return FReply::Unhandled();
}

const FSlateBrush* SLandscapeEditorSelectableBorder::GetBorder() const
{
	const bool bIsSelected = IsSelected.Get();
	const bool bIsHovered = IsHovered() && OnSelected.IsBound();

	if (bIsSelected)
	{
		return bIsHovered
			? FEditorStyle::GetBrush("LandscapeEditor.TargetList", ".RowSelectedHovered")
			: FEditorStyle::GetBrush("LandscapeEditor.TargetList", ".RowSelected");
	}
	else
	{
		return bIsHovered
			? FEditorStyle::GetBrush("LandscapeEditor.TargetList", ".RowBackgroundHovered")
			: FEditorStyle::GetBrush("LandscapeEditor.TargetList", ".RowBackground");
	}
}

#undef LOCTEXT_NAMESPACE
