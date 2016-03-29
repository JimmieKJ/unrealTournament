// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "MacTargetSettingsDetails.h"
#include "TargetPlatform.h"
#include "SExternalImageReference.h"
#include "IExternalImagePickerModule.h"
#include "GameProjectGenerationModule.h"
#include "ISourceControlModule.h"

namespace MacTargetSettingsDetailsConstants
{
	/** The filename for the game splash screen */
	const FString GameSplashFileName(TEXT("Splash/Splash.bmp"));

	/** The filename for the editor splash screen */
	const FString EditorSplashFileName(TEXT("Splash/EdSplash.bmp"));
}

#define LOCTEXT_NAMESPACE "MacTargetSettingsDetails"

TSharedRef<IDetailCustomization> FMacTargetSettingsDetails::MakeInstance()
{
	return MakeShareable(new FMacTargetSettingsDetails);
}

namespace EMacImageScope
{
	enum Type
	{
		Engine,
		GameOverride
	};
}

/* Helper function used to generate filenames for splash screens */
static FString GetSplashFilename(EMacImageScope::Type Scope, bool bIsEditorSplash)
{
	FString Filename;

	if (Scope == EMacImageScope::Engine)
	{
		Filename = FPaths::EngineContentDir();
	}
	else
	{
		Filename = FPaths::GameContentDir();
	}

	if(bIsEditorSplash)
	{
		Filename /= MacTargetSettingsDetailsConstants::EditorSplashFileName;
	}
	else
	{
		Filename /= MacTargetSettingsDetailsConstants::GameSplashFileName;
	}

	Filename = FPaths::ConvertRelativePathToFull(Filename);

	return Filename;
}

/* Helper function used to generate filenames for icons */
static FString GetIconFilename(EMacImageScope::Type Scope)
{
	const FString& PlatformName = FModuleManager::GetModuleChecked<ITargetPlatformModule>("MacTargetPlatform").GetTargetPlatform()->PlatformName();

	if (Scope == EMacImageScope::Engine)
	{
		FString Filename = FPaths::EngineDir() / FString(TEXT("Source/Runtime/Launch/Resources")) / PlatformName / FString("UE4.icns");
		return FPaths::ConvertRelativePathToFull(Filename);
	}
	else
	{
		FString Filename = FPaths::GameDir() / TEXT("Build/Mac/Application.icns");
		if(!FPaths::FileExists(Filename))
		{
			FString LegacyFilename = FPaths::GameSourceDir() / FString(FApp::GetGameName()) / FString(TEXT("Resources")) / PlatformName / FString(FApp::GetGameName()) + TEXT(".icns");
			if(FPaths::FileExists(LegacyFilename))
			{
				Filename = LegacyFilename;
			}
		}
		return FPaths::ConvertRelativePathToFull(Filename);
	}
}

void FMacTargetSettingsDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	// Setup the supported/targeted RHI property view
	TargetShaderFormatsDetails = MakeShareable(new FMacShaderFormatsPropertyDetails(&DetailBuilder, TEXT("TargetedRHIs"), TEXT("Targeted RHIs")));
	TargetShaderFormatsDetails->CreateTargetShaderFormatsPropertyView();
	
	// Setup the supported/targeted RHI property view
	CachedShaderFormatsDetails = MakeShareable(new FMacShaderFormatsPropertyDetails(&DetailBuilder, TEXT("CachedShaderFormats"), TEXT("Cached Shader Formats")));
	CachedShaderFormatsDetails->CreateTargetShaderFormatsPropertyView();
	
	// Add the splash image customization
	const FText EditorSplashDesc(LOCTEXT("EditorSplashLabel", "Editor Splash"));
	IDetailCategoryBuilder& SplashCategoryBuilder = DetailBuilder.EditCategory(TEXT("Splash"));
	FDetailWidgetRow& EditorSplashWidgetRow = SplashCategoryBuilder.AddCustomRow(EditorSplashDesc);

	const FString EditorSplash_TargetImagePath = GetSplashFilename(EMacImageScope::GameOverride, true);
	const FString EditorSplash_DefaultImagePath = GetSplashFilename(EMacImageScope::Engine, true);

	EditorSplashWidgetRow
	.NameContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Text(EditorSplashDesc)
			.Font(DetailBuilder.GetDetailFont())
		]
	]
	.ValueContent()
	.MaxDesiredWidth(500.0f)
	.MinDesiredWidth(100.0f)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SExternalImageReference, EditorSplash_DefaultImagePath, EditorSplash_TargetImagePath)
			.FileDescription(EditorSplashDesc)
			.OnGetPickerPath(FOnGetPickerPath::CreateSP(this, &FMacTargetSettingsDetails::GetPickerPath))
			.OnPostExternalImageCopy(FOnPostExternalImageCopy::CreateSP(this, &FMacTargetSettingsDetails::HandlePostExternalIconCopy))
		]
	];

	const FText GameSplashDesc(LOCTEXT("GameSplashLabel", "Game Splash"));
	FDetailWidgetRow& GameSplashWidgetRow = SplashCategoryBuilder.AddCustomRow(GameSplashDesc);

	const FString GameSplash_TargetImagePath = GetSplashFilename(EMacImageScope::GameOverride, false);
	const FString GameSplash_DefaultImagePath = GetSplashFilename(EMacImageScope::Engine, false);

	GameSplashWidgetRow
	.NameContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Text(GameSplashDesc)
			.Font(DetailBuilder.GetDetailFont())
		]
	]
	.ValueContent()
	.MaxDesiredWidth(500.0f)
	.MinDesiredWidth(100.0f)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SExternalImageReference, GameSplash_DefaultImagePath, GameSplash_TargetImagePath)
			.FileDescription(GameSplashDesc)
			.OnGetPickerPath(FOnGetPickerPath::CreateSP(this, &FMacTargetSettingsDetails::GetPickerPath))
			.OnPostExternalImageCopy(FOnPostExternalImageCopy::CreateSP(this, &FMacTargetSettingsDetails::HandlePostExternalIconCopy))
		]
	];

	IDetailCategoryBuilder& IconsCategoryBuilder = DetailBuilder.EditCategory(TEXT("Icon"));	
	FDetailWidgetRow& GameIconWidgetRow = IconsCategoryBuilder.AddCustomRow(LOCTEXT("GameIconLabel", "Game Icon"));
	GameIconWidgetRow
	.NameContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("GameIconLabel", "Game Icon"))
			.Font(DetailBuilder.GetDetailFont())
		]
	]
	.ValueContent()
	.MaxDesiredWidth(500.0f)
	.MinDesiredWidth(100.0f)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SExternalImageReference, GetIconFilename(EMacImageScope::Engine), GetIconFilename(EMacImageScope::GameOverride))
			.FileDescription(GameSplashDesc)
			.OnPreExternalImageCopy(FOnPreExternalImageCopy::CreateSP(this, &FMacTargetSettingsDetails::HandlePreExternalIconCopy))
			.OnGetPickerPath(FOnGetPickerPath::CreateSP(this, &FMacTargetSettingsDetails::GetPickerPath))
			.OnPostExternalImageCopy(FOnPostExternalImageCopy::CreateSP(this, &FMacTargetSettingsDetails::HandlePostExternalIconCopy))
		]
	];
}


bool FMacTargetSettingsDetails::HandlePreExternalIconCopy(const FString& InChosenImage)
{
	return true;
}


FString FMacTargetSettingsDetails::GetPickerPath()
{
	return FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN);
}


bool FMacTargetSettingsDetails::HandlePostExternalIconCopy(const FString& InChosenImage)
{
	FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_OPEN, FPaths::GetPath(InChosenImage));
	return true;
}

