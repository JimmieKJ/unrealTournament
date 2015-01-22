// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"

#include "SScaleBox.h"

#include "SFoliageEdit.h"
#include "FoliageEditActions.h"
#include "FoliageEdMode.h"
#include "Foliage/FoliageType.h"
#include "Editor/UnrealEd/Public/AssetThumbnail.h"
#include "Editor/UnrealEd/Public/DragAndDrop/AssetDragDropOp.h"
#include "Editor/UnrealEd/Public/AssetSelection.h"
#include "Editor/IntroTutorials/Public/IIntroTutorials.h"
#include "SFoliageEditMeshDisplayItem.h"
#include "Engine/StaticMesh.h"

#define LOCTEXT_NAMESPACE "FoliageEd_Mode"

class SFoliageDragDropHandler : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFoliageDragDropHandler) {}
	SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_ARGUMENT(TWeakPtr<SFoliageEdit>, FoliageEditPtr)
	SLATE_END_ARGS()

	/** SCompoundWidget functions */
	void Construct(const FArguments& InArgs)
	{
		FoliageEditPtr = InArgs._FoliageEditPtr;
		bIsDragOn = false;

		this->ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(this, &SFoliageDragDropHandler::GetBackgroundColor)
			.Padding(FMargin(30))
			[
				InArgs._Content.Widget
			]
		];
	}

	~SFoliageDragDropHandler() {}

	FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
	{
		bIsDragOn = false;

		TSharedPtr<SFoliageEdit> PinnedPtr = FoliageEditPtr.Pin();
		if (PinnedPtr.IsValid())
		{
			return PinnedPtr->OnDrop_ListView(MyGeometry, DragDropEvent);
		}
		return FReply::Handled();
	}

	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
	{
		bIsDragOn = true;
	}

	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override
	{
		bIsDragOn = false;
	}

private:
	FSlateColor GetBackgroundColor() const
	{
		return bIsDragOn ? FLinearColor(1.0f, 0.6f, 0.1f, 0.9f) : FLinearColor(0.1f, 0.1f, 0.1f, 0.9f);
	}

