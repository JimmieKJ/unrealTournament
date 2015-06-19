// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "WorldBrowserPrivatePCH.h"
#include "STiledLandscapeImportDlg.h"
#include "SVectorInputBox.h"
#include "IDesktopPlatform.h"
#include "DesktopPlatformModule.h"
#include "ContentBrowserModule.h"
#include "LandscapeProxy.h"
#include "SNumericEntryBox.h"
#include "PropertyCustomizationHelpers.h"

#define LOCTEXT_NAMESPACE "WorldBrowser"

static int32 CalcLandscapeSquareResolution(int32 ComponetsNumX, int32 SectionNumX, int32 SectionQuadsNumX)
{
	return ComponetsNumX*SectionNumX*SectionQuadsNumX+1;
}

/**
 *	Returns tile coordinates extracted from a specified tile filename
 */
static FIntPoint ExtractTileCoordinates(FString BaseFilename)
{
	//We expect file name in form: <tilename>_x<number>_y<number>
	FIntPoint ResultPosition(-1,-1);
	
	int32 XPos = BaseFilename.Find(TEXT("_x"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	int32 YPos = BaseFilename.Find(TEXT("_y"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	if (XPos != INDEX_NONE && YPos != INDEX_NONE && XPos < YPos)
	{
		FString XCoord = BaseFilename.Mid(XPos+2, YPos-(XPos+2));
		FString YCoord = BaseFilename.Mid(YPos+2, BaseFilename.Len()-(YPos+2));

		if (XCoord.IsNumeric() && YCoord.IsNumeric())
		{
			TTypeFromString<int32>::FromString(ResultPosition.X, *XCoord);
			TTypeFromString<int32>::FromString(ResultPosition.Y, *YCoord);
		}
	}

	return ResultPosition;
}

void STiledLandcapeImportDlg::Construct(const FArguments& InArgs, TSharedPtr<SWindow> InParentWindow)
{
	bShouldImport = false;
	ParentWindow = InParentWindow;
	
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.FillHeight(1)
			.Padding(0,10,0,10)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(2)
				
				// Select tiles
				+SUniformGridPanel::Slot(0, 0)

				+SUniformGridPanel::Slot(1, 0)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked(this, &STiledLandcapeImportDlg::OnClickedSelectHeightmapTiles)
					.Text(LOCTEXT("TiledLandscapeImport_SelectButtonText", "Select Heightmap Tiles..."))
				]
				
				// Tiles origin offset
				+SUniformGridPanel::Slot(0, 1)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.ToolTipText(LOCTEXT("TiledLandscapeImport_TilesOffsetTooltip", "For example: tile x0_y0 will be treated as x(0+offsetX)_y(0+offsetY)"))
					.Text(LOCTEXT("TiledLandscapeImport_TilesOffsetText", "Tile Coordinates Offset"))
				]

				+SUniformGridPanel::Slot(1, 1)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					
					+SHorizontalBox::Slot()
					.Padding(0.0f, 1.0f, 2.0f, 1.0f)
					.FillWidth(1.f)
					[
						SNew(SNumericEntryBox<int32>)
						.Value(this, &STiledLandcapeImportDlg::GetTileOffsetX)
						.OnValueChanged(this, &STiledLandcapeImportDlg::SetTileOffsetX)
						.LabelPadding(0)
						.Label()
						[
							SNumericEntryBox<int>::BuildLabel( LOCTEXT("X_Label", "X"), FLinearColor::White, SNumericEntryBox<int>::RedLabelBackgroundColor )
						]
					]
					
					+SHorizontalBox::Slot()
					.Padding(0.0f, 1.0f, 2.0f, 1.0f)
					.FillWidth(1.f)
					[
						SNew(SNumericEntryBox<int32>)
						.Value(this, &STiledLandcapeImportDlg::GetTileOffsetY)
						.OnValueChanged(this, &STiledLandcapeImportDlg::SetTileOffsetY)
						.LabelPadding(0)
						.Label()
						[
							SNumericEntryBox<float>::BuildLabel( LOCTEXT("Y_Label", "Y"), FLinearColor::White, SNumericEntryBox<int>::GreenLabelBackgroundColor )
						]
					]
				]

				// Tile configuration
				+SUniformGridPanel::Slot(0, 2)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TiledLandscapeImport_ConfigurationText", "Import Configuration"))
				]

				+SUniformGridPanel::Slot(1, 2)
				.VAlign(VAlign_Center)
				[
					SAssignNew(TileConfigurationComboBox, SComboBox<TSharedPtr<FTileImportConfiguration>>)
					.OptionsSource(&ActiveConfigurations)
					.OnSelectionChanged(this, &STiledLandcapeImportDlg::OnSetImportConfiguration)
					.OnGenerateWidget(this, &STiledLandcapeImportDlg::HandleTileConfigurationComboBoxGenarateWidget)
					.Content()
					[
						SNew(STextBlock)
						.Text(this, &STiledLandcapeImportDlg::GetTileConfigurationText)
					]
				]

				// Scale
				+SUniformGridPanel::Slot(0, 3)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TiledLandscapeImport_ScaleText", "Landscape Scale"))
				]
			
				+SUniformGridPanel::Slot(1, 3)
				.VAlign(VAlign_Center)
				[
					SNew( SVectorInputBox )
					.bColorAxisLabels( true )
					.X( this, &STiledLandcapeImportDlg::GetScaleX )
					.Y( this, &STiledLandcapeImportDlg::GetScaleY )
					.Z( this, &STiledLandcapeImportDlg::GetScaleZ )
					.OnXCommitted( this, &STiledLandcapeImportDlg::OnSetScale, 0 )
					.OnYCommitted( this, &STiledLandcapeImportDlg::OnSetScale, 1 )
					.OnZCommitted( this, &STiledLandcapeImportDlg::OnSetScale, 2 )
				]

				// Landcape material
				+SUniformGridPanel::Slot(0, 4)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TiledLandscapeImport_MaterialText", "Material"))
				]
			
				+SUniformGridPanel::Slot(1, 4)
				.VAlign(VAlign_Center)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UMaterialInterface::StaticClass())
					.ObjectPath(this, &STiledLandcapeImportDlg::GetLandscapeMaterialPath)
					.OnObjectChanged(this, &STiledLandcapeImportDlg::OnLandscapeMaterialChanged)
					.AllowClear(true)
				]
			]

			// Layers
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
			[
				SAssignNew(LayerDataListView, SListView<TSharedPtr<FTiledLandscapeImportSettings::LandscapeLayerSettings>>)
				.ListItemsSource( &LayerDataList )
				.OnGenerateRow( this, &STiledLandcapeImportDlg::OnGenerateWidgetForLayerDataListView )
				.SelectionMode(ESelectionMode::None)
			]

			// Import summary
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
			[
				SNew(STextBlock)
				.Text(this, &STiledLandcapeImportDlg::GetImportSummaryText)
				.WrapTextAt(600.0f)
			]

			// Import, Cancel
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(0,10,0,10)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+SUniformGridPanel::Slot(0,0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.IsEnabled(this, &STiledLandcapeImportDlg::IsImportEnabled)
					.OnClicked(this, &STiledLandcapeImportDlg::OnClickedImport)
					.Text(LOCTEXT("TiledLandscapeImport_ImportButtonText", "Import"))
				]
				+SUniformGridPanel::Slot(1,0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked(this, &STiledLandcapeImportDlg::OnClickedCancel)
					.Text(LOCTEXT("TiledLandscapeImport_CancelButtonText", "Cancel"))
				]
			]
		]
	];


	GenerateAllPossibleTileConfigurations();
	SetPossibleConfigurationsForFileSize(0);
}