FText GetFriendlyNameFromRHINameMac(const FString& InRHIName)
{
	FText FriendlyRHIName = LOCTEXT("UnknownRHI", "UnknownRHI");
	if (InRHIName == TEXT("GLSL_150_MAC"))
	{
		FriendlyRHIName = LOCTEXT("OpenGL3", "OpenGL 3 (SM4)");
	}
	else if (InRHIName == TEXT("SF_METAL_MACES3_1"))
	{
		FriendlyRHIName = LOCTEXT("MetalES3.1", "Metal (ES3.1, Mobile Preview)");
	}
	else if (InRHIName == TEXT("SF_METAL_SM4"))
	{
		FriendlyRHIName = LOCTEXT("MetalSM4", "Metal (SM4)");
	}
	else if (InRHIName == TEXT("SF_METAL_SM5"))
	{
		FriendlyRHIName = LOCTEXT("MetalSM5", "Metal (SM5, Experimental)");
	}
	
	return FriendlyRHIName;
}

FMacShaderFormatsPropertyDetails::FMacShaderFormatsPropertyDetails(IDetailLayoutBuilder* InDetailBuilder, FString InProperty, FString InTitle)
: DetailBuilder(InDetailBuilder)
, Property(InProperty)
, Title(InTitle)
{
	ShaderFormatsPropertyHandle = DetailBuilder->GetProperty(*Property);
	ensure(ShaderFormatsPropertyHandle.IsValid());
}

void FMacShaderFormatsPropertyDetails::CreateTargetShaderFormatsPropertyView()
{
	DetailBuilder->HideProperty(ShaderFormatsPropertyHandle);
	
	// List of supported RHI's and selected targets
	ITargetPlatform* TargetPlatform = FModuleManager::GetModuleChecked<ITargetPlatformModule>("MacTargetPlatform").GetTargetPlatform();
	TArray<FName> ShaderFormats;
	TargetPlatform->GetAllPossibleShaderFormats(ShaderFormats);
	
	IDetailCategoryBuilder& TargetedRHICategoryBuilder = DetailBuilder->EditCategory(*Title);
	
	for (const FName& ShaderFormat : ShaderFormats)
	{
		FText FriendlyShaderFormatName = GetFriendlyNameFromRHINameMac(ShaderFormat.ToString());
		
		FDetailWidgetRow& TargetedRHIWidgetRow = TargetedRHICategoryBuilder.AddCustomRow(FriendlyShaderFormatName);
		
		TargetedRHIWidgetRow
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0, 1, 0, 1))
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(FriendlyShaderFormatName)
				.Font(DetailBuilder->GetDetailFont())
			 ]
		 ]
		.ValueContent()
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this, &FMacShaderFormatsPropertyDetails::OnTargetedRHIChanged, ShaderFormat)
			.IsChecked(this, &FMacShaderFormatsPropertyDetails::IsTargetedRHIChecked, ShaderFormat)
		 ];
	}
}


void FMacShaderFormatsPropertyDetails::OnTargetedRHIChanged(ECheckBoxState InNewValue, FName InRHIName)
{
	TArray<void*> RawPtrs;
	ShaderFormatsPropertyHandle->AccessRawData(RawPtrs);
	
	// Update the CVars with the selection
	{
		ShaderFormatsPropertyHandle->NotifyPreChange();
		for (void* RawPtr : RawPtrs)
		{
			TArray<FString>& Array = *(TArray<FString>*)RawPtr;
			if(InNewValue == ECheckBoxState::Checked)
			{
				Array.Add(InRHIName.ToString());
			}
			else
			{
				Array.Remove(InRHIName.ToString());
			}
		}
		ShaderFormatsPropertyHandle->NotifyPostChange();
	}
}


ECheckBoxState FMacShaderFormatsPropertyDetails::IsTargetedRHIChecked(FName InRHIName) const
{
	ECheckBoxState CheckState = ECheckBoxState::Unchecked;
	
	TArray<void*> RawPtrs;
	ShaderFormatsPropertyHandle->AccessRawData(RawPtrs);
	
	for(void* RawPtr : RawPtrs)
	{
		TArray<FString>& Array = *(TArray<FString>*)RawPtr;
		if(Array.Contains(InRHIName.ToString()))
		{
			CheckState = ECheckBoxState::Checked;
		}
	}
	return CheckState;
}

#undef LOCTEXT_NAMESPACE
