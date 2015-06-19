// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "NativeCodeGenerationTool.h"
#include "Dialogs.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/UserDefinedStruct.h"
#include "SourceCodeNavigation.h"
#include "DesktopPlatformModule.h"

#define LOCTEXT_NAMESPACE "NativeCodeGenerationTool"

//
//	THE CODE SHOULD BE MOVED TO GAMEPROJECTGENERATION
//


struct FGeneratedCodeData
{
	FGeneratedCodeData(UBlueprint& InBlueprint) 
		: HeaderSource(new FString())
		, CppSource(new FString())
		, Blueprint(&InBlueprint)
	{
		FName GeneratedClassName, SkeletonClassName;
		InBlueprint.GetBlueprintClassNames(GeneratedClassName, SkeletonClassName);
		ClassName = GeneratedClassName.ToString();

		GatherUserDefinedDependencies(InBlueprint);
	}

	FString TypeDependencies;
	TSharedPtr<FString> HeaderSource;
	TSharedPtr<FString> CppSource;
	FString ErrorString;
	FString ClassName;
	TWeakObjectPtr<UBlueprint> Blueprint;

	void GatherUserDefinedDependencies(UBlueprint& InBlueprint)
	{
		TArray<UObject*> ReferencedObjects;
		{
			FReferenceFinder ReferenceFinder(ReferencedObjects, NULL, false, false, false, false);

			{
				TArray<UObject*> ObjectsToCheck;
				GetObjectsWithOuter(InBlueprint.GeneratedClass, ObjectsToCheck, true);
				for (auto Obj : ObjectsToCheck)
				{
					if (IsValid(Obj))
					{
						ReferenceFinder.FindReferences(Obj);
					}
				}
			}

			for (UClass* Class = InBlueprint.GeneratedClass->GetSuperClass(); Class && !Class->HasAnyClassFlags(CLASS_Native); Class = Class->GetSuperClass())
			{
				ReferencedObjects.Add(Class);
			}
		}

		TypeDependencies.Empty();
		for (auto Obj : ReferencedObjects)
		{
			if (IsValid(Obj) && !Obj->IsIn(InBlueprint.GeneratedClass) && (Obj != InBlueprint.GeneratedClass))
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
		return FPaths::Combine(*DefaultSourceDir, FApp::GetGameName(), TEXT("Public"));
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
		FScopedSlowTask SlowTask(7, LOCTEXT("GeneratingCppFiles", "Generating C++ files.."));
		SlowTask.MakeDialog();
		SlowTask.EnterProgressFrame();

		if (!Blueprint.IsValid())
		{
			ErrorString += LOCTEXT("InvalidBlueprint", "Invalid Blueprint\n").ToString();
			return false;
		}

		FKismetEditorUtilities::GenerateCppCode(Blueprint.Get(), HeaderSource, CppSource, ClassName);

		SlowTask.EnterProgressFrame();

		bool bProjectHadCodeFiles = false;
		{
			TArray<FString> OutProjectCodeFilenames;
			IFileManager::Get().FindFilesRecursive(OutProjectCodeFilenames, *FPaths::GameSourceDir(), TEXT("*.h"), true, false, false);
			IFileManager::Get().FindFilesRecursive(OutProjectCodeFilenames, *FPaths::GameSourceDir(), TEXT("*.cpp"), true, false, false);
			bProjectHadCodeFiles = OutProjectCodeFilenames.Num() > 0;
		}

		TArray<FString> CreatedFiles;

		const FString NeHeaderFilename = FPaths::Combine(*HeaderDirPath, *HeaderFileName());
		const bool bHeaderSaved = FFileHelper::SaveStringToFile(*HeaderSource, *NeHeaderFilename);
		if (!bHeaderSaved)
		{
			ErrorString += LOCTEXT("HeaderNotSaved", "Header file wasn't saved. Check log for details.\n").ToString();
		}
		else
		{
			CreatedFiles.Add(NeHeaderFilename);
		}

		SlowTask.EnterProgressFrame();

		const FString NewCppFilename = FPaths::Combine(*CppDirPath, *SourceFileName());
		const bool bCppSaved = FFileHelper::SaveStringToFile(*CppSource, *NewCppFilename);
		if (!bCppSaved)
		{
			ErrorString += LOCTEXT("CppNotSaved", "Cpp file wasn't saved. Check log for details.\n").ToString();
		}
		else
		{
			CreatedFiles.Add(NewCppFilename);
		}

		TArray<FString> CreatedFilesForExternalAppRead;
		CreatedFilesForExternalAppRead.Reserve(CreatedFiles.Num());
		for (const FString& CreatedFile : CreatedFiles)
		{
			CreatedFilesForExternalAppRead.Add(IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*CreatedFile));
		}

		SlowTask.EnterProgressFrame();

		bool bGenerateProjectFiles = true;
		// First see if we can avoid a full generation by adding the new files to an already open project
		if (bProjectHadCodeFiles && FSourceCodeNavigation::AddSourceFiles(CreatedFilesForExternalAppRead))
		{
			// We successfully added the new files to the solution, but we still need to run UBT with -gather to update any UBT makefiles
			if (FDesktopPlatformModule::Get()->InvalidateMakefiles(FPaths::RootDir(), FPaths::GetProjectFilePath(), GWarn))
			{
				// We managed the gather, so we can skip running the full generate
				bGenerateProjectFiles = false;
			}
		}

		SlowTask.EnterProgressFrame();

		bool bProjectFileUpdated = true;
		if (bGenerateProjectFiles)
		{
			// Generate project files if we happen to be using a project file.
			if (!FDesktopPlatformModule::Get()->GenerateProjectFiles(FPaths::RootDir(), FPaths::GetProjectFilePath(), GWarn))
			{
				ErrorString += LOCTEXT("FailedToGenerateProjectFiles", "Failed to generate project files.").ToString();
				bProjectFileUpdated = false;
			}
		}

		return bHeaderSaved && bCppSaved && bProjectFileUpdated;
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

	FText GetClassName() const
	{
		return GeneratedCodeData.IsValid() ? FText::FromString(GeneratedCodeData->ClassName) : FText::GetEmpty();
	}

	void OnClassNameCommited(const FText& NameText, ETextCommit::Type TextCommit)
	{
		if (GeneratedCodeData.IsValid())
		{
			GeneratedCodeData->ClassName = NameText.ToString();
			if (HeaderDirectoryBrowser.IsValid())
			{
				HeaderDirectoryBrowser->File = GeneratedCodeData->HeaderFileName();
			}
			if (SourceDirectoryBrowser.IsValid())
			{
				SourceDirectoryBrowser->File = GeneratedCodeData->SourceFileName();
			}
		}
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
					.Text(LOCTEXT("ClassName", "Class Name"))
				]
				+ SVerticalBox::Slot()
				.Padding(4.0f)
				.AutoHeight()
				[
					SNew(SEditableTextBox)
					.Text(this, &SNativeCodeGenerationDialog::GetClassName)
					.OnTextCommitted(this, &SNativeCodeGenerationDialog::OnClassNameCommited)
					.IsEnabled(this, &SNativeCodeGenerationDialog::IsEditable)
				]
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
		&& (Blueprint.BlueprintType == EBlueprintType::BPTYPE_Normal || Blueprint.BlueprintType == EBlueprintType::BPTYPE_FunctionLibrary)
		&& (Blueprint.GeneratedClass->GetClass() == UBlueprintGeneratedClass::StaticClass());
}

#undef LOCTEXT_NAMESPACE