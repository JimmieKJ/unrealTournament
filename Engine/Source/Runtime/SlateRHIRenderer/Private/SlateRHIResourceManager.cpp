// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateRHIRendererPrivatePCH.h"
#include "ImageWrapper.h"
#include "SlateNativeTextureResource.h"
#include "SlateUTextureResource.h"
#include "SlateMaterialResource.h"
#include "ImageUtils.h"

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Texture Atlases"), STAT_SlateNumTextureAtlases, STATGROUP_SlateMemory);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Non-Atlased Textures"), STAT_SlateNumNonAtlasedTextures, STATGROUP_SlateMemory);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Dynamic Textures"), STAT_SlateNumDynamicTextures, STATGROUP_SlateMemory);

TSharedPtr<FSlateDynamicTextureResource> FDynamicResourceMap::GetDynamicTextureResource( FName ResourceName ) const
{
	return NativeTextureMap.FindRef( ResourceName );
}

TSharedPtr<FSlateUTextureResource> FDynamicResourceMap::GetUTextureResource( UTexture2D* TextureObject ) const
{
	if(TextureObject)
	{
		return UTextureResourceMap.FindRef(TextureObject);
	}

	return nullptr;
}

TSharedPtr<FSlateMaterialResource> FDynamicResourceMap::GetMaterialResource( UMaterialInterface* Material ) const
{
	return MaterialResourceMap.FindRef( Material );
}


void FDynamicResourceMap::AddDynamicTextureResource( FName ResourceName, TSharedRef<FSlateDynamicTextureResource> InResource )
{
	NativeTextureMap.Add( ResourceName, InResource );
}

void FDynamicResourceMap::AddUTextureResource( UTexture2D* TextureObject, TSharedRef<FSlateUTextureResource> InResource)
{
	if(TextureObject)
	{
		check(TextureObject == InResource->TextureObject);
		UTextureResourceMap.Add(TextureObject, InResource);
	}
}

void FDynamicResourceMap::AddMaterialResource( UMaterialInterface* Material, TSharedRef<FSlateMaterialResource> InMaterialResource )
{
	check( Material == InMaterialResource->GetMaterialObject() );
	MaterialResourceMap.Add( Material, InMaterialResource );
}

void FDynamicResourceMap::RemoveDynamicTextureResource(FName ResourceName)
{
	NativeTextureMap.Remove(ResourceName);
}

void FDynamicResourceMap::RemoveUTextureResource( UTexture2D* TextureObject )
{
	if(TextureObject)
	{
		UTextureResourceMap.Remove(TextureObject);
	}
}


void FDynamicResourceMap::RemoveMaterialResource( UMaterialInterface* Material )
{
	MaterialResourceMap.Remove( Material );
}

void FDynamicResourceMap::Empty()
{
	EmptyUTextureResources();
	EmptyMaterialResources();
	EmptyDynamicTextureResources();
}

void FDynamicResourceMap::EmptyDynamicTextureResources()
{
	NativeTextureMap.Empty();
}

void FDynamicResourceMap::EmptyUTextureResources()
{
	UTextureResourceMap.Empty();
}

void FDynamicResourceMap::EmptyMaterialResources()
{
	MaterialResourceMap.Empty();
}


void FDynamicResourceMap::ReleaseResources()
{
	for (TMap<FName, TSharedPtr<FSlateDynamicTextureResource> >::TIterator It(NativeTextureMap); It; ++It)
	{
		BeginReleaseResource(It.Value()->RHIRefTexture);
	}
	
	for (TMap<TWeakObjectPtr<UTexture2D>, TSharedPtr<FSlateUTextureResource> >::TIterator It(UTextureResourceMap); It; ++It)
	{
		It.Value()->UpdateRenderResource(nullptr);
	}

}