TSharedRef<SWidget> STiledLandcapeImportDlg::HandleTileConfigurationComboBoxGenarateWidget(TSharedPtr<FTileImportConfiguration> InItem) const
{
	const FText ItemText = GenerateConfigurationText(InItem->NumComponents, InItem->NumSectionsPerComponent, InItem->NumQuadsPerSection);
		
	return SNew(SBox)
	.Padding(4)
	[
		SNew(STextBlock).Text(ItemText)
	];
}

FText STiledLandcapeImportDlg::GetTileConfigurationText() const
{
	if (ImportSettings.HeightmapFileList.Num() == 0)
	{
		return LOCTEXT("TiledLandscapeImport_NoTilesText", "No tiles selected");
	}

	if (ImportSettings.SectionsPerComponent <= 0)
	{
		return LOCTEXT("TiledLandscapeImport_InvalidTileResolutionText", "Selected tiles have unsupported resolution");
	}
	
	return GenerateConfigurationText(ImportSettings.ComponentsNum, ImportSettings.SectionsPerComponent, ImportSettings.QuadsPerSection);
}

TSharedRef<ITableRow> STiledLandcapeImportDlg::OnGenerateWidgetForLayerDataListView(
	TSharedPtr<FTiledLandscapeImportSettings::LandscapeLayerSettings> InLayerData, 
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return 	SNew(STableRow<TSharedRef<FTiledLandscapeImportSettings::LandscapeLayerSettings>>, OwnerTable)
			[
				SNew(SBorder)
				[
					SNew(SHorizontalBox)

					// Layer name
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					.FillWidth(1)
					[
						SNew(STextBlock).Text(FText::FromName(InLayerData->Name))
					]

					// Blend option
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.Padding(2)
					.AutoWidth()
					[
						SNew(SCheckBox)
						.IsChecked(this, &STiledLandcapeImportDlg::GetLayerBlendState, InLayerData)
						.OnCheckStateChanged(this, &STiledLandcapeImportDlg::OnLayerBlendStateChanged, InLayerData)
						.ToolTipText(LOCTEXT("TiledLandscapeImport_BlendOption", "Weight-Blended Layer"))
					]
					
					// Number of selected weightmaps
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Right)
					.Padding(2)
					.AutoWidth()
					[
						SNew(STextBlock).Text(this, &STiledLandcapeImportDlg::GetWeightmapCountText, InLayerData)
					]
					
					// Button for selecting weightmap files
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Right)
					.AutoWidth()
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
						.OnClicked(this, &STiledLandcapeImportDlg::OnClickedSelectWeighmapTiles, InLayerData)
						.Text(LOCTEXT("TiledLandscapeImport_SelectWeightmapButtonText", "Select Weightmap Tiles..."))
					]
				]
			];
	
}

