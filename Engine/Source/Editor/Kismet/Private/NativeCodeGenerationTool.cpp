// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "NativeCodeGenerationTool.h"
#include "Dialogs.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/UserDefinedStruct.h"

#define LOCTEXT_NAMESPACE "NativeCodeGenerationTool"

struct FGeneratedCodeData
{
	FGeneratedCodeData(UBlueprint& Blueprint) : HeaderSource(new FString()), CppSource(new FString())
	{
		FName GeneratedClassName, SkeletonClassName;
		Blueprint.GetBlueprintClassNames(GeneratedClassName, SkeletonClassName);
		ClassName = GeneratedClassName.ToString();

		GatherUserDefinedDependencies(Blueprint);
		
		FKismetEditorUtilities::GenerateCppCode(&Blueprint, HeaderSource, CppSource);
	}

	FString TypeDependencies;
	TSharedPtr<FString> HeaderSource;
	TSharedPtr<FString> CppSource;
	FString ErrorString;
	FString ClassName;

	void GatherUserDefinedDependencies(UBlueprint& Blueprint)
	{
		TArray<UObject*> ReferencedObjects;
		{
			FReferenceFinder ReferenceFinder(ReferencedObjects, NULL, false, false, false, false);

			{
				TArray<UObject*> ObjectsToCheck;
				GetObjectsWithOuter(Blueprint.GeneratedClass, ObjectsToCheck, true);
				for (auto Obj : ObjectsToCheck)
				{
					if (IsValid(Obj))
					{
						ReferenceFinder.FindReferences(Obj);
					}
				}
			}

			for (UClass* Class = Blueprint.GeneratedClass->GetSuperClass(); Class && !Class->HasAnyClassFlags(CLASS_Native); Class = Class->GetSuperClass())
			{
				ReferencedObjects.Add(Class);
			}
		}

		TypeDependencies.Empty();
		for (auto Obj : ReferencedObjects)
		{
			if (IsValid(Obj) && !Obj->IsIn(Blueprint.GeneratedClass) && (Obj != Blueprint.GeneratedClass))
			{
				if (Obj->IsA<UBlueprintGeneratedClass>() || Obj->IsA<UUserDefinedEnum>() || Obj->IsA<UUserDefinedStruct>())
				{
					TypeDependencies += Obj->GetPathName();
					TypeDependencies += TEXT("\n");
				}
			}
		}

		if (TypeDependencies.IsEmpty())
		{
			TypeDependencies += LOCTEXT("NoNonNativeDependencies", "No non-native dependencies.").ToString();
		}
	}

	static FString DefaultHeaderDir()
	{
		auto DefaultSourceDir = FPaths::ConvertRelativePathToFull(FPaths::GameSourceDir());
		return FPaths::Combine(*DefaultSourceDir, FApp::GetGameName(), TEXT("Classes"));
	}

	static FString DefaultSourceDir()
	{
		auto DefaultSourceDir = FPaths::ConvertRelativePathToFull(FPaths::GameSourceDir());
		return FPaths::Combine(*DefaultSourceDir, FApp::GetGameName(), TEXT("Private"));
	}

	FString HeaderFileName() const
	{
		return ClassName + TEXT(".h");
	}

	FString SourceFileName() const
	{
		return ClassName + TEXT(".cpp");
	}

	bool Save(const FString& HeaderDirPath, const FString& CppDirPath)
	{
		const bool bHeaderSaved = FFileHelper::SaveStringToFile(*HeaderSource, *FPaths::Combine(*HeaderDirPath, *HeaderFileName()));
		if (!bHeaderSaved)
		{
			ErrorString += LOCTEXT("HeaderNotSaved", "Header file wasn't saved. Check log for details.\n").ToString();
		}
		const bool bCppSaved = FFileHelper::SaveStringToFile(*CppSource, *FPaths::Combine(*CppDirPath, *SourceFileName()));
		if (!bCppSaved)
		{
			ErrorString += LOCTEXT("CppNotSaved", "Cpp file wasn't saved. Check log for details.\n").ToString();
		}

		return bHeaderSaved && bCppSaved;
	}
};

class SSimpleDirectoryPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSimpleDirectoryPicker){}
		SLATE_ARGUMENT(FString, Directory)
		SLATE_ARGUMENT(FString, File)
		SLATE_ARGUMENT(FText, Message)
		SLATE_ATTRIBUTE(bool, IsEnabled)
	SLATE_END_ARGS()

	FString File;
	FString Directory;
	FText Message;

	TSharedPtr<SEditableTextBox> EditableTextBox;

	FString GetFilePath() const
	{
		return FPaths::Combine(*Directory, *File);
	}

	FText GetFilePathText() const
	{
		return FText::FromString(GetFilePath());
	}

	FReply BrowseHeaderDirectory()
	{
		PromptUserForDirectory(Directory, Message.ToString(), Directory);

		//workaround for UI problem
		EditableTextBox->SetText(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SSimpleDirectoryPicker::GetFilePathText)));

		return FReply::Handled();
	}

	void Construct(const FArguments& InArgs)
	{
		Directory = InArgs._Directory;
		File = InArgs._File;
		Message = InArgs._Message;

		TSharedPtr<SButton> OpenButton;
		ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(EditableTextBox, SEditableTextBox)
				.Text(this, &SSimpleDirectoryPicker::GetFilePathText)
				.IsReadOnly(true)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(OpenButton, SButton)
				.ToolTipText(Message)
				.OnClicked(this, &SSimpleDirectoryPicker::BrowseHeaderDirectory)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("PropertyWindow.Button_Ellipsis"))
				]
			]
		];

		OpenButton->SetEnabled(InArgs._IsEnabled);
	}
};

class SNativeCodeGenerationDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNativeCodeGenerationDialog){}
		SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)
		SLATE_ARGUMENT(TSharedPtr<FGeneratedCodeData>, GeneratedCodeData)
	SLATE_END_ARGS()

private:
	
	// Child widgets
	TSharedPtr<SSimpleDirectoryPicker> HeaderDirectoryBrowser;
	TSharedPtr<SSimpleDirectoryPicker> SourceDirectoryBrowser;
	TSharedPtr<SErrorText> ErrorWidget;
	// 
	TWeakPtr<SWindow> WeakParentWindow;
	TSharedPtr<FGeneratedCodeData> GeneratedCodeData;
	bool bSaved;

	void CloseParentWindow()
	{
		auto ParentWindow = WeakParentWindow.Pin();
		if (ParentWindow.IsValid())
		{
			ParentWindow->RequestDestroyWindow();
		}
	}

	bool IsEditable() const
	{
		return !bSaved && GeneratedCodeData->ErrorString.IsEmpty();
	}

	FReply OnButtonClicked()
	{
		if (IsEditable())
		{
			bSaved = GeneratedCodeData->Save(HeaderDirectoryBrowser->Directory, SourceDirectoryBrowser->Directory);
			ErrorWidget->SetError(GeneratedCodeData->ErrorString);
		}
		else
		{
			CloseParentWindow();
		}
		
		return FReply::Handled();
	}

	FText ButtonText() const
	{
		return IsEditable() ? LOCTEXT("Generate", "Generate") : LOCTEXT("Close", "Close");
	}

public:

	void Construct(const FArguments& InArgs)
	{
		GeneratedCodeData = InArgs._GeneratedCodeData;
		bSaved = false;
		WeakParentWindow = InArgs._ParentWindow;

		ChildSlot
		[
			SNew(SBorder)
			.Padding(4.f)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(4.0f)
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("HeaderPath", "Header Path"))
				]
				+ SVerticalBox::Slot()
				.Padding(4.0f)
				.AutoHeight()
				[
					SAssignNew(HeaderDirectoryBrowser, SSimpleDirectoryPicker)
					.Directory(FGeneratedCodeData::DefaultHeaderDir())
					.File(GeneratedCodeData->HeaderFileName())
					.Message(LOCTEXT("HeaderDirectory", "Header Directory"))
					.IsEnabled(this, &SNativeCodeGenerationDialog::IsEditable)
				]
				+ SVerticalBox::Slot()
				.Padding(4.0f)
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SourcePath", "Source Path"))
				]
				+ SVerticalBox::Slot()
				.Padding(4.0f)
				.AutoHeight()
				[
					SAssignNew(SourceDirectoryBrowser, SSimpleDirectoryPicker)
					.Directory(FGeneratedCodeData::DefaultSourceDir())
					.File(GeneratedCodeData->SourceFileName())
					.Message(LOCTEXT("SourceDirectory", "Source Directory"))
					.IsEnabled(this, &SNativeCodeGenerationDialog::IsEditable)
				]
				+ SVerticalBox::Slot()
				.Padding(4.0f)
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Dependencies", "Dependencies"))
				]
				+ SVerticalBox::Slot()
				.Padding(4.0f)
				.AutoHeight()
				[
					SNew(SBox)
					.WidthOverride(360.0f)
					.HeightOverride(200.0f)
					[
						SNew(SMultiLineEditableTextBox)
						.IsReadOnly(true)
						.Text(FText::FromString(GeneratedCodeData->TypeDependencies))
					]
				]
				+ SVerticalBox::Slot()
				.Padding(4.0f)
				.AutoHeight()
				[
					SAssignNew(ErrorWidget, SErrorText)
				]
				+ SVerticalBox::Slot()
				.Padding(4.0f)
				.AutoHeight()
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.Text(this, &SNativeCodeGenerationDialog::ButtonText)
					.OnClicked(this, &SNativeCodeGenerationDialog::OnButtonClicked)
				]
			]
		];

		ErrorWidget->SetError(GeneratedCodeData->ErrorString);
	}
};

void FNativeCodeGenerationTool::Open(UBlueprint& Blueprint, TSharedRef< class FBlueprintEditor> Editor)
{
	TSharedRef<FGeneratedCodeData> GeneratedCodeData(new FGeneratedCodeData(Blueprint));

	TSharedRef<SWindow> PickerWindow = SNew(SWindow)
		.Title(LOCTEXT("GenerateNativeCode", "Generate Native Code"))
		.SizingRule(ESizingRule::Autosized)
		.ClientSize(FVector2D(0.f, 300.f))
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	TSharedRef<SNativeCodeGenerationDialog> CodeGenerationDialog = SNew(SNativeCodeGenerationDialog)
		.ParentWindow(PickerWindow)
		.GeneratedCodeData(GeneratedCodeData);

	PickerWindow->SetContent(CodeGenerationDialog);
	GEditor->EditorAddModalWindow(PickerWindow);
}

bool FNativeCodeGenerationTool::CanGenerate(const UBlueprint& Blueprint)
{
	return (Blueprint.Status == EBlueprintStatus::BS_UpToDate)
		&& (Blueprint.BlueprintType == EBlueprintType::BPTYPE_Normal)
		&& (Blueprint.GeneratedClass->GetClass() == UBlueprintGeneratedClass::StaticClass());
}

#undef LOCTEXT_NAMESPACE