FSlateRHIResourceManager::FSlateRHIResourceManager()
	: BadResourceTexture(nullptr)
{
	FCoreDelegates::OnPreExit.AddRaw( this, &FSlateRHIResourceManager::OnAppExit );

	MaxAltasedTextureSize = FIntPoint(256,256);
	if( GIsEditor )
	{
		AtlasSize = 2048;
	}
	else
	{
		AtlasSize = 1024;
		if( GConfig )
		{
			int32 RequestedSize = 1024;
			GConfig->GetInt( TEXT("SlateRenderer"), TEXT("TextureAtlasSize"), RequestedSize, GEngineIni );
			AtlasSize = FMath::Clamp<uint32>( RequestedSize, 0, 2048 );

			int32 MaxAtlasedTextureWidth = 256;
			int32 MaxAtlasedTextureHeight = 256;
			GConfig->GetInt( TEXT("SlateRenderer"), TEXT("MaxAtlasedTextureWidth"), MaxAtlasedTextureWidth, GEngineIni );
			GConfig->GetInt( TEXT("SlateRenderer"), TEXT("MaxAtlasedTextureHeight"),MaxAtlasedTextureHeight, GEngineIni );

			// Max texture size cannot be larger than the max size of the atlas
			MaxAltasedTextureSize.X = FMath::Clamp<int32>( MaxAtlasedTextureWidth, 0, AtlasSize );
			MaxAltasedTextureSize.Y = FMath::Clamp<int32>( MaxAtlasedTextureHeight, 0, AtlasSize );
		}
	}
}

FSlateRHIResourceManager::~FSlateRHIResourceManager()
{
	FCoreDelegates::OnPreExit.RemoveAll( this );

	if ( GIsRHIInitialized )
	{
		DeleteResources();
	}
}

int32 FSlateRHIResourceManager::GetNumAtlasPages() const
{
	return TextureAtlases.Num();
}

FIntPoint FSlateRHIResourceManager::GetAtlasPageSize() const
{
	return FIntPoint(1024, 1024);
}

FSlateShaderResource* FSlateRHIResourceManager::GetAtlasPageResource(const int32 InIndex) const
{
	return TextureAtlases[InIndex]->GetAtlasTexture();
}

bool FSlateRHIResourceManager::IsAtlasPageResourceAlphaOnly() const
{
	return false;
}

void FSlateRHIResourceManager::CreateTextures( const TArray< const FSlateBrush* >& Resources )
{
	TMap<FName,FNewTextureInfo> TextureInfoMap;

	const uint32 Stride = GPixelFormats[PF_R8G8B8A8].BlockBytes;
	for( int32 ResourceIndex = 0; ResourceIndex < Resources.Num(); ++ResourceIndex )
	{
		const FSlateBrush& Brush = *Resources[ResourceIndex];
		const FName TextureName = Brush.GetResourceName();
		if( TextureName != NAME_None && !Brush.HasUObject() && !Brush.IsDynamicallyLoaded() && !ResourceMap.Contains(TextureName) )
		{
			// Find the texture or add it if it doesnt exist (only load the texture once)
			FNewTextureInfo& Info = TextureInfoMap.FindOrAdd( TextureName );
	
			Info.bSrgb = (Brush.ImageType != ESlateBrushImageType::Linear);

			// Only atlas the texture if none of the brushes that use it tile it and the image is srgb
		
			Info.bShouldAtlas &= ( Brush.Tiling == ESlateBrushTileType::NoTile && Info.bSrgb && AtlasSize > 0 );

			// Texture has been loaded if the texture data is valid
			if( !Info.TextureData.IsValid() )
			{
				uint32 Width = 0;
				uint32 Height = 0;
				TArray<uint8> RawData;
				bool bSucceeded = LoadTexture( Brush, Width, Height, RawData );

				Info.TextureData = MakeShareable( new FSlateTextureData( Width, Height, Stride, RawData ) );

				const bool bTooLargeForAtlas = (Width >= (uint32)MaxAltasedTextureSize.X || Height >= (uint32)MaxAltasedTextureSize.Y || Width >= AtlasSize || Height >= AtlasSize );

				Info.bShouldAtlas &= !bTooLargeForAtlas;

				if( !bSucceeded || !ensureMsgf( Info.TextureData->GetRawBytes().Num() > 0, TEXT("Slate resource: (%s) contains no data"), *TextureName.ToString() ) )
				{
					TextureInfoMap.Remove( TextureName );
				}
			}
		}
	}

	// Sort textures by size.  The largest textures are atlased first which creates a more compact atlas
	TextureInfoMap.ValueSort( FCompareFNewTextureInfoByTextureSize() );

	for( TMap<FName,FNewTextureInfo>::TConstIterator It(TextureInfoMap); It; ++It )
	{
		const FNewTextureInfo& Info = It.Value();
		FName TextureName = It.Key();
		FString NameStr = TextureName.ToString();

		checkSlow( TextureName != NAME_None );

		FSlateShaderResourceProxy* NewTexture = GenerateTextureResource( Info );

		ResourceMap.Add( TextureName, NewTexture );
	}
}