bool STiledLandcapeImportDlg::ShouldImport() const
{
	return bShouldImport;
}

const FTiledLandscapeImportSettings& STiledLandcapeImportDlg::GetImportSettings() const
{
	return ImportSettings;
}

TOptional<float> STiledLandcapeImportDlg::GetScaleX() const
{
	return ImportSettings.Scale3D.X;
}

TOptional<float> STiledLandcapeImportDlg::GetScaleY() const
{
	return ImportSettings.Scale3D.Y;
}

TOptional<float> STiledLandcapeImportDlg::GetScaleZ() const
{
	return ImportSettings.Scale3D.Z;
}

void STiledLandcapeImportDlg::OnSetScale(float InValue, ETextCommit::Type CommitType, int32 InAxis)
{
	if (InAxis < 2) //XY uniform
	{
		ImportSettings.Scale3D.X = FMath::Abs(InValue);
		ImportSettings.Scale3D.Y = FMath::Abs(InValue);
	}
	else //Z
	{
		ImportSettings.Scale3D.Z = FMath::Abs(InValue);
	}
}

TOptional<int32> STiledLandcapeImportDlg::GetTileOffsetX() const
{
	return ImportSettings.TilesCoordinatesOffset.X;
}

void STiledLandcapeImportDlg::SetTileOffsetX(int32 InValue)
{
	ImportSettings.TilesCoordinatesOffset.X = InValue;
}

TOptional<int32> STiledLandcapeImportDlg::GetTileOffsetY() const
{
	return ImportSettings.TilesCoordinatesOffset.Y;
}

void STiledLandcapeImportDlg::SetTileOffsetY(int32 InValue)
{
	ImportSettings.TilesCoordinatesOffset.Y = InValue;
}

void STiledLandcapeImportDlg::OnSetImportConfiguration(TSharedPtr<FTileImportConfiguration> InTileConfig, ESelectInfo::Type SelectInfo)
{
	if (InTileConfig.IsValid())
	{
		ImportSettings.ComponentsNum = InTileConfig->NumComponents;
		ImportSettings.QuadsPerSection = InTileConfig->NumQuadsPerSection;
		ImportSettings.SectionsPerComponent = InTileConfig->NumSectionsPerComponent;
		ImportSettings.SizeX = InTileConfig->SizeX;
	}
	else
	{
		ImportSettings.ComponentsNum = 0;
		ImportSettings.HeightmapFileList.Empty();
	}
}

FReply STiledLandcapeImportDlg::OnClickedSelectHeightmapTiles()
{
	TotalLandscapeRect = FIntRect(MAX_int32, MAX_int32, MIN_int32, MIN_int32);
	ImportSettings.HeightmapFileList.Empty();
	ImportSettings.TileCoordinates.Empty();
	
	SetPossibleConfigurationsForFileSize(0);
	
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		if (ParentWindow->GetNativeWindow().IsValid())
		{
			void* ParentWindowWindowHandle = ParentWindow->GetNativeWindow()->GetOSWindowHandle();

			const FString ImportFileTypes = TEXT("Heightmap tiles (*.r16)|*.r16");
			bool bOpened = DesktopPlatform->OpenFileDialog(
								ParentWindowWindowHandle,
								LOCTEXT("SelectHeightmapTiles", "Select heightmap tiles").ToString(),
								*FEditorDirectories::Get().GetLastDirectory(ELastDirectory::UNR),
								TEXT(""),
								*ImportFileTypes,
								EFileDialogFlags::Multiple,
								ImportSettings.HeightmapFileList);

			if (bOpened	&& ImportSettings.HeightmapFileList.Num())
			{
				IFileManager& FileManager = IFileManager::Get();
				bool bValidTiles = true;

				// All heightmap tiles have to be the same size and have correct tile position encoded into filename
				const int64 TargetFileSize = FileManager.FileSize(*ImportSettings.HeightmapFileList[0]);
				for (const FString& Filename : ImportSettings.HeightmapFileList)
				{
					int64 FileSize = FileManager.FileSize(*Filename);
					
					if ((FileSize & 1) != 0) // we expect 2 bytes per sample
					{
						FFormatNamedArguments Arguments;
						Arguments.Add(TEXT("FileName"), FText::FromString(Filename));
						StatusMessage = FText::Format(LOCTEXT("TiledLandscapeImport_HeightmapTileInvalidFormat", "File ({FileName}) should have raw data with 2 bytes per sample."), Arguments);
						bValidTiles = false;
					}
					
					if (FileSize != TargetFileSize)
					{
						FFormatNamedArguments Arguments;
						Arguments.Add(TEXT("FileName"), FText::FromString(Filename));
						Arguments.Add(TEXT("FileSize"), FileSize);
						Arguments.Add(TEXT("TargetFileSize"), TargetFileSize);
						StatusMessage = FText::Format(LOCTEXT("TiledLandscapeImport_HeightmapTileSizeMismatch", "File ({FileName}) size ({FileSize}) should match other tiles file size ({TargetFileSize})."), Arguments);
						bValidTiles = false;
						break;
					}

					FIntPoint TileCoordinate = ExtractTileCoordinates(FPaths::GetBaseFilename(Filename));
					if (TileCoordinate.GetMin() < 0)
					{
						FFormatNamedArguments Arguments;
						Arguments.Add(TEXT("FileName"), FText::FromString(Filename));
						StatusMessage = FText::Format(LOCTEXT("TiledLandscapeImport_HeightmapTileInvalidName", "File name ({FileName}) should match pattern: <name>_X<number>_Y<number>."), Arguments);
						bValidTiles = false;
						break;
					}

					TotalLandscapeRect.Include(TileCoordinate);
					ImportSettings.TileCoordinates.Add(TileCoordinate);
				}
				
				if (bValidTiles)
				{
					if (SetPossibleConfigurationsForFileSize(TargetFileSize) < 1)
					{
						int64 NumSamples = TargetFileSize/2; // 2 bytes per sample
						FFormatNamedArguments Arguments;
						Arguments.Add(TEXT("NumSamples"), NumSamples);
						StatusMessage = FText::Format(LOCTEXT("TiledLandscapeImport_HeightmapTileInvalidSize", "No landscape configuration found for ({NumSamples}) samples."), Arguments);
						bValidTiles = false;
					}
				}
			}
		}
	}
		
	return FReply::Handled();
}