private:
	TWeakPtr<SFoliageEdit> FoliageEditPtr;
	bool bIsDragOn;
};

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SFoliageEdit::Construct(const FArguments& InArgs)
{
	FoliageEditMode = (FEdModeFoliage*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Foliage);

	FFoliageEditCommands::Register();

	UICommandList = MakeShareable(new FUICommandList);

	BindCommands();

	FEditorDelegates::NewCurrentLevel.AddSP(this, &SFoliageEdit::NotifyNewCurrentLevel);

	AssetThumbnailPool = MakeShareable(new FAssetThumbnailPool(512, TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SFoliageEdit::IsHovered))));

	// Everything (or almost) uses this padding, change it to expand the padding.
	FMargin StandardPadding(0.0f, 4.0f, 0.0f, 4.0f);

	this->ChildSlot
	[
		SNew(SOverlay)

		+ SOverlay::Slot()
			[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			.Padding(2.0f, 0)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(StandardPadding)
				[
					BuildToolBar()
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					.ToolTipText(LOCTEXT("BrushSize_Tooltip", "The size of the foliage brush"))

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(StandardPadding)
					[
						SNew(STextBlock)
						.Visibility(this, &SFoliageEdit::GetVisibility_Radius)
						.Text(LOCTEXT("BrushSize", "Brush Size"))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(2.0f)
					.Padding(StandardPadding)
					[
						SNew(SSpinBox<float>)
						.Visibility(this, &SFoliageEdit::GetVisibility_Radius)
						.MinValue(0.0f)
						.MaxValue(65536.0f)
						.MaxSliderValue(8192.0f)
						.SliderExponent(3.0f)
						.Value(this, &SFoliageEdit::GetRadius)
						.OnValueChanged(this, &SFoliageEdit::SetRadius)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					.ToolTipText(LOCTEXT("PaintDensity_Tooltip", "The density of foliage to paint. This is a multiplier for the individual foliage type's density specifier."))

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(StandardPadding)
					[
						SNew(STextBlock)
						.Visibility(this, &SFoliageEdit::GetVisibility_PaintDensity)
						.Text(LOCTEXT("PaintDensity", "Paint Density"))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(2.0f)
					.Padding(StandardPadding)
					[
						SNew(SSpinBox<float>)
						.Visibility(this, &SFoliageEdit::GetVisibility_PaintDensity)
						.MinValue(0.0f)
						.MaxValue(2.0f)
						.MaxSliderValue(1.0f)
						.Value(this, &SFoliageEdit::GetPaintDensity)
						.OnValueChanged(this, &SFoliageEdit::SetPaintDensity)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					.ToolTipText(LOCTEXT("EraseDensity_Tooltip", "The density of foliage to leave behind when erasing with the Shift key held. 0 will remove all foliage."))

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(StandardPadding)
					[
						SNew(STextBlock)
						.Visibility(this, &SFoliageEdit::GetVisibility_EraseDensity)
						.Text(LOCTEXT("EraseDensity", "Erase Density"))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(2.0f)
					.Padding(StandardPadding)
					[
						SNew(SSpinBox<float>)
						.Visibility(this, &SFoliageEdit::GetVisibility_EraseDensity)
						.MinValue(0.0f)
						.MaxValue(2.0f)
						.MaxSliderValue(1.0f)
						.Value(this, &SFoliageEdit::GetEraseDensity)
						.OnValueChanged(this, &SFoliageEdit::SetEraseDensity)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(StandardPadding)
				[
					SNew(STextBlock)
					.Visibility(this, &SFoliageEdit::GetVisibility_Filters)
					.Text(LOCTEXT("Filter", "Filter"))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(StandardPadding)
				[
					SNew(SWrapBox)
					.UseAllottedWidth(true)

					+ SWrapBox::Slot()
					.Padding(0, 0, 16, 0)
					[
						SNew(SCheckBox)
						.Visibility(this, &SFoliageEdit::GetVisibility_Filters)
						.OnCheckStateChanged(this, &SFoliageEdit::OnCheckStateChanged_Landscape)
						.IsChecked(this, &SFoliageEdit::GetCheckState_Landscape)
						.ToolTipText(LOCTEXT("FilterLandscape_Tooltip", "Place foliage on Landscape"))
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Landscape", "Landscape"))
						]
					]

					+ SWrapBox::Slot()
					.Padding(0, 0, 16, 0)
					[
						SNew(SCheckBox)
						.Visibility(this, &SFoliageEdit::GetVisibility_Filters)
						.OnCheckStateChanged(this, &SFoliageEdit::OnCheckStateChanged_StaticMesh)
						.IsChecked(this, &SFoliageEdit::GetCheckState_StaticMesh)
						.ToolTipText(LOCTEXT("FilterStaticMesh_Tooltip", "Place foliage on StaticMeshes"))
						[
							SNew(STextBlock)
							.Text(LOCTEXT("StaticMeshes", "Static Meshes"))
						]
					]

					+ SWrapBox::Slot()
					.Padding(0, 0, 16, 0)
					[
						SNew(SCheckBox)
						.Visibility(this, &SFoliageEdit::GetVisibility_Filters)
						.OnCheckStateChanged(this, &SFoliageEdit::OnCheckStateChanged_BSP)
						.IsChecked(this, &SFoliageEdit::GetCheckState_BSP)
						.ToolTipText(LOCTEXT("FilterBSP_Tooltip", "Place foliage on BSP"))
						[
							SNew(STextBlock)
							.Text(LOCTEXT("BSP", "BSP"))
						]
					]

					+ SWrapBox::Slot()
					.Padding(0, 0, 16, 0)
					[
						SNew(SCheckBox)
						.Visibility(this, &SFoliageEdit::GetVisibility_Filters)
						.OnCheckStateChanged(this, &SFoliageEdit::OnCheckStateChanged_Translucent)
						.IsChecked(this, &SFoliageEdit::GetCheckState_Translucent)
						.ToolTipText(LOCTEXT("FilterTranslucent_Tooltip", "Place foliage on translucent surfaces"))
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Translucent", "Translucent"))
						]
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHeaderRow)

					+ SHeaderRow::Column(TEXT("Meshes"))
					.DefaultLabel(LOCTEXT("Meshes", "Meshes"))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(StandardPadding)
				[
					SNew(SBox)
					.Visibility(this, &SFoliageEdit::GetVisibility_EmptyList)
					.Padding(FMargin(15, 0))
					.MinDesiredHeight(30)
					[
						SNew(SScaleBox)
						.Stretch(EStretch::ScaleToFit)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Foliage_DropStatic", "+ Drop Static Meshes Here"))
							.ToolTipText(LOCTEXT("Foliage_DropStatic_ToolTip", "Drag static meshes from the content browser into this area."))
						]
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(ItemScrollBox, SScrollBox)
				]
			]
		]

		// Static Mesh Drop Zone
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SFoliageDragDropHandler)
			.Visibility(this, &SFoliageEdit::GetVisibility_FoliageDropTarget)
			.FoliageEditPtr(SharedThis(this))
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFit)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Foliage_AddFoliageMesh", "+ Foliage Mesh"))
					.ShadowOffset(FVector2D(1.f, 1.f))
				]
			]
		]
	];

	RefreshFullList();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

SFoliageEdit::~SFoliageEdit()
{
	// Release all rendering resources being held onto
	AssetThumbnailPool->ReleaseResources();

	FEditorDelegates::NewCurrentLevel.RemoveAll(this);
}

void SFoliageEdit::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	AssetThumbnailPool->Tick(InDeltaTime);
}