bool FSlateRHIResourceManager::LoadTexture( const FSlateBrush& InBrush, uint32& Width, uint32& Height, TArray<uint8>& DecodedImage )
{
	FString ResourcePath = GetResourcePath( InBrush );

	return LoadTexture(InBrush.GetResourceName(), ResourcePath, Width, Height, DecodedImage);
}

/** 
 * Loads a UTexture2D from a package and stores it in the cache
 *
 * @param TextureName	The name of the texture to load
 */
bool FSlateRHIResourceManager::LoadTexture( const FName& TextureName, const FString& ResourcePath, uint32& Width, uint32& Height, TArray<uint8>& DecodedImage )
{
	check( IsThreadSafeForSlateRendering() );

	bool bSucceeded = true;
	uint32 BytesPerPixel = 4;

	TArray<uint8> RawFileData;
	if( FFileHelper::LoadFileToArray( RawFileData, *ResourcePath ) )
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName("ImageWrapper") );
		IImageWrapperPtr ImageWrapper = ImageWrapperModule.CreateImageWrapper( EImageFormat::PNG );
		if ( ImageWrapper.IsValid() && ImageWrapper->SetCompressed( RawFileData.GetData(), RawFileData.Num() ) )
		{
			Width = ImageWrapper->GetWidth();
			Height = ImageWrapper->GetHeight();
			
			const TArray<uint8>* RawData = NULL;
			if (ImageWrapper->GetRaw( ERGBFormat::BGRA, 8, RawData))
			{
				DecodedImage.AddUninitialized( Width*Height*BytesPerPixel );
				DecodedImage = *RawData;
			}
			else
			{
				UE_LOG(LogSlate, Log, TEXT("Invalid texture format for Slate resource only RGBA and RGB pngs are supported: %s"), *TextureName.ToString() );
				bSucceeded = false;
			}
		}
		else
		{
			UE_LOG(LogSlate, Log, TEXT("Only pngs are supported in Slate"));
			bSucceeded = false;
		}
	}
	else
	{
		UE_LOG(LogSlate, Log, TEXT("Could not find file for Slate resource: %s"), *TextureName.ToString() );
		bSucceeded = false;
	}

	return bSucceeded;
}

FSlateShaderResourceProxy* FSlateRHIResourceManager::GenerateTextureResource( const FNewTextureInfo& Info )
{
	FSlateShaderResourceProxy* NewProxy = NULL;
	const uint32 Width = Info.TextureData->GetWidth();
	const uint32 Height = Info.TextureData->GetHeight();

	if( Info.bShouldAtlas )
	{
		const FAtlasedTextureSlot* NewSlot = NULL;
		FSlateTextureAtlasRHI* Atlas = NULL;

		// See if any atlases can hold the texture
		for( int32 AtlasIndex = 0; AtlasIndex < TextureAtlases.Num() && !NewSlot; ++AtlasIndex )
		{
			Atlas = TextureAtlases[AtlasIndex];
			NewSlot = Atlas->AddTexture( Width, Height, Info.TextureData->GetRawBytes() );
		}

		if( !NewSlot )
		{
			INC_DWORD_STAT_BY(STAT_SlateNumTextureAtlases, 1);

			Atlas = new FSlateTextureAtlasRHI( AtlasSize, AtlasSize, ESlateTextureAtlasPaddingStyle::DilateBorder );
			TextureAtlases.Add( Atlas );
			NewSlot = TextureAtlases.Last()->AddTexture( Width, Height, Info.TextureData->GetRawBytes() );
		}
		
		check( Atlas && NewSlot );

		// Create a proxy to the atlased texture. The texture being used is the atlas itself with sub uvs to access the correct texture
		NewProxy = new FSlateShaderResourceProxy;
		NewProxy->Resource = Atlas->GetAtlasTexture();
		const uint32 Padding = NewSlot->Padding;
		NewProxy->StartUV = FVector2D((float)(NewSlot->X + Padding) / Atlas->GetWidth(), (float)(NewSlot->Y + Padding) / Atlas->GetHeight());
		NewProxy->SizeUV = FVector2D( (float)(NewSlot->Width-Padding*2) / Atlas->GetWidth(), (float)(NewSlot->Height-Padding*2) / Atlas->GetHeight() );
		NewProxy->ActualSize = FIntPoint( Width, Height );
	}
	else
	{
		NewProxy = new FSlateShaderResourceProxy;

		// Create a new standalone texture because we can't atlas this one
		FSlateTexture2DRHIRef* Texture = new FSlateTexture2DRHIRef( Width, Height, PF_B8G8R8A8, Info.TextureData, Info.bSrgb ? TexCreate_SRGB : TexCreate_None );
		// Add it to the list of non atlased textures that we must clean up later
		NonAtlasedTextures.Add( Texture );

		INC_DWORD_STAT_BY( STAT_SlateNumNonAtlasedTextures, 1 );

		BeginInitResource( Texture );

		// The texture proxy only contains a single texture
		NewProxy->Resource = Texture;
		NewProxy->StartUV = FVector2D(0.0f, 0.0f);
		NewProxy->SizeUV = FVector2D(1.0f, 1.0f);
		NewProxy->ActualSize = FIntPoint( Width, Height );
	}

	return NewProxy;
}

