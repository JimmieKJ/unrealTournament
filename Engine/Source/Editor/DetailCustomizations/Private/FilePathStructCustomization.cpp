// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "FilePathStructCustomization.h"
#include "SFilePathPicker.h"


#define LOCTEXT_NAMESPACE "FilePathStructCustomization"


/* IDetailCustomization interface
 *****************************************************************************/

void FFilePathStructCustomization::CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	/* do nothing */
}


void FFilePathStructCustomization::CustomizeHeader( TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	PathStringProperty = StructPropertyHandle->GetChildHandle("FilePath");

	FString FileTypeFilter;

	// construct file type filter
	const FString& MetaData = StructPropertyHandle->GetMetaData(TEXT("FilePathFilter"));

	if (MetaData.IsEmpty())
	{
		FileTypeFilter = TEXT("All files (*.*)|*.*");
	}
	else
	{
		FileTypeFilter = FString::Printf(TEXT("%s files (*.%s)|*.%s"), *MetaData, *MetaData, *MetaData);
	}

	// create path picker widget
	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MaxDesiredWidth(0.0f)
		.MinDesiredWidth(125.0f)
		[
			SNew(SFilePathPicker)
				.BrowseButtonImage(FEditorStyle::GetBrush("PropertyWindow.Button_Ellipsis"))
				.BrowseButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
				.BrowseButtonToolTip(LOCTEXT("FileButtonToolTipText", "Choose a file from this computer"))
				.BrowseDirectory(FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN))
				.BrowseTitle(LOCTEXT("PropertyEditorTitle", "File picker..."))
				.FilePath(this, &FFilePathStructCustomization::HandleFilePathPickerFilePath)
				.FileTypeFilter(FileTypeFilter)
				.OnPathPicked(this, &FFilePathStructCustomization::HandleFilePathPickerPathPicked)
		];
}


/* FFilePathStructCustomization callbacks
 *****************************************************************************/

FString FFilePathStructCustomization::HandleFilePathPickerFilePath( ) const
{
	FString FilePath;
	PathStringProperty->GetValue(FilePath);

	return FilePath;
}


void FFilePathStructCustomization::HandleFilePathPickerPathPicked( const FString& PickedPath )
{
	PathStringProperty->SetValue(PickedPath);
	FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_OPEN, FPaths::GetPath(PickedPath));
}


#undef LOCTEXT_NAMESPACE
