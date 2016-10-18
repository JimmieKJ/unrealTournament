// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "MessageLog.h"
#include "UObjectToken.h"
#include "Runtime/Engine/Classes/Engine/TextureRenderTarget2D.h"
#include "Runtime/Engine/Classes/Engine/CanvasRenderTarget2D.h"
#include "ImageUtils.h"
#include "FileManagerGeneric.h"
#include "Paths.h"

//////////////////////////////////////////////////////////////////////////
// UKismetRenderingLibrary

#define LOCTEXT_NAMESPACE "KismetRenderingLibrary"

UKismetRenderingLibrary::UKismetRenderingLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UKismetRenderingLibrary::ClearRenderTarget2D(UObject* WorldContextObject, UTextureRenderTarget2D* TextureRenderTarget, FLinearColor ClearColor)
{
	check(WorldContextObject);
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);

	if (TextureRenderTarget
		&& TextureRenderTarget->Resource
		&& World)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			ClearRTCommand,
			FTextureRenderTargetResource*,RenderTargetResource,TextureRenderTarget->GameThread_GetRenderTargetResource(),
			FLinearColor,ClearColor,ClearColor,
		{
			SetRenderTarget(RHICmdList, RenderTargetResource->GetRenderTargetTexture(), FTextureRHIRef(), true);
			RHICmdList.Clear(true, ClearColor, false, 0.0f, false, 0, FIntRect());
		});
	}
}

UTextureRenderTarget2D* UKismetRenderingLibrary::CreateRenderTarget2D(UObject* WorldContextObject, int32 Width, int32 Height)
{
	check(WorldContextObject);
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);

	if (Width > 0 && Height > 0 && World)
	{
		UTextureRenderTarget2D* NewRenderTarget2D = NewObject<UTextureRenderTarget2D>(WorldContextObject);
		check(NewRenderTarget2D);
		NewRenderTarget2D->InitAutoFormat(Width, Height); 
		NewRenderTarget2D->UpdateResourceImmediate(true);

		return NewRenderTarget2D; 
	}

	return nullptr;
}

void UKismetRenderingLibrary::DrawMaterialToRenderTarget(UObject* WorldContextObject, UTextureRenderTarget2D* TextureRenderTarget, UMaterialInterface* Material)
{
	check(WorldContextObject);
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, false);

	if (TextureRenderTarget 
		&& TextureRenderTarget->Resource 
		&& World 
		&& Material)
	{
		UCanvas* Canvas = World->GetCanvasForDrawMaterialToRenderTarget();

		Canvas->Init(TextureRenderTarget->SizeX, TextureRenderTarget->SizeY, nullptr);
		Canvas->Update();

		FCanvas RenderCanvas(
			TextureRenderTarget->GameThread_GetRenderTargetResource(), 
			nullptr, 
			World,
			World->FeatureLevel);

		Canvas->Canvas = &RenderCanvas;

		TDrawEvent<FRHICommandList>* DrawMaterialToTargetEvent = new TDrawEvent<FRHICommandList>();

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			BeginDrawEventCommand,
			FName,RTName,TextureRenderTarget->GetFName(),
			TDrawEvent<FRHICommandList>*,DrawMaterialToTargetEvent,DrawMaterialToTargetEvent,
		{
			BEGIN_DRAW_EVENTF(
				RHICmdList, 
				DrawCanvasToTarget, 
				(*DrawMaterialToTargetEvent), 
				*RTName.ToString());
		});

		Canvas->K2_DrawMaterial(Material, FVector2D(0, 0), FVector2D(TextureRenderTarget->SizeX, TextureRenderTarget->SizeY), FVector2D(0, 0));

		RenderCanvas.Flush_GameThread();
		Canvas->Canvas = NULL;

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			CanvasRenderTargetResolveCommand, FTextureRenderTargetResource*, RenderTargetResource, TextureRenderTarget->GameThread_GetRenderTargetResource(),
			TDrawEvent<FRHICommandList>*,DrawMaterialToTargetEvent,DrawMaterialToTargetEvent,
			{
				RHICmdList.CopyToResolveTarget(RenderTargetResource->GetRenderTargetTexture(), RenderTargetResource->TextureRHI, true, FResolveParams());
				STOP_DRAW_EVENT((*DrawMaterialToTargetEvent));
				delete DrawMaterialToTargetEvent;
			}
		);
	}
	else if (!World)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("DrawMaterialToRenderTarget_InvalidWorldContextObject", "DrawMaterialToRenderTarget: WorldContextObject is not valid."));
	}
	else if (!Material)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("DrawMaterialToRenderTarget_InvalidMaterial", "DrawMaterialToRenderTarget: Material must be non-null."));
	}
	else if (!TextureRenderTarget)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("DrawMaterialToRenderTarget_InvalidTextureRenderTarget", "DrawMaterialToRenderTarget: TextureRenderTarget must be non-null."));
	}
}

