// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "WindowsTargetSettingsDetails.h"
#include "TargetPlatform.h"
#include "SExternalImageReference.h"
#include "IExternalImagePickerModule.h"
#include "GameProjectGenerationModule.h"
#include "ISourceControlModule.h"

#define LOCTEXT_NAMESPACE "WindowsTargetSettingsDetails"

namespace WindowsTargetSettingsDetailsConstants
{
	/** The filename for the game splash screen */
	const FString GameSplashFileName(TEXT("Splash/Splash.bmp"));

	/** The filename for the editor splash screen */
	const FString EditorSplashFileName(TEXT("Splash/EdSplash.bmp"));

	/** ToolTip used when an option is not available to binary users. */
	const FText DisabledTip = LOCTEXT("GitHubSourceRequiredToolTip", "This requires GitHub source.");
}

FText GetFriendlyNameFromRHIName(const FString& InRHIName)
{
	FText FriendlyRHIName = LOCTEXT("UnknownRHI", "UnknownRHI");
	if (InRHIName == TEXT("PCD3D_SM5"))
	{
		FriendlyRHIName = LOCTEXT("DirectX11", "DirectX 11 (SM5)");
	}
	else if (InRHIName == TEXT("PCD3D_SM4"))
	{
		FriendlyRHIName = LOCTEXT("DirectX10", "DirectX 10 (SM4)");
	}
	else if (InRHIName == TEXT("GLSL_150"))
	{
		FriendlyRHIName = LOCTEXT("OpenGL3", "OpenGL 3 (SM4)");
	}
	else if (InRHIName == TEXT("GLSL_430"))
	{
		FriendlyRHIName = LOCTEXT("OpenGL4", "OpenGL 4 (SM5, Experimental)");
	}
	else if (InRHIName == TEXT("PC_VULKAN_ES2"))
	{
		FriendlyRHIName = LOCTEXT("VulkanES2", "Vulkan Mobile(ES2, Experimental)");
	}

	return FriendlyRHIName;
}


TSharedRef<IDetailCustomization> FWindowsTargetSettingsDetails::MakeInstance()
{
	return MakeShareable(new FWindowsTargetSettingsDetails);
}

namespace EImageScope
{
	enum Type
	{
		Engine,
		GameOverride
	};
}

/* Helper function used to generate filenames for splash screens */
static FString GetSplashFilename(EImageScope::Type Scope, bool bIsEditorSplash)
{
	FString Filename;

	if (Scope == EImageScope::Engine)
	{
		Filename = FPaths::EngineContentDir();
	}
	else
	{
		Filename = FPaths::GameContentDir();
	}

	if(bIsEditorSplash)
	{
		Filename /= WindowsTargetSettingsDetailsConstants::EditorSplashFileName;
	}
	else
	{
		Filename /= WindowsTargetSettingsDetailsConstants::GameSplashFileName;
	}

	Filename = FPaths::ConvertRelativePathToFull(Filename);

	return Filename;
}

/* Helper function used to generate filenames for icons */
static FString GetIconFilename(EImageScope::Type Scope)
{
	const FString& PlatformName = FModuleManager::GetModuleChecked<ITargetPlatformModule>("WindowsTargetPlatform").GetTargetPlatform()->PlatformName();

	if (Scope == EImageScope::Engine)
	{
		FString Filename = FPaths::EngineDir() / FString(TEXT("Source/Runtime/Launch/Resources")) / PlatformName / FString("UE4.ico");
		return FPaths::ConvertRelativePathToFull(Filename);
	}
	else
	{
		FString Filename = FPaths::GameDir() / TEXT("Build/Windows/Application.ico");
		if(!FPaths::FileExists(Filename))
		{
			FString LegacyFilename = FPaths::GameSourceDir() / FString(FApp::GetGameName()) / FString(TEXT("Resources")) / PlatformName / FString(FApp::GetGameName()) + TEXT(".ico");
			if(FPaths::FileExists(LegacyFilename))
			{
				Filename = LegacyFilename;
			}
		}
		return FPaths::ConvertRelativePathToFull(Filename);
	}
}

void FWindowsTargetSettingsDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	// Setup the supported/targeted RHI property view
	TargetShaderFormatsDetails = MakeShareable(new FTargetShaderFormatsPropertyDetails(&DetailBuilder));
	TargetShaderFormatsDetails->CreateTargetShaderFormatsPropertyView();

	TSharedRef<IPropertyHandle> MinOSProperty = DetailBuilder.GetProperty("MinimumOSVersion");
	IDetailCategoryBuilder& OSInfoCategory = DetailBuilder.EditCategory(TEXT("OS Info"));
	
	// Setup edit condition and tool tip of Min OS property. Determined by whether the engine is installed or not.
	bool bIsMinOSSelectionAvailable = FApp::IsEngineInstalled() == false;
	IDetailPropertyRow& MinOSRow = OSInfoCategory.AddProperty(MinOSProperty);
	MinOSRow.IsEnabled(bIsMinOSSelectionAvailable);
	MinOSRow.ToolTip(bIsMinOSSelectionAvailable ? MinOSProperty->GetToolTipText() : WindowsTargetSettingsDetailsConstants::DisabledTip);

	// Next add the splash image customization
	const FText EditorSplashDesc(LOCTEXT("EditorSplashLabel", "Editor Splash"));
	IDetailCategoryBuilder& SplashCategoryBuilder = DetailBuilder.EditCategory(TEXT("Splash"));
	FDetailWidgetRow& EditorSplashWidgetRow = SplashCategoryBuilder.AddCustomRow(EditorSplashDesc);

	const FString EditorSplash_TargetImagePath = GetSplashFilename(EImageScope::GameOverride, true);
	const FString EditorSplash_DefaultImagePath = GetSplashFilename(EImageScope::Engine, true);

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
			.OnGetPickerPath(FOnGetPickerPath::CreateSP(this, &FWindowsTargetSettingsDetails::GetPickerPath))
			.OnPostExternalImageCopy(FOnPostExternalImageCopy::CreateSP(this, &FWindowsTargetSettingsDetails::HandlePostExternalIconCopy))
		]
	];

	const FText GameSplashDesc(LOCTEXT("GameSplashLabel", "Game Splash"));
	FDetailWidgetRow& GameSplashWidgetRow = SplashCategoryBuilder.AddCustomRow(GameSplashDesc);
	const FString GameSplash_TargetImagePath = GetSplashFilename(EImageScope::GameOverride, false);
	const FString GameSplash_DefaultImagePath = GetSplashFilename(EImageScope::Engine, false);

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
			.OnGetPickerPath(FOnGetPickerPath::CreateSP(this, &FWindowsTargetSettingsDetails::GetPickerPath))
			.OnPostExternalImageCopy(FOnPostExternalImageCopy::CreateSP(this, &FWindowsTargetSettingsDetails::HandlePostExternalIconCopy))
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
			SNew(SExternalImageReference, GetIconFilename(EImageScope::Engine), GetIconFilename(EImageScope::GameOverride))
			.FileDescription(GameSplashDesc)
			.OnPreExternalImageCopy(FOnPreExternalImageCopy::CreateSP(this, &FWindowsTargetSettingsDetails::HandlePreExternalIconCopy))
			.OnGetPickerPath(FOnGetPickerPath::CreateSP(this, &FWindowsTargetSettingsDetails::GetPickerPath))
			.OnPostExternalImageCopy(FOnPostExternalImageCopy::CreateSP(this, &FWindowsTargetSettingsDetails::HandlePostExternalIconCopy))
		]
	];
}


bool FWindowsTargetSettingsDetails::HandlePreExternalIconCopy(const FString& InChosenImage)
{
	return true;
}


FString FWindowsTargetSettingsDetails::GetPickerPath()
{
	return FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN);
}


bool FWindowsTargetSettingsDetails::HandlePostExternalIconCopy(const FString& InChosenImage)
{
	FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_OPEN, FPaths::GetPath(InChosenImage));
	return true;
}

FTargetShaderFormatsPropertyDetails::FTargetShaderFormatsPropertyDetails(IDetailLayoutBuilder* InDetailBuilder)
: DetailBuilder(InDetailBuilder)
{
	TargetShaderFormatsPropertyHandle = DetailBuilder->GetProperty("TargetedRHIs");
	ensure(TargetShaderFormatsPropertyHandle.IsValid());
}

void FTargetShaderFormatsPropertyDetails::CreateTargetShaderFormatsPropertyView()
{
	DetailBuilder->HideProperty(TargetShaderFormatsPropertyHandle);

	// List of supported RHI's and selected targets
	ITargetPlatform* WindowsTargetPlatform = FModuleManager::GetModuleChecked<ITargetPlatformModule>("WindowsTargetPlatform").GetTargetPlatform();
	TArray<FName> ShaderFormats;
	WindowsTargetPlatform->GetAllPossibleShaderFormats(ShaderFormats);

	IDetailCategoryBuilder& TargetedRHICategoryBuilder = DetailBuilder->EditCategory(TEXT("Targeted RHIs"));

	for (const FName& ShaderFormat : ShaderFormats)
	{
		FText FriendlyShaderFormatName = GetFriendlyNameFromRHIName(ShaderFormat.ToString());

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
			.OnCheckStateChanged(this, &FTargetShaderFormatsPropertyDetails::OnTargetedRHIChanged, ShaderFormat)
			.IsChecked(this, &FTargetShaderFormatsPropertyDetails::IsTargetedRHIChecked, ShaderFormat)
		];
	}
}


void FTargetShaderFormatsPropertyDetails::OnTargetedRHIChanged(ECheckBoxState InNewValue, FName InRHIName)
{
	TArray<void*> RawPtrs;
	TargetShaderFormatsPropertyHandle->AccessRawData(RawPtrs);

	// Update the CVars with the selection
	{
		TargetShaderFormatsPropertyHandle->NotifyPreChange();
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
		TargetShaderFormatsPropertyHandle->NotifyPostChange();
	}
}


ECheckBoxState FTargetShaderFormatsPropertyDetails::IsTargetedRHIChecked(FName InRHIName) const
{
	ECheckBoxState CheckState = ECheckBoxState::Unchecked;

	TArray<void*> RawPtrs;
	TargetShaderFormatsPropertyHandle->AccessRawData(RawPtrs);
	
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