static void LoadUObjectForBrush( const FSlateBrush& InBrush )
{
	// Load the utexture
	FString Path = InBrush.GetResourceName().ToString();

	if (!Path.IsEmpty() && Path.StartsWith(FSlateBrush::UTextureIdentifier()))
	{
		FString NewPath = Path.RightChop(FSlateBrush::UTextureIdentifier().Len());
		UObject* TextureObject = LoadObject<UTexture2D>(NULL, *NewPath, NULL, LOAD_None, NULL);
		FSlateBrush* Brush = const_cast<FSlateBrush*>(&InBrush);

		// Set the texture object to a default texture to prevent constant loading of missing textures
		if( !TextureObject )
		{
			UE_LOG(LogSlate, Warning, TEXT("Error loading loading UTexture from path: %s not found"), *Path);
			TextureObject = GEngine->DefaultTexture;
		}
		else
		{
			// We do this here because this deprecated system of loading textures will not report references and we dont want the Slate RHI resource manager to manage references
			TextureObject->AddToRoot();
		}

		
		Brush->SetResourceObject(TextureObject);

		UE_LOG(LogSlate, Warning, TEXT("The texture:// method of loading UTextures for use in Slate is deprecated.  Please convert %s to a Brush Asset"), *Path);
	}
}

FSlateShaderResourceProxy* FSlateRHIResourceManager::GetShaderResource( const FSlateBrush& InBrush )
{
	check( IsThreadSafeForSlateRendering() );

	FSlateShaderResourceProxy* Texture = NULL;
	if( !InBrush.IsDynamicallyLoaded() && !InBrush.HasUObject() )
	{
		Texture = ResourceMap.FindRef( InBrush.GetResourceName() );
	}
	else if (InBrush.GetResourceObject() && InBrush.GetResourceObject()->IsA<UMaterialInterface>())
	{
		Texture = GetMaterialResource(InBrush);
	}
	else if( InBrush.IsDynamicallyLoaded() || ( InBrush.HasUObject() ) )
	{
		if( InBrush.HasUObject() && InBrush.GetResourceObject() == nullptr )
		{
			// Hack for loading via the deprecated path
			LoadUObjectForBrush( InBrush );
		}

		Texture = FindOrCreateDynamicTextureResource( InBrush );
	}

	return Texture;
}

ISlateAtlasProvider* FSlateRHIResourceManager::GetTextureAtlasProvider()
{
	return this;
}

