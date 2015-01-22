// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "PaperAtlasGenerator.h"

#include "AssetRegistryModule.h"
#include "Textures/TextureAtlas.h"

//////////////////////////////////////////////////////////////////////////
// 

void CopyTextureData(const uint8* Source, uint8* Dest, uint32 SizeX, uint32 SizeY, uint32 BytesPerPixel, uint32 SourceStride, uint32 DestStride)
{
	const uint32 NumBytesPerRow = SizeX * BytesPerPixel;

	for (uint32 Y = 0; Y < SizeY; ++Y)
	{
		FMemory::Memcpy(
			Dest + (DestStride * Y),
			Source + (SourceStride * Y),
			NumBytesPerRow
			);
	}
}

struct FSingleTexturePaperAtlas : public FSlateTextureAtlas
{
public:
	FSingleTexturePaperAtlas(uint32 InWidth, uint32 InHeight)
	: FSlateTextureAtlas(InWidth, InHeight, sizeof(FColor), ESlateTextureAtlasPaddingStyle::NoPadding)
	{
		//@TODO: check(StrideBytes >= 4*Width);
	}

	virtual void ConditionalUpdateTexture() override {}

	void CopyToTexture(UTexture2D* Texture)
	{
		const int32 NumMips = 1;

		Texture->Source.Init(AtlasWidth, AtlasHeight, /*NumSlices=*/ 1, NumMips, ETextureSourceFormat::TSF_BGRA8, AtlasData.GetData());
		Texture->UpdateResource();
	}

	const FAtlasedTextureSlot* AddSprite(UPaperSprite* Sprite)
	{
		const FVector2D SpriteSizeFloat = Sprite->GetSourceSize();
		const FIntPoint SpriteSize(FMath::TruncToInt(SpriteSizeFloat.X), FMath::TruncToInt(SpriteSizeFloat.Y));

		TArray<uint8> DummyBuffer;
		DummyBuffer.AddZeroed(SpriteSize.X * SpriteSize.Y * BytesPerPixel);
		
		check(Sprite->GetSourceTexture());
		FTextureSource& SourceData = Sprite->GetSourceTexture()->Source;

		//@TODO: Handle different texture formats!
		if (SourceData.GetFormat() == TSF_BGRA8)
		{
			uint32 BytesPerPixel = SourceData.GetBytesPerPixel();
			uint8* OffsetSource = SourceData.LockMip(0) + (FMath::TruncToInt(Sprite->GetSourceUV().X) + FMath::TruncToInt(Sprite->GetSourceUV().Y) * SourceData.GetSizeX()) * BytesPerPixel;
			uint8* OffsetDest = DummyBuffer.GetData();

			CopyTextureData(OffsetSource, OffsetDest, SpriteSize.X, SpriteSize.Y, BytesPerPixel, SourceData.GetSizeX() * BytesPerPixel, SpriteSize.X * BytesPerPixel);

			SourceData.UnlockMip(0);
		}
		else
		{
			UE_LOG(LogPaper2DEditor, Error, TEXT("Sprite %s is not BGRA8, which isn't supported in atlases yet"), *(Sprite->GetPathName()));
		}

		const FAtlasedTextureSlot* Slot = AddTexture(SpriteSize.X, SpriteSize.Y, DummyBuffer);
		if (Slot != nullptr)
		{
			SpriteToSlotMap.Add(Sprite, Slot);
		}

		return Slot;
	}

public:
	TMap<UPaperSprite*, const FAtlasedTextureSlot*> SpriteToSlotMap;
};

//////////////////////////////////////////////////////////////////////////
// FPaperAtlasGenerator

