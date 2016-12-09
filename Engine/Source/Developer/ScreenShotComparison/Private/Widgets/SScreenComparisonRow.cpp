// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SScreenComparisonRow.h"
#include "Models/ScreenComparisonModel.h"
#include "Modules/ModuleManager.h"
#include "ISourceControlModule.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "ISourceControlProvider.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Input/SButton.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Interfaces/IImageWrapperModule.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "SScreenShotBrowser"

void SScreenComparisonRow::Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
{
	Comparisons = InArgs._Comparisons;
	ScreenshotManager = InArgs._ScreenshotManager;
	ComparisonDirectory = InArgs._ComparisonDirectory;
	Model = InArgs._ComparisonResult;

	CachedActualImageSize = FIntPoint::NoneValue;

	switch ( Model->GetType() )
	{
		case EComparisonResultType::Added:
		{
			FString AddedFolder = ComparisonDirectory / Comparisons->IncomingPath / Model->Folder;
			IFileManager::Get().FindFilesRecursive(ExternalFiles, *AddedFolder, TEXT("*.*"), true, false);
			break;
		}
		case EComparisonResultType::Missing:
		{
			FString AddedFolder = ComparisonDirectory / Comparisons->ApprovedPath / Model->Folder;
			IFileManager::Get().FindFilesRecursive(ExternalFiles, *AddedFolder, TEXT("*.*"), true, false);
			break;
		}
		case EComparisonResultType::Compared:
		{
			const FImageComparisonResult& ComparisonResult = Model->ComparisonResult.GetValue();
			FString IncomingImage = ComparisonDirectory / Comparisons->IncomingPath / ComparisonResult.IncomingFile;
			FString IncomingMetadata = FPaths::ChangeExtension(IncomingImage, TEXT("json"));

			ExternalFiles.Add(IncomingImage);
			ExternalFiles.Add(IncomingMetadata);
			break;
		}
	}

	SMultiColumnTableRow<TSharedPtr<FScreenComparisonModel>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SScreenComparisonRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if ( ColumnName == "Name" )
	{
		return SNew(STextBlock).Text(LOCTEXT("Unknown", "Test Name?  Driver?"));
	}
	else if ( ColumnName == "Delta" )
	{
		if ( Model->ComparisonResult.IsSet() )
		{
			FNumberFormattingOptions Format;
			Format.MinimumFractionalDigits = 2;
			Format.MaximumFractionalDigits = 2;
			const FText GlobalDelta = FText::AsPercent(Model->ComparisonResult->GlobalDifference, &Format);
			const FText LocalDelta = FText::AsPercent(Model->ComparisonResult->MaxLocalDifference, &Format);

			const FText Differences = FText::Format(LOCTEXT("LocalvGlobalDelta", "{0} | {1}"), LocalDelta, GlobalDelta);
			return SNew(STextBlock).Text(Differences);
		}

		return SNew(STextBlock).Text(FText::AsPercent(1.0));
	}
	else if ( ColumnName == "Preview" )
	{
		switch ( Model->GetType() )
		{
			case EComparisonResultType::Added:
			{
				return BuildAddedView();
			}
			case EComparisonResultType::Missing:
			{
				return BuildMissingView();
			}
			case EComparisonResultType::Compared:
			{
				return
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						BuildComparisonPreview()
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(SButton)
						.IsEnabled(this, &SScreenComparisonRow::CanUseSourceControl)
						.Text(LOCTEXT("ApproveNew", "Approve New!"))
						.OnClicked(this, &SScreenComparisonRow::ReplaceOld)
					];
			}
		}
	}

	return SNullWidget::NullWidget;
}

bool SScreenComparisonRow::CanUseSourceControl() const
{
	return ISourceControlModule::Get().IsEnabled();
}

TSharedRef<SWidget> SScreenComparisonRow::BuildMissingView()
{
	return
		SNew(SHorizontalBox)
		
		+SHorizontalBox::Slot()
		[
			SNew(SButton)
			.IsEnabled(this, &SScreenComparisonRow::CanUseSourceControl)
			.Text(LOCTEXT("RemoveOld", "Remove Old"))
			.OnClicked(this, &SScreenComparisonRow::RemoveOld)
		];
}

TSharedRef<SWidget> SScreenComparisonRow::BuildAddedView()
{
	return
		SNew(SHorizontalBox)
		
		+SHorizontalBox::Slot()
		[
			SNew(SButton)
			.IsEnabled(this, &SScreenComparisonRow::CanUseSourceControl)
			.Text(LOCTEXT("AddComparison", "Add New!"))
			.OnClicked(this, &SScreenComparisonRow::AddNew)
		];
}

TSharedRef<SWidget> SScreenComparisonRow::BuildComparisonPreview()
{
	const FImageComparisonResult& ComparisonResult = Model->ComparisonResult.GetValue();

	FString ApprovedFile = ComparisonDirectory / Comparisons->ApprovedPath / ComparisonResult.ApprovedFile;
	FString IncomingFile = ComparisonDirectory / Comparisons->IncomingPath / ComparisonResult.IncomingFile;
	FString DeltaFile = ComparisonDirectory / Comparisons->DeltaPath / ComparisonResult.ComparisonFile;

	ApprovedBrush = LoadScreenshot(ApprovedFile);
	UnapprovedBrush = LoadScreenshot(IncomingFile);
	ComparisonBrush = LoadScreenshot(DeltaFile);

	// Create the screen shot data widget.
	return 
		SNew(SBox)
		.MaxDesiredHeight(100)
		.HAlign(HAlign_Left)
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			[
				SNew(SHorizontalBox)
		
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.0f, 4.0f)
				[
					SNew(SImage)
					.Image(ApprovedBrush.Get())
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.0f, 4.0f)
				[
					SNew(SImage)
					.Image(ComparisonBrush.Get())
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.0f, 4.0f)
				[
					SNew(SImage)
					.Image(UnapprovedBrush.Get())
				]
			]
		];
}