TSharedPtr<FSlateDynamicTextureResource> FSlateRHIResourceManager::MakeDynamicTextureResource( FName ResourceName, uint32 Width, uint32 Height, const TArray< uint8 >& Bytes )
{
	// Make storage for the image
	FSlateTextureDataRef TextureStorage = MakeShareable( new FSlateTextureData( Width, Height, GPixelFormats[PF_B8G8R8A8].BlockBytes, Bytes ) );

	TSharedPtr<FSlateDynamicTextureResource> TextureResource;
	// Get a resource from the free list if possible
	if(DynamicTextureFreeList.Num() > 0)
	{
		TextureResource = DynamicTextureFreeList.Pop(/*bAllowShrinking=*/ false);
	}
	else
	{
		// Free list is empty, we have to allocate a new resource
		TextureResource = MakeShareable(new FSlateDynamicTextureResource(nullptr));
	}

	TextureResource->Proxy->ActualSize = FIntPoint(TextureStorage->GetWidth(), TextureStorage->GetHeight());


	// Init render thread data
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(InitNewSlateDynamicTextureResource,
		FSlateDynamicTextureResource*, TextureResource, TextureResource.Get(),
		FSlateTextureDataPtr, InNewTextureData, TextureStorage,
	{
		if(InNewTextureData.IsValid())
		{
			// Set the texture to use as the texture we just loaded
			TextureResource->RHIRefTexture->SetTextureData(InNewTextureData, PF_B8G8R8A8, TexCreate_SRGB);

		}

		// Initialize and link the rendering resource
		TextureResource->RHIRefTexture->InitResource();
	});

	// Map the new resource so we don't have to load again
	DynamicResourceMap.AddDynamicTextureResource( ResourceName, TextureResource.ToSharedRef() );
	INC_DWORD_STAT_BY(STAT_SlateNumDynamicTextures, 1);

	return TextureResource;
}

TSharedPtr<FSlateDynamicTextureResource> FSlateRHIResourceManager::GetDynamicTextureResourceByName( FName ResourceName )
{
	return DynamicResourceMap.GetDynamicTextureResource( ResourceName );
}

TSharedPtr<FSlateUTextureResource> FSlateRHIResourceManager::MakeDynamicUTextureResource(UTexture2D* InTextureObject)
{
	// Generated texture resource
	TSharedPtr<FSlateUTextureResource> TextureResource;

	// Data for a loaded disk image
	FNewTextureInfo Info;

	bool bUsingDeprecatedUTexturePath = false;

	bool bSucceeded = false;
	if( InTextureObject != NULL )
	{
		TextureResource = DynamicResourceMap.GetUTextureResource( InTextureObject );
		if( TextureResource.IsValid() )
		{
			// Bail out of the resource is already loaded
			return TextureResource;
		}

		bSucceeded = true;
	}
	
	if( bSucceeded )
	{

		// Get a resource from the free list if possible
		if (UTextureFreeList.Num() > 0)
		{
			TextureResource = UTextureFreeList.Pop(/*bAllowShrinking=*/ false);
			TextureResource->TextureObject = InTextureObject;
		}
		else
		{
			// Free list is empty, we have to allocate a new resource
			TextureResource = MakeShareable(new FSlateUTextureResource(InTextureObject));
		
		}

		TextureResource->Proxy->ActualSize = FIntPoint(InTextureObject->GetSizeX(), InTextureObject->GetSizeY());

		checkSlow(!AccessedUTextures.Contains(InTextureObject));
	}
	else
	{
		// Add the null texture so we don't continuously try to load it.
		TextureResource = FSlateUTextureResource::NullResource;
	}

	DynamicResourceMap.AddUTextureResource(InTextureObject, TextureResource.ToSharedRef());

	return TextureResource;
}