void UKismetRenderingLibrary::ExportRenderTarget(UObject* WorldContextObject, UTextureRenderTarget2D* TextureRenderTarget, const FString& FilePath, const FString& FileName)
{
	FString TotalFileName = FPaths::Combine(*FilePath, *FileName);
	FText PathError;
	FPaths::ValidatePath(TotalFileName, &PathError);

	if (TextureRenderTarget && !FileName.IsEmpty() && PathError.IsEmpty())
	{
		FArchive* Ar = IFileManager::Get().CreateFileWriter(*TotalFileName);

		if (Ar)
		{
			FBufferArchive Buffer;
			bool bSuccess = FImageUtils::ExportRenderTarget2DAsHDR(TextureRenderTarget, Buffer);

			if (bSuccess)
			{
				Ar->Serialize(const_cast<uint8*>(Buffer.GetData()), Buffer.Num());
			}

			delete Ar;
		}
		else
		{
			FMessageLog("Blueprint").Warning(LOCTEXT("ExportRenderTarget_FileWriterFailedToCreate", "ExportRenderTarget: FileWrite failed to create."));
		}
	}

    else if (!TextureRenderTarget)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("ExportRenderTarget_InvalidTextureRenderTarget", "ExportRenderTarget: TextureRenderTarget must be non-null."));
	}
	if (!PathError.IsEmpty())
	{
		FMessageLog("Blueprint").Warning(FText::Format(LOCTEXT("ExportRenderTarget_InvalidFilePath", "ExportRenderTarget: Invalid file path provided: '{0}'"), PathError));
	}
	if (FileName.IsEmpty())
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("ExportRenderTarget_InvalidFileName", "ExportRenderTarget: FileName must be non-empty."));
	}
}

void UKismetRenderingLibrary::ExportTexture2D(UObject* WorldContextObject, UTexture2D* Texture, const FString& FilePath, const FString& FileName)
{
	FString TotalFileName = FPaths::Combine(*FilePath, *FileName);
	FText PathError;
	FPaths::ValidatePath(TotalFileName, &PathError);

	if (Texture && !FileName.IsEmpty() && PathError.IsEmpty())
	{
		FArchive* Ar = IFileManager::Get().CreateFileWriter(*TotalFileName);

		if (Ar)
		{
			FBufferArchive Buffer;
			bool bSuccess = FImageUtils::ExportTexture2DAsHDR(Texture, Buffer);

			if (bSuccess)
			{
				Ar->Serialize(const_cast<uint8*>(Buffer.GetData()), Buffer.Num());
			}

			delete Ar;
		}
		else
		{
			FMessageLog("Blueprint").Warning(LOCTEXT("ExportTexture2D_FileWriterFailedToCreate", "ExportTexture2D: FileWrite failed to create."));
		}
	}

	else if (!Texture)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("ExportTexture2D_InvalidTextureRenderTarget", "ExportTexture2D: TextureRenderTarget must be non-null."));
	}
	if (!PathError.IsEmpty())
	{
		FMessageLog("Blueprint").Warning(FText::Format(LOCTEXT("ExportTexture2D_InvalidFilePath", "ExportTexture2D: Invalid file path provided: '{0}'"), PathError));
	}
	if (FileName.IsEmpty())
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("ExportTexture2D_InvalidFileName", "ExportTexture2D: FileName must be non-empty."));
	}
}

void UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(UObject* WorldContextObject, UTextureRenderTarget2D* TextureRenderTarget, UCanvas*& Canvas, FVector2D& Size, FDrawToRenderTargetContext& Context)
{
	check(WorldContextObject);
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, false);

	Canvas = NULL;
	Size = FVector2D(0, 0);
	Context = FDrawToRenderTargetContext();

	if (TextureRenderTarget 
		&& TextureRenderTarget->Resource 
		&& World)
	{
		Context.RenderTarget = TextureRenderTarget;

		Canvas = World->GetCanvasForRenderingToTarget();

		Size = FVector2D(TextureRenderTarget->SizeX, TextureRenderTarget->SizeY);

		Canvas->Init(TextureRenderTarget->SizeX, TextureRenderTarget->SizeY, nullptr);
		Canvas->Update();

		Canvas->Canvas = new FCanvas(
			TextureRenderTarget->GameThread_GetRenderTargetResource(), 
			nullptr, 
			World,
			World->FeatureLevel, 
			// Draw immediately so that interleaved SetVectorParameter (etc) function calls work as expected
			FCanvas::CDM_ImmediateDrawing);

		Context.DrawEvent = new TDrawEvent<FRHICommandList>();

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			BeginDrawEventCommand,
			FName,RTName,TextureRenderTarget->GetFName(),
			TDrawEvent<FRHICommandList>*,DrawEvent,Context.DrawEvent,
		{
			BEGIN_DRAW_EVENTF(
				RHICmdList, 
				DrawCanvasToTarget, 
				(*DrawEvent), 
				*RTName.ToString());
		});
	}
	else if (!World)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("BeginDrawCanvasToRenderTarget_InvalidWorldContextObject", "BeginDrawCanvasToRenderTarget: WorldContextObject is not valid."));
	}
	else if (!TextureRenderTarget)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("BeginDrawCanvasToRenderTarget_InvalidTextureRenderTarget", "BeginDrawCanvasToRenderTarget: TextureRenderTarget must be non-null."));
	}
}

void UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(UObject* WorldContextObject, const FDrawToRenderTargetContext& Context)
{
	check(WorldContextObject);
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, false);

	if (World)
	{
		UCanvas* WorldCanvas = World->GetCanvasForRenderingToTarget();

		if (WorldCanvas->Canvas)
		{
			WorldCanvas->Canvas->Flush_GameThread();
			delete WorldCanvas->Canvas;
			WorldCanvas->Canvas = nullptr;
		}
		
		if (Context.RenderTarget)
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
				CanvasRenderTargetResolveCommand, FTextureRenderTargetResource*, RenderTargetResource, Context.RenderTarget->GameThread_GetRenderTargetResource(),
				TDrawEvent<FRHICommandList>*,DrawEvent,Context.DrawEvent,
				{
					RHICmdList.CopyToResolveTarget(RenderTargetResource->GetRenderTargetTexture(), RenderTargetResource->TextureRHI, true, FResolveParams());
					STOP_DRAW_EVENT((*DrawEvent));
					delete DrawEvent;
				}
			);

			// Remove references to the context now that we've resolved it, to avoid a crash when EndDrawCanvasToRenderTarget is called multiple times with the same context
			// const cast required, as BP will treat Context as an output without the const
			const_cast<FDrawToRenderTargetContext&>(Context) = FDrawToRenderTargetContext();
		}
		else if (!World)
		{
			FMessageLog("Blueprint").Warning(LOCTEXT("EndDrawCanvasToRenderTarget_InvalidWorldContextObject", "EndDrawCanvasToRenderTarget: WorldContextObject is not valid."));
		}
		else if (!Context.RenderTarget)
		{
			FMessageLog("Blueprint").Warning(LOCTEXT("EndDrawCanvasToRenderTarget_InvalidContext", "EndDrawCanvasToRenderTarget: Context must be valid."));
		}
	}
}

#undef LOCTEXT_NAMESPACE