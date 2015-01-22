// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "AssetRegistryModule.h"
#include "Engine/TextureRenderTarget.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_TextureRenderTarget::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	FAssetTypeActions_Texture::GetActions(InObjects, MenuBuilder);

	auto RenderTargets = GetTypedWeakObjectPtrs<UTextureRenderTarget>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("TextureRenderTarget_CreateStatic", "Create Static Texture"),
		LOCTEXT("TextureRenderTarget_CreateStaticTooltip", "Creates a static texture from the selected render targets."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.Texture2D"),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_TextureRenderTarget::ExecuteCreateStatic, RenderTargets ),
			FCanExecuteAction()
			)
		);
}

void FAssetTypeActions_TextureRenderTarget::ExecuteCreateStatic(TArray<TWeakObjectPtr<UTextureRenderTarget>> Objects)
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if ( Object )
		{
			FString Name;
			FString PackageName;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), TEXT("_Tex"), PackageName, Name);

			UObject* NewObj = NULL;
			UTextureRenderTarget2D* TexRT = Cast<UTextureRenderTarget2D>(Object);
			UTextureRenderTargetCube* TexRTCube = Cast<UTextureRenderTargetCube>(Object);
			if( TexRTCube )
			{
				// create a static cube texture as well as its 6 faces
				NewObj = TexRTCube->ConstructTextureCube( CreatePackage(NULL,*PackageName), Name, Object->GetMaskedFlags() );
			}
			else if( TexRT )
			{
				// create a static 2d texture
				NewObj = TexRT->ConstructTexture2D( CreatePackage(NULL,*PackageName), Name, Object->GetMaskedFlags(), CTF_Default, NULL );
			}

			if( NewObj )
			{
				// package needs saving
				NewObj->MarkPackageDirty();

				// Notify the asset registry
				FAssetRegistryModule::AssetCreated(NewObj);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE