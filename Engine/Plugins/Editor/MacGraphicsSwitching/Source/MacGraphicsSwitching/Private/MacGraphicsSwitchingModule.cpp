// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MacGraphicsSwitchingModule.h"
#include "IMacGraphicsSwitchingModule.h"
#include "MacGraphicsSwitchingSettings.h"
#include "MacGraphicsSwitchingSettingsDetails.h"
#include "MacGraphicsSwitchingStyle.h"
#include "MacGraphicsSwitchingWidget.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"
#include "Runtime/SlateCore/Public/Rendering/SlateRenderer.h"
#include "Editor/MainFrame/Public/Interfaces/IMainFrameModule.h"
#include "ISettingsModule.h"
#include "LevelEditor.h"
#include "ModuleManager.h"

#import <IOKit/ps/IOPowerSources.h>
#import <IOKit/ps/IOPSKeys.h>
#import <OpenGL/OpenGL.h>

#define LOCTEXT_NAMESPACE "MacGraphicsSwitching"

class FMacGraphicsSwitchingModule : public IMacGraphicsSwitchingModule
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** IMacGraphicsSwitchingModule implementation */
	virtual bool AllowAutomaticGraphicsSwitching() override;
	virtual bool AllowMultipleGPUs() override;
	
private:
	void Initialize( TSharedPtr<SWindow> InRootWindow, bool bIsNewProjectWindow );
	void AddGraphicsSwitcher( FToolBarBuilder& ToolBarBuilder );
	
private:
	TSharedPtr< FExtender > NotificationBarExtender;
	bool bAllowAutomaticGraphicsSwitching;
	bool bAllowMultiGPUs;
};

IMPLEMENT_MODULE(FMacGraphicsSwitchingModule, MacGraphicsSwitching)

void FMacGraphicsSwitchingModule::StartupModule()
{
	bAllowMultiGPUs = true;
	bAllowAutomaticGraphicsSwitching = false;
	
	CFTypeRef PowerSourcesInfo = IOPSCopyPowerSourcesInfo();
	if (PowerSourcesInfo)
	{
		CFArrayRef PowerSourcesArray = IOPSCopyPowerSourcesList(PowerSourcesInfo);
		for (CFIndex Index = 0; Index < CFArrayGetCount(PowerSourcesArray); Index++)
		{
			CFTypeRef PowerSource = CFArrayGetValueAtIndex(PowerSourcesArray, Index);
			NSDictionary* Description = (NSDictionary*)IOPSGetPowerSourceDescription(PowerSourcesInfo, PowerSource);
			if ([(NSString*)[Description objectForKey: @kIOPSTypeKey] isEqualToString: @kIOPSInternalBatteryType])
			{
				bAllowAutomaticGraphicsSwitching = true;
				bAllowMultiGPUs = false;
				break;
			}
		}
		CFRelease(PowerSourcesArray);
		CFRelease(PowerSourcesInfo);
	}
	
	if(bAllowMultiGPUs || bAllowAutomaticGraphicsSwitching)
	{
		GLint NumberOfRenderers = 0;
		CGLRendererInfoObj RendererInfo;
		CGLQueryRendererInfo(0xffffffff, &RendererInfo, &NumberOfRenderers);
		if(RendererInfo)
		{
			TArray<uint32> HardwareRenderers;
			for(uint32 i = 0; i < NumberOfRenderers; i++)
			{
				GLint bAccelerated = 0;
				CGLDescribeRenderer(RendererInfo, i, kCGLRPAccelerated, &bAccelerated);
				if(bAccelerated)
				{
					HardwareRenderers.Push(i);
				}
			}
			
			bool bRenderersMatch = true;
			GLint MajorGLVersion = 0;
			CGLDescribeRenderer(RendererInfo, 0, kCGLRPMajorGLVersion, &MajorGLVersion);
			for(uint32 i = 1; i < HardwareRenderers.Num(); i++)
			{
				GLint OtherMajorGLVersion = 0;
				CGLDescribeRenderer(RendererInfo, i, kCGLRPMajorGLVersion, &OtherMajorGLVersion);
				bRenderersMatch &= MajorGLVersion == OtherMajorGLVersion;
			}
			
			CGLDestroyRendererInfo(RendererInfo);
			
			// Can only automatically switch when the renderers use the same major GL version, or errors will occur.
			bAllowAutomaticGraphicsSwitching &= bRenderersMatch;
			bAllowMultiGPUs &= bRenderersMatch;
		}
	}
	
	// Register settings
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if( SettingsModule != nullptr )
	{
		SettingsModule->RegisterSettings( "Editor", "Plugins", "MacGraphicsSwitching",
										 LOCTEXT( "MacGraphicsSwitchingSettingsName", "Graphics Switching"),
										 LOCTEXT( "MacGraphicsSwitchingSettingsDescription", "Settings for Mac OS X graphics switching"),
										 GetMutableDefault< UMacGraphicsSwitchingSettings >()
										 );
		
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout("MacGraphicsSwitchingSettings", FOnGetDetailCustomizationInstance::CreateStatic(&FMacGraphicsSwitchingSettingsDetails::MakeInstance));
		PropertyModule.NotifyCustomizationModuleChanged();
		
		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		MainFrameModule.OnMainFrameCreationFinished().AddRaw( this, &FMacGraphicsSwitchingModule::Initialize );
	}
}

void FMacGraphicsSwitchingModule::Initialize( TSharedPtr<SWindow> InRootWindow, bool bIsNewProjectWindow )
{
	if( !bIsNewProjectWindow )
	{
		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		MainFrameModule.OnMainFrameCreationFinished().RemoveAll(this);

		FMacGraphicsSwitchingStyle::Initialize();
		
		bool MacUseAutomaticGraphicsSwitching = false;
		if ( GConfig->GetBool(TEXT("/Script/MacGraphicsSwitching.MacGraphicsSwitchingSettings"), TEXT("bAllowAutomaticGraphicsSwitching"), MacUseAutomaticGraphicsSwitching, GEditorSettingsIni) && MacUseAutomaticGraphicsSwitching )
		{
			if ( AllowAutomaticGraphicsSwitching() )
			{
				NotificationBarExtender = MakeShareable( new FExtender() );
				NotificationBarExtender->AddToolBarExtension("Start",
															 EExtensionHook::After,
															 nullptr,
															 FToolBarExtensionDelegate::CreateRaw(this, &FMacGraphicsSwitchingModule::AddGraphicsSwitcher));
				
				FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
				LevelEditorModule.GetNotificationBarExtensibilityManager()->AddExtender( NotificationBarExtender );
				LevelEditorModule.BroadcastNotificationBarChanged();
			}
			else
			{
				GConfig->SetBool(TEXT("/Script/MacGraphicsSwitching.MacGraphicsSwitchingSettings"), TEXT("bAllowAutomaticGraphicsSwitching"), false, GEditorSettingsIni);
			}
		}
	}
}

void FMacGraphicsSwitchingModule::AddGraphicsSwitcher( FToolBarBuilder& ToolBarBuilder )
{
	ToolBarBuilder.AddWidget( SNew( SMacGraphicsSwitchingWidget ).bLiveSwitching(true) );
}

void FMacGraphicsSwitchingModule::ShutdownModule()
{
	// Unregister settings
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if( SettingsModule != nullptr )
	{
		SettingsModule->UnregisterSettings( "Editor", "Plugins", "MacGraphicsSwitching" );
	}
	
	if ( FModuleManager::Get().IsModuleLoaded("LevelEditor") && NotificationBarExtender.IsValid() )
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetNotificationBarExtensibilityManager()->RemoveExtender( NotificationBarExtender );
	}
	
	if ( FModuleManager::Get().IsModuleLoaded("MainFrame") )
	{
		FMacGraphicsSwitchingStyle::Shutdown();
		
		IMainFrameModule& MainFrameModule = FModuleManager::GetModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		MainFrameModule.OnMainFrameCreationFinished().RemoveAll(this);
	}
	
	if(FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyEditorModule.UnregisterCustomClassLayout("MacGraphicsSwitchingSettings");
	}
}

bool FMacGraphicsSwitchingModule::AllowAutomaticGraphicsSwitching()
{
	return bAllowAutomaticGraphicsSwitching;
}

bool FMacGraphicsSwitchingModule::AllowMultipleGPUs()
{
	return bAllowMultiGPUs;
}

#undef LOCTEXT_NAMESPACE
