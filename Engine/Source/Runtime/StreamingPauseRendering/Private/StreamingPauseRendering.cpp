// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Engine.h"
#include "StreamingPauseRendering.h"
#include "SceneViewport.h"
#include "MoviePlayer.h"
#include "SThrobber.h"


IMPLEMENT_MODULE(FStreamingPauseRenderingModule, StreamingPauseRendering);


static TAutoConsoleVariable<int32> CVarRenderLastFrameInStreamingPause(
	TEXT("r.RenderLastFrameInStreamingPause"),
	1,
	TEXT("If 1 the previous frame is displayed during streaming pause. If zero the screen is left black."),
	ECVF_RenderThreadSafe);


FStreamingPauseRenderingModule::FStreamingPauseRenderingModule()
	: Viewport(nullptr)
	, ViewportWidget(nullptr)
	, ViewportClient(nullptr)
{ }


void FStreamingPauseRenderingModule::StartupModule()
{
	BeginDelegate.BindRaw(this, &FStreamingPauseRenderingModule::BeginStreamingPause);
	EndDelegate.BindRaw(this, &FStreamingPauseRenderingModule::EndStreamingPause);
	check(GEngine);
	GEngine->RegisterBeginStreamingPauseRenderingDelegate(&BeginDelegate);
	GEngine->RegisterEndStreamingPauseRenderingDelegate(&EndDelegate);
}


void FStreamingPauseRenderingModule::ShutdownModule()
{
	BeginDelegate.Unbind();
	EndDelegate.Unbind();
	if( GEngine )
	{
		GEngine->RegisterBeginStreamingPauseRenderingDelegate(nullptr);
		GEngine->RegisterEndStreamingPauseRenderingDelegate(nullptr);
	}
}


void FStreamingPauseRenderingModule::BeginStreamingPause( FViewport* GameViewport )
{
	check(GameViewport);

	//Create the viewport widget and add a throbber.
	ViewportWidget = SNew( SViewport )
		.EnableGammaCorrection(false);

	ViewportWidget->SetContent
		(
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.VAlign(VAlign_Bottom)
		.HAlign(HAlign_Right)
		.Padding(FMargin(10.0f))
		[
			SNew(SThrobber)
		]
	);

	//Render the current scene to a new viewport.
	FIntPoint SizeXY = GameViewport->GetSizeXY();
	if (SizeXY.X > 0 && SizeXY.Y > 0 && CVarRenderLastFrameInStreamingPause.GetValueOnGameThread() != 0)
	{
		ViewportClient = GameViewport->GetClient();

		Viewport = MakeShareable(new FSceneViewport(ViewportClient, ViewportWidget));

		ViewportWidget->SetViewportInterface(Viewport.ToSharedRef());
	
		Viewport->UpdateViewportRHI(false,SizeXY.X,SizeXY.Y, EWindowMode::Fullscreen);

		Viewport->EnqueueBeginRenderFrame();

		FCanvas Canvas(Viewport.Get(), nullptr, ViewportClient->GetWorld(), ViewportClient->GetWorld()->FeatureLevel);
		{
			ViewportClient->Draw(Viewport.Get(), &Canvas);
		}
		Canvas.Flush_GameThread();

		//Don't need debug canvas I presume?
		//Viewport->GetDebugCanvas()->Flush(true);

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			EndDrawingCommand,
			FViewport*,Viewport,Viewport.Get(),
			//FIntPoint,InRestoreSize,RestoreSize,
		{
			Viewport->EndRenderFrame(RHICmdList, false, false);
		});
	}

	//Create the loading screen and begin rendering the widget.
	FLoadingScreenAttributes LoadingScreen;
	LoadingScreen.bAutoCompleteWhenLoadingCompletes = true; 
	LoadingScreen.WidgetLoadingScreen = ViewportWidget; // SViewport from above
	GetMoviePlayer()->SetupLoadingScreen(LoadingScreen);			
	GetMoviePlayer()->PlayMovie();
}


void FStreamingPauseRenderingModule::EndStreamingPause()
{
	//Stop rendering the loading screen and resume
	GetMoviePlayer()->WaitForMovieToFinish();

	ViewportWidget = nullptr;
	Viewport = nullptr;
	ViewportClient = nullptr;
	
	FlushRenderingCommands();
}