TSharedPtr<FAssetThumbnail> SFoliageEdit::CreateThumbnail(UStaticMesh* InStaticMesh)
{
	return MakeShareable(new FAssetThumbnail(InStaticMesh, 80, 80, AssetThumbnailPool));
}

void SFoliageEdit::RefreshFullList()
{
	for (int DisplayItemIdx = 0; DisplayItemIdx < DisplayItemList.Num();)
	{
		RemoveItemFromScrollbox(DisplayItemList[DisplayItemIdx]);
	}

	TArray<struct FFoliageMeshUIInfo>& FoliageMeshList = FoliageEditMode->GetFoliageMeshList();

	for (int MeshIdx = 0; MeshIdx < FoliageMeshList.Num(); MeshIdx++)
	{
		AddItemToScrollbox(FoliageMeshList[MeshIdx]);
	}
}

void SFoliageEdit::AddItemToScrollbox(struct FFoliageMeshUIInfo& InFoliageInfoToAdd)
{
	UFoliageType* Settings = InFoliageInfoToAdd.Settings;

	TSharedRef<SFoliageEditMeshDisplayItem> DisplayItem =
		SNew(SFoliageEditMeshDisplayItem)
		.FoliageEditPtr(SharedThis(this))
		.FoliageSettingsPtr(Settings)
		.AssetThumbnail(CreateThumbnail(Settings->GetStaticMesh()))
		.FoliageMeshUIInfo(MakeShareable(new FFoliageMeshUIInfo(InFoliageInfoToAdd)));

	DisplayItemList.Add(DisplayItem);

	ItemScrollBox->AddSlot()
	[
		DisplayItem
	];
}

void SFoliageEdit::RemoveItemFromScrollbox(const TSharedPtr<SFoliageEditMeshDisplayItem> InWidgetToRemove)
{
	DisplayItemList.Remove(InWidgetToRemove.ToSharedRef());

	ItemScrollBox->RemoveSlot(InWidgetToRemove.ToSharedRef());
}