void SScreenComparisonRow::GetStatus()
{
	//if ( SourceControlStates.Num() == 0 )
	//{
	//	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	//	SourceControlProvider.GetState(SourceControlFiles, SourceControlStates, EStateCacheUsage::Use);
	//}
}

FReply SScreenComparisonRow::AddNew()
{
	// Copy files to the approved
	const FString& LocalApprovedFolder = ScreenshotManager->GetLocalApprovedFolder();
	const FString ImportIncomingRoot = ComparisonDirectory / Comparisons->IncomingPath / TEXT("");

	TArray<FString> SourceControlFiles;

	for ( const FString& File : ExternalFiles )
	{
		FString RelativeFile = File;
		FPaths::MakePathRelativeTo(RelativeFile, *ImportIncomingRoot);
		
		FString DestFilePath = LocalApprovedFolder / RelativeFile;
		IFileManager::Get().Copy(*DestFilePath, *File, true, true);

		SourceControlFiles.Add(DestFilePath);
	}

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	if ( SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), SourceControlFiles) == ECommandResult::Failed )
	{
		//TODO Error
	}

	// Invalidate Status

	return FReply::Handled();
}

FReply SScreenComparisonRow::RemoveOld()
{
	// Copy files to the approved
	const FString& LocalApprovedFolder = ScreenshotManager->GetLocalApprovedFolder();
	const FString ImportApprovedRoot = ComparisonDirectory / Comparisons->ApprovedPath / TEXT("");

	TArray<FString> SourceControlFiles;

	for ( const FString& File : ExternalFiles )
	{
		FString RelativeFile = ExternalFiles[0];
		FPaths::MakePathRelativeTo(RelativeFile, *ImportApprovedRoot);

		FString DestFilePath = LocalApprovedFolder / RelativeFile;
		SourceControlFiles.Add(DestFilePath);
	}

	if ( ExternalFiles.Num() > 0 )
	{
		FString RelativeFile = ExternalFiles[0];
		FPaths::MakePathRelativeTo(RelativeFile, *ImportApprovedRoot);

		FString TestFolder = FPaths::GetPath(FPaths::GetPath(RelativeFile));

		//FString ParentDir = SelectedPaths[0] / TEXT("..");
		//FPaths::CollapseRelativeDirectories(ParentDir);

		FString MissingLocalTest = LocalApprovedFolder / TestFolder;

		IFileManager::Get().DeleteDirectory(*MissingLocalTest, false, true);
	}

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	if ( SourceControlProvider.Execute(ISourceControlOperation::Create<FDelete>(), SourceControlFiles) == ECommandResult::Failed )
	{
		//TODO Error
	}

	// Invalidate Status

	return FReply::Handled();
}

FReply SScreenComparisonRow::ReplaceOld()
{
	// Copy files to the approved
	const FString& LocalApprovedFolder = ScreenshotManager->GetLocalApprovedFolder();
	const FString ImportIncomingRoot = ComparisonDirectory / Comparisons->IncomingPath / TEXT("");

	TArray<FString> SourceControlFiles;

	for ( const FString& File : ExternalFiles )
	{
		FString RelativeFile = File;
		FPaths::MakePathRelativeTo(RelativeFile, *ImportIncomingRoot);

		FString DestFilePath = LocalApprovedFolder / RelativeFile;
		IFileManager::Get().Copy(*DestFilePath, *File, true, true);

		SourceControlFiles.Add(DestFilePath);
	}

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	if ( SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), SourceControlFiles) == ECommandResult::Failed )
	{
		//TODO Error
	}

	return FReply::Handled();
}

//TSharedRef<SWidget> SScreenComparisonRow::BuildSourceControl()
//{
//	if ( ISourceControlModule::Get().IsEnabled() )
//	{
//		// note: calling QueueStatusUpdate often does not spam status updates as an internal timer prevents this
//		ISourceControlModule::Get().QueueStatusUpdate(CachedConfigFileName);
//
//		ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
//	}
//}

TSharedPtr<FSlateDynamicImageBrush> SScreenComparisonRow::LoadScreenshot(FString ImagePath)
{
	TArray<uint8> RawFileData;
	if ( FFileHelper::LoadFileToArray(RawFileData, *ImagePath) )
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
		IImageWrapperPtr ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

		if ( ImageWrapper.IsValid() && ImageWrapper->SetCompressed(RawFileData.GetData(), RawFileData.Num()) )
		{
			const TArray<uint8>* RawData = NULL;
			if ( ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData) )
			{
				if ( FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(*ImagePath, ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), *RawData) )
				{
					return MakeShareable(new FSlateDynamicImageBrush(*ImagePath, FVector2D(ImageWrapper->GetWidth(), ImageWrapper->GetHeight())));
				}
			}
		}
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
