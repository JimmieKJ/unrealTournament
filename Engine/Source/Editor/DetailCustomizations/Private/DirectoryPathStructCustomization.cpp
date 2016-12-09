// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DirectoryPathStructCustomization.h"
#include "Misc/PackageName.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "EditorDirectories.h"
#include "DetailWidgetRow.h"
#include "DesktopPlatformModule.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#define LOCTEXT_NAMESPACE "DirectoryPathStructCustomization"

TSharedRef<IPropertyTypeCustomization> FDirectoryPathStructCustomization::MakeInstance()
{
	return MakeShareable(new FDirectoryPathStructCustomization());
}

void FDirectoryPathStructCustomization::CustomizeHeader( TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	TSharedPtr<IPropertyHandle> PathProperty = StructPropertyHandle->GetChildHandle("Path");

	const bool bRelativeToGameContentDir = StructPropertyHandle->HasMetaData( TEXT("RelativeToGameContentDir") );
	const bool bUseRelativePath = StructPropertyHandle->HasMetaData( TEXT("RelativePath") );
	const bool bLongPackageName = StructPropertyHandle->HasMetaData( TEXT("LongPackageName") );
	const bool bContentDir = StructPropertyHandle->HasMetaData( TEXT("ContentDir") );

	AbsoluteGameContentDir = FPaths::ConvertRelativePathToFull(FPaths::GameContentDir());

	if(PathProperty.IsValid())
	{
		TSharedPtr<SWidget> PickerWidget = nullptr;

		if(bContentDir)
		{
			FPathPickerConfig PathPickerConfig;
			PathPickerConfig.bAllowContextMenu = false;
			PathPickerConfig.OnPathSelected = FOnPathSelected::CreateSP(this, &FDirectoryPathStructCustomization::OnPathPicked, PathProperty.ToSharedRef());

			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

			PickerWidget = SAssignNew(PickerButton, SComboButton)
			.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
			.ToolTipText( LOCTEXT( "FolderComboToolTipText", "Choose a content directory") )
			.ContentPadding( 2.0f )
			.ForegroundColor( FSlateColor::UseForeground() )
			.IsFocusable( false )
			.MenuContent()
			[
				SNew(SBox)
				.WidthOverride(300.0f)
				.HeightOverride(300.0f)
				[
					ContentBrowserModule.Get().CreatePathPicker(PathPickerConfig)
				]
			];			
		}
		else
		{
			PickerWidget = SAssignNew(BrowseButton, SButton)
			.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
			.ToolTipText( LOCTEXT( "FolderButtonToolTipText", "Choose a directory from this computer") )
			.OnClicked( FOnClicked::CreateSP(this, &FDirectoryPathStructCustomization::OnPickDirectory, PathProperty.ToSharedRef(), bRelativeToGameContentDir, bUseRelativePath,bLongPackageName) )
			.ContentPadding( 2.0f )
			.ForegroundColor( FSlateColor::UseForeground() )
			.IsFocusable( false )
			[
				SNew( SImage )
				.Image( FEditorStyle::GetBrush("PropertyWindow.Button_Ellipsis") )
				.ColorAndOpacity( FSlateColor::UseForeground() )
			];
		}

		HeaderRow.ValueContent()
		.MinDesiredWidth(125.0f)
		.MaxDesiredWidth(600.0f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				PathProperty->CreatePropertyValueWidget()
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.VAlign(VAlign_Center)
			[
				PickerWidget.ToSharedRef()
			]
		]
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		];
	}
}

void FDirectoryPathStructCustomization::CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
}

FReply FDirectoryPathStructCustomization::OnPickDirectory(TSharedRef<IPropertyHandle> PropertyHandle, const bool bRelativeToGameContentDir, const bool bUseRelativePath, const bool bLongPackageName) const
{
	FString Directory;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{

		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(BrowseButton.ToSharedRef());
		void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid()) ? ParentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;

		FString StartDirectory = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_IMPORT);
		if (bRelativeToGameContentDir && !IsValidPath(StartDirectory, bRelativeToGameContentDir))
		{
			StartDirectory = AbsoluteGameContentDir;
		}

		// Loop until; a) the user cancels (OpenDirectoryDialog returns false), or, b) the chosen path is valid (IsValidPath returns true)
		for (;;)
		{
			if (DesktopPlatform->OpenDirectoryDialog(ParentWindowHandle, LOCTEXT("FolderDialogTitle", "Choose a directory").ToString(), StartDirectory, Directory))
			{
				FText FailureReason;
				if (IsValidPath(Directory, bRelativeToGameContentDir, &FailureReason))
				{
					FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_IMPORT, Directory);

					if (bLongPackageName)
					{
						FString LongPackageName;
						FString StringFailureReason;
						if (FPackageName::TryConvertFilenameToLongPackageName(Directory, LongPackageName, &StringFailureReason) == false)
						{
							StartDirectory = Directory;
							FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(StringFailureReason));
							continue;
						}
						Directory = LongPackageName;
					}

					if (bRelativeToGameContentDir)
					{
						Directory = Directory.RightChop(AbsoluteGameContentDir.Len());
					}

					if (bUseRelativePath)
					{
						Directory = IFileManager::Get().ConvertToRelativePath(*Directory);
					}

					PropertyHandle->SetValue(Directory);
				}
				else
				{
					StartDirectory = Directory;
					FMessageDialog::Open(EAppMsgType::Ok, FailureReason);
					continue;
				}
			}
			break;
		}
	}

	return FReply::Handled();
}

bool FDirectoryPathStructCustomization::IsValidPath(const FString& AbsolutePath, const bool bRelativeToGameContentDir, FText* const OutReason) const
{
	if(bRelativeToGameContentDir)
	{
		if(!AbsolutePath.StartsWith(AbsoluteGameContentDir))
		{
			if(OutReason)
			{
				*OutReason = FText::Format(LOCTEXT("Error_InvalidRootPath", "The chosen directory must be within {0}"), FText::FromString(AbsoluteGameContentDir));
			}
			return false;
		}
	}

	return true;
}

void FDirectoryPathStructCustomization::OnPathPicked(const FString& Path, TSharedRef<IPropertyHandle> PropertyHandle)
{
	PickerButton->SetIsOpen(false);

	PropertyHandle->SetValue(Path);
}

#undef LOCTEXT_NAMESPACE