void SFoliageEdit::ReplaceItem(const TSharedPtr<SFoliageEditMeshDisplayItem> InDisplayItemToReplaceIn, UStaticMesh* InNewStaticMesh)
{
	TArray<FFoliageMeshUIInfo>& FoliageMeshList = FoliageEditMode->GetFoliageMeshList();

	for (FFoliageMeshUIInfo& UIInfo : FoliageMeshList)
	{
		UFoliageType* Settings = UIInfo.Settings;
		if (Settings->GetStaticMesh() == InNewStaticMesh)
		{
			// This is the info for the new static mesh. Update the display item.
			InDisplayItemToReplaceIn->Replace(Settings,
				CreateThumbnail(Settings->GetStaticMesh()),
				MakeShareable(new FFoliageMeshUIInfo(UIInfo)));
			break;
		}
	}
}

void SFoliageEdit::ClearAllToolSelection()
{
	FoliageEditMode->UISettings.SetLassoSelectToolSelected(false);
	FoliageEditMode->UISettings.SetPaintToolSelected(false);
	FoliageEditMode->UISettings.SetReapplyToolSelected(false);
	FoliageEditMode->UISettings.SetSelectToolSelected(false);
	FoliageEditMode->UISettings.SetPaintBucketToolSelected(false);
}

void SFoliageEdit::BindCommands()
{
	const FFoliageEditCommands& Commands = FFoliageEditCommands::Get();

	UICommandList->MapAction(
		Commands.SetPaint,
		FExecuteAction::CreateSP(this, &SFoliageEdit::OnSetPaint),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SFoliageEdit::IsPaintTool));

	UICommandList->MapAction(
		Commands.SetReapplySettings,
		FExecuteAction::CreateSP(this, &SFoliageEdit::OnSetReapplySettings),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SFoliageEdit::IsReapplySettingsTool));

	UICommandList->MapAction(
		Commands.SetSelect,
		FExecuteAction::CreateSP(this, &SFoliageEdit::OnSetSelectInstance),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SFoliageEdit::IsSelectTool));

	UICommandList->MapAction(
		Commands.SetLassoSelect,
		FExecuteAction::CreateSP(this, &SFoliageEdit::OnSetLasso),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SFoliageEdit::IsLassoSelectTool));

	UICommandList->MapAction(
		Commands.SetPaintBucket,
		FExecuteAction::CreateSP(this, &SFoliageEdit::OnSetPaintFill),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SFoliageEdit::IsPaintFillTool));
}

TSharedRef<SWidget> SFoliageEdit::BuildToolBar()
{
	FToolBarBuilder Toolbar(UICommandList, FMultiBoxCustomization::None);
	{
		Toolbar.AddToolBarButton(FFoliageEditCommands::Get().SetPaint);
		Toolbar.AddToolBarButton(FFoliageEditCommands::Get().SetReapplySettings);
		Toolbar.AddToolBarButton(FFoliageEditCommands::Get().SetSelect);
		Toolbar.AddToolBarButton(FFoliageEditCommands::Get().SetLassoSelect);
		Toolbar.AddToolBarButton(FFoliageEditCommands::Get().SetPaintBucket);
	}

	IIntroTutorials& IntroTutorials = FModuleManager::LoadModuleChecked<IIntroTutorials>(TEXT("IntroTutorials"));

	return
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.Padding(4, 0)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.HAlign(HAlign_Center)
				.Padding(0)
				.BorderImage(FEditorStyle::GetBrush("NoBorder"))
				.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
				[
					Toolbar.MakeWidget()
				]
			]

			// Tutorial link
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(4)
			[
				IntroTutorials.CreateTutorialsWidget(TEXT("FoliageMode"))
			]
		];
}

void SFoliageEdit::OnSetPaint()
{
	ClearAllToolSelection();

	FoliageEditMode->UISettings.SetPaintToolSelected(true);

	FoliageEditMode->NotifyToolChanged();
}