FSlateShaderResourceProxy* FSlateRHIResourceManager::FindOrCreateDynamicTextureResource(const FSlateBrush& InBrush)
{
	check( IsThreadSafeForSlateRendering() );

	const FName ResourceName = InBrush.GetResourceName();
	if ( ResourceName.IsValid() && ResourceName != NAME_None )
	{
		if ( UObject* ResourceObject = InBrush.GetResourceObject() )
		{
			if ( !ResourceObject->IsA<UTexture2D>() )
			{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				static TSet<UObject*> FailedTextures;
				if ( !FailedTextures.Contains(ResourceObject) )
				{
					FailedTextures.Add(ResourceObject);
					ensureMsgf(false, TEXT("Slate RHI Error - Invalid Texture2D '%s'."), *ResourceName.ToString());
				}
				ResourceObject = GetBadResourceTexture();
#else
				return nullptr;
#endif
			}

			UTexture2D* TextureObject = Cast<UTexture2D>(ResourceObject);

			TSharedPtr<FSlateUTextureResource> TextureResource = DynamicResourceMap.GetUTextureResource(TextureObject);

			if ( !TextureResource.IsValid() )
			{
				TextureResource = MakeDynamicUTextureResource(TextureObject);
				if ( TextureResource.IsValid() )
				{
					INC_DWORD_STAT_BY(STAT_SlateNumDynamicTextures, 1);
				}
			}

			if ( TextureResource.IsValid() )
			{
				UTexture2D* Texture2DObject = TextureResource->TextureObject;
				if ( Texture2DObject && !AccessedUTextures.Contains(Texture2DObject) && Texture2DObject->Resource )
				{
					// Set the texture rendering resource that should be used.  The UTexture resource could change at any time so we must do this each frame
					ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(UpdateSlateUTextureResource,
						FSlateUTextureResource*, InUTextureResource, TextureResource.Get(),
						{
							FTexture* RenderTexture = InUTextureResource->TextureObject->Resource;

							// Let the streaming manager know we are using this texture now
							RenderTexture->LastRenderTime = FApp::GetCurrentTime();

							// Refresh FTexture
							InUTextureResource->UpdateRenderResource(RenderTexture);

						});


					AccessedUTextures.Add(Texture2DObject);
				}

				return TextureResource->Proxy;
			}
		}
		else
		{
			TSharedPtr<FSlateDynamicTextureResource> TextureResource = DynamicResourceMap.GetDynamicTextureResource( ResourceName );

			if( !TextureResource.IsValid() )
			{
				uint32 Width; 
				uint32 Height;
				TArray<uint8> RawData;

				// Load the image from disk
				bool bSucceeded = LoadTexture(ResourceName, ResourceName.ToString(), Width, Height, RawData);
				if(bSucceeded)
				{
					TextureResource = MakeDynamicTextureResource(ResourceName, Width, Height, RawData);
				}
			}

			if(TextureResource.IsValid())
			{
				return TextureResource->Proxy;
			}
		}
	}

	// dynamic texture was not found or loaded
	return nullptr;
}

FSlateShaderResourceProxy* FSlateRHIResourceManager::GetMaterialResource(const FSlateBrush& InBrush)
{
	check(IsThreadSafeForSlateRendering());

	const FName ResourceName = InBrush.GetResourceName();

	UMaterialInterface* Material = CastChecked<UMaterialInterface>(InBrush.GetResourceObject());

	TSharedPtr<FSlateMaterialResource> MaterialResource = DynamicResourceMap.GetMaterialResource(Material);
	if (!MaterialResource.IsValid())
	{
		// Get a resource from the free list if possible
		if(MaterialResourceFreeList.Num() > 0)
		{
			MaterialResource = MaterialResourceFreeList.Pop();
		
			MaterialResource->UpdateMaterial( *Material, InBrush.ImageSize );
		}
		else
		{
			MaterialResource = MakeShareable(new FSlateMaterialResource(*Material, InBrush.ImageSize));
		}
		
		DynamicResourceMap.AddMaterialResource(Material, MaterialResource.ToSharedRef());
	}
	else
	{
		// Keep the resource up to date
		MaterialResource->UpdateRenderResource(Material->GetRenderProxy(false, false));
		MaterialResource->SlateProxy->ActualSize = InBrush.ImageSize.IntPoint();
	}

	return MaterialResource->SlateProxy;
}

void FSlateRHIResourceManager::OnAppExit()
{
	ReleaseResources();

	FlushRenderingCommands();

	DeleteResources();
}

bool FSlateRHIResourceManager::ContainsTexture( const FName& ResourceName ) const
{
	return ResourceMap.Contains( ResourceName );
}

void FSlateRHIResourceManager::ReleaseDynamicResource( const FSlateBrush& InBrush )
{
	check( IsThreadSafeForSlateRendering() )

	// Note: Only dynamically loaded or utexture brushes can be dynamically released
	if( InBrush.HasUObject() || InBrush.IsDynamicallyLoaded() )
	{
		FName ResourceName = InBrush.GetResourceName();

		UObject* ResourceObject = InBrush.GetResourceObject();

		if( ResourceObject && DynamicResourceMap.GetNumObjectResources() > 0 )
		{
			TSharedPtr<FSlateUTextureResource> TextureResource = DynamicResourceMap.GetUTextureResource(Cast<UTexture2D>(ResourceObject));

			if(TextureResource.IsValid())
			{
				//remove it from the accessed textures
				AccessedUTextures.Remove(TextureResource->TextureObject);
				DynamicResourceMap.RemoveUTextureResource(TextureResource->TextureObject);

				UTextureFreeList.Add(TextureResource);

				DEC_DWORD_STAT_BY(STAT_SlateNumDynamicTextures, 1);
			}
			else
			{
				UMaterialInterface* Material = Cast<UMaterialInterface>(ResourceObject);

				TSharedPtr<FSlateMaterialResource> MaterialResource = DynamicResourceMap.GetMaterialResource(Material);
				
				DynamicResourceMap.RemoveMaterialResource(Material);

				if (MaterialResource.IsValid())
				{
					MaterialResourceFreeList.Add( MaterialResource );
				}
			}
		
		}
		else if( !ResourceObject )
		{
			TSharedPtr<FSlateDynamicTextureResource> TextureResource = DynamicResourceMap.GetDynamicTextureResource(ResourceName);

			if( TextureResource.IsValid() )
			{
				// Release the rendering resource, its no longer being used
				BeginReleaseResource(TextureResource->RHIRefTexture);

				//remove it from the texture map
				DynamicResourceMap.RemoveDynamicTextureResource(ResourceName);

				DynamicTextureFreeList.Add( TextureResource );

				DEC_DWORD_STAT_BY(STAT_SlateNumDynamicTextures, 1);
			}
		}
		
	}
}

