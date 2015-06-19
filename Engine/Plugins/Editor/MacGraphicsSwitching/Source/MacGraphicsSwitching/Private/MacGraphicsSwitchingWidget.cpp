// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MacGraphicsSwitchingModule.h"
#include "MacGraphicsSwitchingStyle.h"
#include "MacGraphicsSwitchingWidget.h"
#include "DetailLayoutBuilder.h"
#include <OpenGL/gl3.h>

#define LOCTEXT_NAMESPACE "MacGraphicsSwitchingWidget"

SMacGraphicsSwitchingWidget::SMacGraphicsSwitchingWidget()
: bLiveSwitching(false)
{
	// regenerate renderer list
	Renderers.Empty();
	
	FString DefaultDesc = FString(TEXT("System Default: ")) + FPlatformMisc::GetPrimaryGPUBrand();
	Renderers.Add(MakeShareable(new FRendererItem(FText::FromString(DefaultDesc), 0)));
	
	NSOpenGLPixelFormatAttribute Attributes[] =
	{
		NSOpenGLPFAOpenGLProfile,
		NSOpenGLProfileVersion3_2Core,
		NSOpenGLPFAAccelerated,
		NSOpenGLPFANoRecovery,
		NSOpenGLPFAAllowOfflineRenderers,
		kCGLPFASupportsAutomaticGraphicsSwitching,
		NSOpenGLPFADoubleBuffer,
		NSOpenGLPFAColorSize,
		32,
		0
	};
	
	NSOpenGLPixelFormat* PixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes: Attributes];
	if (PixelFormat)
	{
		NSOpenGLContext* Context = [[NSOpenGLContext alloc] initWithFormat: PixelFormat shareContext: nil];
		
		if (Context)
		{
			NSOpenGLContext* PreviousContext = [NSOpenGLContext currentContext];
			[Context makeCurrentContext];
			
			for(uint32 i = 0; i < [PixelFormat numberOfVirtualScreens]; i++)
			{
				[Context setCurrentVirtualScreen: i];
				
				GLint RendererID = 0;
				[Context getValues:&RendererID forParameter:NSOpenGLCPCurrentRendererID];
				
				FString Gpu(ANSI_TO_TCHAR((const ANSICHAR*)glGetString(GL_RENDERER)));
				
				FString RendererDesc = FString::Printf(TEXT("%d: %s"), i, *Gpu);
				
				Renderers.Add(MakeShareable(new FRendererItem(FText::FromString(RendererDesc), RendererID)));
			}
			
			if ( PreviousContext )
			{
				[PreviousContext makeCurrentContext];
			}
			else
			{
				[NSOpenGLContext clearCurrentContext];
			}
			
			[Context release];
		}
		[PixelFormat release];
	}
}

SMacGraphicsSwitchingWidget::~SMacGraphicsSwitchingWidget()
{
	
}

void SMacGraphicsSwitchingWidget::Construct(SMacGraphicsSwitchingWidget::FArguments const& InArgs)
{
	bLiveSwitching = InArgs._bLiveSwitching;
	
	if ( bLiveSwitching )
	{
		ChildSlot
		[
			SNew(SComboBox< TSharedPtr<FRendererItem>>)
			.ToolTipText(LOCTEXT("PreferredRendererToolTip", "Choose the preferred rendering device."))
			.ComboBoxStyle(FMacGraphicsSwitchingStyle::Get(), "MacGraphicsSwitcher.ComboBox")
			.ForegroundColor(FCoreStyle::Get().GetSlateColor("DefaultForeground"))
			.OptionsSource(&Renderers)
			.OnSelectionChanged(this, &SMacGraphicsSwitchingWidget::OnSelectionChanged, InArgs._PreferredRendererPropertyHandle)
			.ContentPadding(2)
			.OnGenerateWidget(this, &SMacGraphicsSwitchingWidget::OnGenerateWidget)
			.Content()
			[
				SNew(STextBlock)
				.Text(this, &SMacGraphicsSwitchingWidget::GetRendererText)
				.Font( IDetailLayoutBuilder::GetDetailFont() )
			]
		];
	}
	else
	{
		ChildSlot
		[
			SNew(SComboBox< TSharedPtr<FRendererItem>>)
			.ToolTipText(LOCTEXT("PreferredRendererToolTip", "Choose the preferred rendering device."))
			.OptionsSource(&Renderers)
			.OnSelectionChanged(this, &SMacGraphicsSwitchingWidget::OnSelectionChanged, InArgs._PreferredRendererPropertyHandle)
			.ContentPadding(2)
			.OnGenerateWidget(this, &SMacGraphicsSwitchingWidget::OnGenerateWidget)
			.Content()
			[
				SNew(STextBlock)
				.Text(this, &SMacGraphicsSwitchingWidget::GetRendererText)
				.Font( IDetailLayoutBuilder::GetDetailFont() )
			]
		];
	}
}

TSharedRef<SWidget> SMacGraphicsSwitchingWidget::OnGenerateWidget( TSharedPtr<FRendererItem> InItem )
{
	return SNew(STextBlock)
		.Text(InItem->Text);
}

void SMacGraphicsSwitchingWidget::OnSelectionChanged(TSharedPtr<FRendererItem> InItem, ESelectInfo::Type InSeletionInfo, TSharedPtr<IPropertyHandle> PreferredRendererPropertyHandle)
{
	if ( PreferredRendererPropertyHandle.IsValid() )
	{
		PreferredRendererPropertyHandle->SetValue(InItem->RendererID);
	}
	else if ( bLiveSwitching )
	{
		GConfig->SetInt(TEXT("/Script/MacGraphicsSwitching.MacGraphicsSwitchingSettings"), TEXT("RendererID"), InItem->RendererID, GEditorSettingsIni);
	}
	
	// Update all the contexts
	static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Mac.ExplicitRendererID"));
	if ( CVar )
	{
		CVar->Set( (int32)InItem->RendererID );
	}
}

FText SMacGraphicsSwitchingWidget::GetRendererText() const
{
	FText Text = FText::FromString(FString::Printf(TEXT("System Default: %s"), *FPlatformMisc::GetPrimaryGPUBrand()));

	int32 ExplicitRendererId = 0;
	GConfig->GetInt(TEXT("/Script/MacGraphicsSwitching.MacGraphicsSwitchingSettings"), TEXT("RendererID"), ExplicitRendererId, GEngineIni);
	
	if ( ExplicitRendererId )
	{
		for (auto Renderer: Renderers)
		{
			if ( Renderer->RendererID == ExplicitRendererId )
			{
				Text = Renderer->Text;
				break;
			}
		}
	}
	
	return Text;
}

#undef LOCTEXT_NAMESPACE