FReply STiledLandcapeImportDlg::OnClickedSelectWeighmapTiles(TSharedPtr<FTiledLandscapeImportSettings::LandscapeLayerSettings> InLayerData)
{
	InLayerData->WeightmapFiles.Empty();

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		if (ParentWindow->GetNativeWindow().IsValid())
		{
			void* ParentWindowWindowHandle = ParentWindow->GetNativeWindow()->GetOSWindowHandle();

			TArray<FString> WeightmapFilesList;

			const FString ImportFileTypes = TEXT("Weightmap tiles (*.raw, *.png)|*.raw;*.png");
			bool bOpened = DesktopPlatform->OpenFileDialog(
								ParentWindowWindowHandle,
								LOCTEXT("SelectHeightmapTiles", "Select weightmap tiles").ToString(),
								*FEditorDirectories::Get().GetLastDirectory(ELastDirectory::UNR),
								TEXT(""),
								*ImportFileTypes,
								EFileDialogFlags::Multiple,
								WeightmapFilesList);

			if (bOpened	&& WeightmapFilesList.Num())
			{
				for (FString WeighmapFile : WeightmapFilesList)
				{
					FIntPoint TileCoordinate = ExtractTileCoordinates(FPaths::GetBaseFilename(WeighmapFile));
					InLayerData->WeightmapFiles.Add(TileCoordinate, WeighmapFile);
				}
							
				// TODO: check if it's a valid weightmaps
			}
		}
	}
	
	return FReply::Handled();
}

bool STiledLandcapeImportDlg::IsImportEnabled() const
{
	return ImportSettings.HeightmapFileList.Num() && ImportSettings.ComponentsNum > 0;
}

FReply STiledLandcapeImportDlg::OnClickedImport()
{
	// copy weightmaps list data to an import structure  
	ImportSettings.LandscapeLayerSettingsList.Empty();

	for (int32 LayerIdx = 0; LayerIdx < LayerDataList.Num(); ++LayerIdx)
	{
		ImportSettings.LandscapeLayerSettingsList.Add(*(LayerDataList[LayerIdx].Get()));
	}
					
	ParentWindow->RequestDestroyWindow();
	bShouldImport = true;	
	return FReply::Handled();	
}

FReply STiledLandcapeImportDlg::OnClickedCancel()
{
	ParentWindow->RequestDestroyWindow();
	bShouldImport = false;	

	return FReply::Handled();	
}

FString STiledLandcapeImportDlg::GetLandscapeMaterialPath() const
{
	return ImportSettings.LandscapeMaterial.IsValid() ? ImportSettings.LandscapeMaterial->GetPathName() : FString("");
}

void STiledLandcapeImportDlg::OnLandscapeMaterialChanged(const FAssetData& AssetData)
{
	ImportSettings.LandscapeMaterial = Cast<UMaterialInterface>(AssetData.GetAsset());

	// pull landscape layers from a chosen material
	UpdateLandscapeLayerList();
}

int32 STiledLandcapeImportDlg::SetPossibleConfigurationsForFileSize(int64 TargetFileSize)
{
	int32 Idx = AllConfigurations.IndexOfByPredicate([&](const FTileImportConfiguration& A){
		return TargetFileSize == A.ImportFileSize;
	});

	ActiveConfigurations.Empty();
	ImportSettings.ComponentsNum = 0; // Set invalid options
	StatusMessage = FText();

	// AllConfigurations - sorted by resolution
	while(AllConfigurations.IsValidIndex(Idx) && AllConfigurations[Idx].ImportFileSize == TargetFileSize)
	{
		TSharedPtr<FTileImportConfiguration> TileConfig = MakeShareable(new FTileImportConfiguration(AllConfigurations[Idx++]));
		ActiveConfigurations.Add(TileConfig);
	}

	// Refresh combo box with active configurations
	TileConfigurationComboBox->RefreshOptions();
	// Set first configurationion as active
	if (ActiveConfigurations.Num())
	{
		TileConfigurationComboBox->SetSelectedItem(ActiveConfigurations[0]);
	}

	return ActiveConfigurations.Num();
}

void STiledLandcapeImportDlg::GenerateAllPossibleTileConfigurations()
{
	AllConfigurations.Empty();
	for (int32 ComponentsNum = 1; ComponentsNum <= 32; ComponentsNum++)
	{
		for (int32 SectionsPerComponent = 1; SectionsPerComponent <= 2; SectionsPerComponent++)
		{
			for (int32 QuadsPerSection = 3; QuadsPerSection <= 8; QuadsPerSection++)
			{
				FTileImportConfiguration Entry;
				Entry.NumComponents				= ComponentsNum;
				Entry.NumSectionsPerComponent	= SectionsPerComponent;
				Entry.NumQuadsPerSection		= (1 << QuadsPerSection) - 1;
				Entry.SizeX = CalcLandscapeSquareResolution(Entry.NumComponents, Entry.NumSectionsPerComponent, Entry.NumQuadsPerSection);
				// Calculate expected file size for this setup
				Entry.ImportFileSize = FMath::Square(Entry.SizeX)*2; // 2 bytes per sample 

				AllConfigurations.Add(Entry);
			}
		}
	}
	
	// Sort by resolution
	AllConfigurations.Sort([](const FTileImportConfiguration& A, const FTileImportConfiguration& B){
		if (A.SizeX == B.SizeX)
		{
			return A.NumComponents < B.NumComponents;
		}
		return A.SizeX < B.SizeX;
	});
}