bool SFoliageEdit::IsPaintTool() const
{
	return FoliageEditMode->UISettings.GetPaintToolSelected();
}

void SFoliageEdit::OnSetReapplySettings()
{
	ClearAllToolSelection();

	FoliageEditMode->UISettings.SetReapplyToolSelected(true);

	FoliageEditMode->NotifyToolChanged();
}

bool SFoliageEdit::IsReapplySettingsTool() const
{
	return FoliageEditMode->UISettings.GetReapplyToolSelected();
}

void SFoliageEdit::OnSetSelectInstance()
{
	ClearAllToolSelection();

	FoliageEditMode->UISettings.SetSelectToolSelected(true);

	FoliageEditMode->NotifyToolChanged();
}

bool SFoliageEdit::IsSelectTool() const
{
	return FoliageEditMode->UISettings.GetSelectToolSelected();
}

void SFoliageEdit::OnSetLasso()
{
	ClearAllToolSelection();

	FoliageEditMode->UISettings.SetLassoSelectToolSelected(true);

	FoliageEditMode->NotifyToolChanged();
}

bool SFoliageEdit::IsLassoSelectTool() const
{
	return FoliageEditMode->UISettings.GetLassoSelectToolSelected();
}

void SFoliageEdit::OnSetPaintFill()
{
	ClearAllToolSelection();

	FoliageEditMode->UISettings.SetPaintBucketToolSelected(true);

	FoliageEditMode->NotifyToolChanged();
}

bool SFoliageEdit::IsPaintFillTool() const
{
	return FoliageEditMode->UISettings.GetPaintBucketToolSelected();
}

void SFoliageEdit::SetRadius(float InRadius)
{
	FoliageEditMode->UISettings.SetRadius(InRadius);
}

float SFoliageEdit::GetRadius() const
{
	return FoliageEditMode->UISettings.GetRadius();
}

void SFoliageEdit::SetPaintDensity(float InDensity)
{
	FoliageEditMode->UISettings.SetPaintDensity(InDensity);
}

float SFoliageEdit::GetPaintDensity() const
{
	return FoliageEditMode->UISettings.GetPaintDensity();
}


void SFoliageEdit::SetEraseDensity(float InDensity)
{
	FoliageEditMode->UISettings.SetUnpaintDensity(InDensity);
}

float SFoliageEdit::GetEraseDensity() const
{
	return FoliageEditMode->UISettings.GetUnpaintDensity();
}

EVisibility SFoliageEdit::GetVisibility_EmptyList() const
{
	if (DisplayItemList.Num() > 0)
	{
		return EVisibility::Collapsed;
	}

	return EVisibility::HitTestInvisible;
}

EVisibility SFoliageEdit::GetVisibility_FoliageDropTarget() const
{
	if ( FSlateApplication::Get().IsDragDropping() )
	{
		TArray<FAssetData> DraggedAssets = AssetUtil::ExtractAssetDataFromDrag(FSlateApplication::Get().GetDragDroppingContent());
		for ( const FAssetData& AssetData : DraggedAssets )
		{
			if ( AssetData.IsValid() && AssetData.GetClass()->IsChildOf(UStaticMesh::StaticClass()) )
			{
				return EVisibility::Visible;
			}
		}
	}
	
	return EVisibility::Hidden;
}

EVisibility SFoliageEdit::GetVisibility_NonEmptyList() const
{
	if (DisplayItemList.Num() == 0)
	{
		return EVisibility::Collapsed;
	}

	return EVisibility::HitTestInvisible;
}

bool SFoliageEdit::CanAddStaticMesh(const UStaticMesh* const InStaticMesh) const
{
	for (FFoliageMeshUIInfo& UIInfo : FoliageEditMode->GetFoliageMeshList())
	{
		UFoliageType* Settings = UIInfo.Settings;

		if (Settings->GetStaticMesh() == InStaticMesh)
		{
			return false;
		}
	}
	return true;
}