void FPaperAtlasGenerator::HandleAssetChangedEvent(UPaperSpriteAtlas* Atlas)
{
	Atlas->MaxWidth = FMath::Clamp(Atlas->MaxWidth, 16, 4096);
	Atlas->MaxHeight = FMath::Clamp(Atlas->MaxHeight, 16, 4096);

	// Find all sprites that reference the atlas and force them loaded
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TArray<FAssetData> AssetList;
	TMultiMap<FName, FString> TagsAndValues;
	TagsAndValues.Add(TEXT("AtlasGroupGUID"), Atlas->AtlasGUID.ToString(EGuidFormats::Digits));
	AssetRegistryModule.Get().GetAssetsByTagValues(TagsAndValues, AssetList);

	TIndirectArray<FSingleTexturePaperAtlas> AtlasGenerators;

	// Start off with one page
	new (AtlasGenerators) FSingleTexturePaperAtlas(Atlas->MaxWidth, Atlas->MaxHeight);

	for (const FAssetData& SpriteRef : AssetList)
	{
		if (UPaperSprite* Sprite = Cast<UPaperSprite>(SpriteRef.GetAsset()))
		{
			//@TODO: Use the tight bounds instead of the source bounds
			const FVector2D SpriteSizeFloat = Sprite->GetSourceSize();
			const FIntPoint SpriteSize(FMath::TruncToInt(SpriteSizeFloat.X), FMath::TruncToInt(SpriteSizeFloat.Y));

			if (Sprite->GetSourceTexture() == nullptr)
			{
				UE_LOG(LogPaper2DEditor, Error, TEXT("Sprite %s has no source texture and cannot be packed"), *(Sprite->GetPathName()));
				continue;
			}

			//@TODO: Take padding into account (push this into the generator)
			if ((SpriteSize.X > Atlas->MaxWidth) || (SpriteSize.Y >= Atlas->MaxHeight))
			{
				// This sprite cannot ever fit into an atlas page
				UE_LOG(LogPaper2DEditor, Error, TEXT("Sprite %s (%d x %d) can never fit into atlas %s (%d x %d) due to maximum page size restrictions"),
					*(Sprite->GetPathName()),
					SpriteSize.X,
					SpriteSize.Y,
					*(Atlas->GetPathName()),
					Atlas->MaxWidth,
					Atlas->MaxHeight);
			}
			else
			{
				const FAtlasedTextureSlot* Slot = nullptr;

				// Does it fit in any existing pages?
				for (auto& Generator : AtlasGenerators)
				{
					Slot = Generator.AddSprite(Sprite);

					if (Slot != nullptr)
					{
						break;
					}
				}

				if (Slot == nullptr)
				{
					// Doesn't fit in any current pages, make a new one
					FSingleTexturePaperAtlas* NewGenerator = new (AtlasGenerators) FSingleTexturePaperAtlas(Atlas->MaxWidth, Atlas->MaxHeight);
					Slot = NewGenerator->AddSprite(Sprite);
				}

				if (Slot != nullptr)
				{
					UE_LOG(LogPaper2DEditor, Warning, TEXT("Allocated %s to (%d,%d)"), *(Sprite->GetPathName()), Slot->X, Slot->Y);
				}
				else
				{
					// ERROR: Didn't fit into a brand new page, maybe padding was wrong
					UE_LOG(LogPaper2DEditor, Error, TEXT("Failed to allocate %s into a brand new page"), *(Sprite->GetPathName()));
				}
			}
		}
	}

	// Turn the generators back into textures
	Atlas->GeneratedTextures.Empty(AtlasGenerators.Num());
	for (int32 GeneratorIndex = 0; GeneratorIndex < AtlasGenerators.Num(); ++GeneratorIndex)
	{
		FSingleTexturePaperAtlas& AtlasPage = AtlasGenerators[GeneratorIndex];

		UTexture2D* Texture = NewNamedObject<UTexture2D>(Atlas, NAME_None, RF_Public);
		Atlas->GeneratedTextures.Add(Texture);

		AtlasPage.CopyToTexture(Texture);

		// Now update the baked data for all the sprites to point there
		for (auto& SpriteToSlot : AtlasPage.SpriteToSlotMap)
		{
			UPaperSprite* Sprite = SpriteToSlot.Key;
			Sprite->Modify();
			const FAtlasedTextureSlot* Slot = SpriteToSlot.Value;
			Sprite->BakedSourceTexture = Texture;
			Sprite->BakedSourceUV = FVector2D(Slot->X, Slot->Y);
			Sprite->RebuildRenderData();
		}
	}

	//@TODO: Adjust the atlas rebuild code so that it tries to preserve existing sprites (optimize for the 'just added one to the end' case, preventing dirtying lots of assets unnecessarily)
	//@TODO: invoke this code when a sprite has the atlas group property modified
}