void FSlateRHIResourceManager::LoadUsedTextures()
{
	TArray< const FSlateBrush* > Resources;
	FSlateStyleRegistry::GetAllResources( Resources );
	 
	CreateTextures( Resources );
}

void FSlateRHIResourceManager::LoadStyleResources( const ISlateStyle& Style )
{
	TArray< const FSlateBrush* > Resources;
	Style.GetResources( Resources );

	CreateTextures( Resources );
}

void FSlateRHIResourceManager::ClearAccessedUTextures()
{
	AccessedUTextures.Empty();
}

void FSlateRHIResourceManager::UpdateTextureAtlases()
{
	for( int32 AtlasIndex = 0; AtlasIndex < TextureAtlases.Num(); ++AtlasIndex )
	{
		TextureAtlases[AtlasIndex]->ConditionalUpdateTexture();
	}
}

void FSlateRHIResourceManager::ReleaseResources()
{
	check( IsThreadSafeForSlateRendering() );

	for( int32 AtlasIndex = 0; AtlasIndex < TextureAtlases.Num(); ++AtlasIndex )
	{
		TextureAtlases[AtlasIndex]->ReleaseAtlasTexture();
	}

	for( int32 ResourceIndex = 0; ResourceIndex < NonAtlasedTextures.Num(); ++ResourceIndex )
	{
		BeginReleaseResource( NonAtlasedTextures[ResourceIndex] );
	}

	DynamicResourceMap.ReleaseResources();

	// Note the base class has texture proxies only which do not need to be released
}

void FSlateRHIResourceManager::DeleteResources()
{
	for( int32 AtlasIndex = 0; AtlasIndex < TextureAtlases.Num(); ++AtlasIndex )
	{
		delete TextureAtlases[AtlasIndex];
	}

	for( int32 ResourceIndex = 0; ResourceIndex < NonAtlasedTextures.Num(); ++ResourceIndex )
	{
		delete NonAtlasedTextures[ResourceIndex];
	}
	SET_DWORD_STAT(STAT_SlateNumNonAtlasedTextures, 0);
	SET_DWORD_STAT(STAT_SlateNumTextureAtlases, 0);
	SET_DWORD_STAT(STAT_SlateNumDynamicTextures, 0);

	AccessedUTextures.Empty();
	DynamicResourceMap.Empty();
	TextureAtlases.Empty();
	NonAtlasedTextures.Empty();
	DynamicTextureFreeList.Empty();
	MaterialResourceFreeList.Empty();
	UTextureFreeList.Empty();

	// Clean up mapping to texture
	ClearTextureMap();
}

void FSlateRHIResourceManager::ReloadTextures()
{
	check( IsThreadSafeForSlateRendering() );

	// Release rendering resources
	ReleaseResources();

	// wait for all rendering resources to be released
	FlushRenderingCommands();

	// Delete allocated resources (cpu)
	DeleteResources();

	// Reload everythng
	LoadUsedTextures();
}

UTexture* FSlateRHIResourceManager::GetBadResourceTexture()
{
	if ( BadResourceTexture == nullptr )
	{
		BadResourceTexture = FImageUtils::CreateCheckerboardTexture(FColor(255, 0, 255), FColor(255, 255, 0));
		BadResourceTexture->AddToRoot();
	}

	return BadResourceTexture;
}