FReply SFoliageEdit::OnDrop_ListView(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TArray<FAssetData> DroppedAssetData = AssetUtil::ExtractAssetDataFromDrag(DragDropEvent);

	for (int32 AssetIdx = 0; AssetIdx < DroppedAssetData.Num(); ++AssetIdx)
	{
		GWarn->BeginSlowTask(LOCTEXT("OnDrop_LoadPackage", "Fully Loading Package For Drop"), true, false);
		UStaticMesh* StaticMesh = Cast<UStaticMesh>(DroppedAssetData[AssetIdx].GetAsset());
		GWarn->EndSlowTask();

		if (StaticMesh && CanAddStaticMesh(StaticMesh))
		{
			FoliageEditMode->AddFoliageMesh(StaticMesh);
			AddItemToScrollbox(FoliageEditMode->GetFoliageMeshList()[FoliageEditMode->GetFoliageMeshList().Num() - 1]);
		}

	}

	return FReply::Handled();
}

void SFoliageEdit::OnCheckStateChanged_Landscape(ECheckBoxState InState)
{
	FoliageEditMode->UISettings.SetFilterLandscape(InState == ECheckBoxState::Checked ? true : false);
}

ECheckBoxState SFoliageEdit::GetCheckState_Landscape() const
{
	return FoliageEditMode->UISettings.GetFilterLandscape() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEdit::OnCheckStateChanged_StaticMesh(ECheckBoxState InState)
{
	FoliageEditMode->UISettings.SetFilterStaticMesh(InState == ECheckBoxState::Checked ? true : false);
}

ECheckBoxState SFoliageEdit::GetCheckState_StaticMesh() const
{
	return FoliageEditMode->UISettings.GetFilterStaticMesh() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEdit::OnCheckStateChanged_BSP(ECheckBoxState InState)
{
	FoliageEditMode->UISettings.SetFilterBSP(InState == ECheckBoxState::Checked ? true : false);
}

ECheckBoxState SFoliageEdit::GetCheckState_BSP() const
{
	return FoliageEditMode->UISettings.GetFilterBSP() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEdit::OnCheckStateChanged_Translucent(ECheckBoxState InState)
{
	FoliageEditMode->UISettings.SetFilterTranslucent(InState == ECheckBoxState::Checked ? true : false);
}

ECheckBoxState SFoliageEdit::GetCheckState_Translucent() const
{
	return FoliageEditMode->UISettings.GetFilterTranslucent() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

EVisibility SFoliageEdit::GetVisibility_Radius() const
{
	if (FoliageEditMode->UISettings.GetSelectToolSelected() || FoliageEditMode->UISettings.GetReapplyPaintBucketToolSelected() || FoliageEditMode->UISettings.GetPaintBucketToolSelected() )
	{
		return EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

EVisibility SFoliageEdit::GetVisibility_PaintDensity() const
{
	if (FoliageEditMode->UISettings.GetSelectToolSelected() || FoliageEditMode->UISettings.GetReapplyPaintBucketToolSelected() || FoliageEditMode->UISettings.GetLassoSelectToolSelected())
	{
		return EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

EVisibility SFoliageEdit::GetVisibility_EraseDensity() const
{
	if (FoliageEditMode->UISettings.GetSelectToolSelected() || FoliageEditMode->UISettings.GetReapplyPaintBucketToolSelected() || FoliageEditMode->UISettings.GetLassoSelectToolSelected() || FoliageEditMode->UISettings.GetPaintBucketToolSelected())
	{
		return EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

EVisibility SFoliageEdit::GetVisibility_Filters() const
{
	if (FoliageEditMode->UISettings.GetSelectToolSelected())
	{
		return EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}


void SFoliageEdit::NotifyNewCurrentLevel()
{
	RefreshFullList();
}


#undef LOCTEXT_NAMESPACE