FText STiledLandcapeImportDlg::GetImportSummaryText() const
{
	if (ImportSettings.HeightmapFileList.Num() && ImportSettings.ComponentsNum > 0)
	{
		// Tile information(num, resolution)
		const FString TilesSummary = FString::Printf(TEXT("%d - %dx%d"), ImportSettings.HeightmapFileList.Num(), ImportSettings.SizeX, ImportSettings.SizeX);
	
		// Total landscape size(NxN km)
		int32 WidthInTilesX = TotalLandscapeRect.Width() + 1;
		int32 WidthInTilesY = TotalLandscapeRect.Height() + 1;
		float WidthX = 0.00001f*ImportSettings.Scale3D.X*WidthInTilesX*ImportSettings.SizeX;
		float WidthY = 0.00001f*ImportSettings.Scale3D.Y*WidthInTilesY*ImportSettings.SizeX;
		const FString LandscapeSummary = FString::Printf(TEXT("%.3fx%.3f"), WidthX, WidthY);
	
		StatusMessage = FText::Format(
			LOCTEXT("TiledLandscapeImport_SummaryText", "{0} tiles, {1}km landscape"), 
			FText::FromString(TilesSummary),
			FText::FromString(LandscapeSummary)
			);
	}
	
	return StatusMessage;
}

FText STiledLandcapeImportDlg::GetWeightmapCountText(TSharedPtr<FTiledLandscapeImportSettings::LandscapeLayerSettings> InLayerData) const
{
	int32 NumWeighmaps = InLayerData.IsValid() ? InLayerData->WeightmapFiles.Num() : 0;
	return FText::AsNumber(NumWeighmaps);
}

ECheckBoxState STiledLandcapeImportDlg::GetLayerBlendState(TSharedPtr<FTiledLandscapeImportSettings::LandscapeLayerSettings> InLayerData) const
{
	return (InLayerData->bNoBlendWeight ? ECheckBoxState::Unchecked : ECheckBoxState::Checked);
}

void STiledLandcapeImportDlg::OnLayerBlendStateChanged(ECheckBoxState NewState, TSharedPtr<FTiledLandscapeImportSettings::LandscapeLayerSettings> InLayerData)
{
	InLayerData->bNoBlendWeight = !(NewState == ECheckBoxState::Checked);
}

FText STiledLandcapeImportDlg::GenerateConfigurationText(int32 NumComponents, int32 NumSectionsPerComponent, int32 NumQuadsPerSection) const
{
	const FString ComponentsStr = FString::Printf(TEXT("%dx%d"), NumComponents, NumComponents);
	const FString SectionsStr = FString::Printf(TEXT("%dx%d"), NumSectionsPerComponent, NumSectionsPerComponent);
	const FString QuadsStr = FString::Printf(TEXT("%dx%d"), NumQuadsPerSection, NumQuadsPerSection);
	
	return FText::Format(
		LOCTEXT("TiledLandscapeImport_ConfigurationText", "Components: {0} Sections: {1} Quads: {2}"), 
		FText::FromString(ComponentsStr),
		FText::FromString(SectionsStr),
		FText::FromString(QuadsStr)
		);
}

void STiledLandcapeImportDlg::UpdateLandscapeLayerList()
{
	TArray<FName> LayerNamesList = ALandscapeProxy::GetLayersFromMaterial(ImportSettings.LandscapeMaterial.Get());
	LayerDataList.Empty();
	for (FName LayerName : LayerNamesList)
	{
		// List view data source
		TSharedPtr<FTiledLandscapeImportSettings::LandscapeLayerSettings> LayerData = MakeShareable(new FTiledLandscapeImportSettings::LandscapeLayerSettings());
		LayerData->Name = LayerName;
		LayerDataList.Add(LayerData);
	}

	LayerDataListView->RequestListRefresh();
}

#undef LOCTEXT_NAMESPACE
