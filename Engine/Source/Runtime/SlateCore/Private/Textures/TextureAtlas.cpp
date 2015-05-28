// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"
#include "SlateRenderer.h"

DEFINE_STAT(STAT_SlateTextureGPUMemory);
DEFINE_STAT(STAT_SlateTextureDataMemory);
DECLARE_MEMORY_STAT(TEXT("Texture Atlas CPU Memory"), STAT_SlateTextureAtlasMemory, STATGROUP_SlateMemory);

/* FSlateTextureAtlas helper class
 *****************************************************************************/

FSlateTextureAtlas::~FSlateTextureAtlas()
{
	Empty();
}


/* FSlateTextureAtlas interface
 *****************************************************************************/

void FSlateTextureAtlas::Empty()
{
	// Remove all nodes
	DestroyNodes( RootNode );
	RootNode = nullptr;

	STAT(uint32 MemoryBefore = AtlasData.GetAllocatedSize());

	// Clear all raw data
	AtlasData.Empty();

	STAT(uint32 MemoryAfter = AtlasData.GetAllocatedSize());
	DEC_MEMORY_STAT_BY(STAT_SlateTextureAtlasMemory, MemoryBefore-MemoryAfter);
}


const FAtlasedTextureSlot* FSlateTextureAtlas::AddTexture( uint32 TextureWidth, uint32 TextureHeight, const TArray<uint8>& Data )
{
	// Find a spot for the character in the texture
	const FAtlasedTextureSlot* NewSlot = FindSlotForTexture(TextureWidth, TextureHeight);

	// handle cases like space, where the size of the glyph is zero. The copy data code doesn't handle zero sized source data with a padding
	// so make sure to skip this call.
	if (NewSlot && TextureWidth > 0 && TextureHeight > 0)
	{
		CopyDataIntoSlot(NewSlot, Data);
		MarkTextureDirty();
	}

	return NewSlot;
}


void FSlateTextureAtlas::MarkTextureDirty()
{
	check( 
		(GSlateLoadingThreadId != 0) || 
		(AtlasOwnerThread == ESlateTextureAtlasOwnerThread::Game && IsInGameThread()) || 
		(AtlasOwnerThread == ESlateTextureAtlasOwnerThread::Render && IsInRenderingThread()) 
		);

	bNeedsUpdate = true;
}


const FAtlasedTextureSlot* FSlateTextureAtlas::FindSlotForTexture( uint32 InWidth, uint32 InHeight )
{
	return FindSlotForTexture(*RootNode, InWidth, InHeight);
}

void FSlateTextureAtlas::DestroyNodes( FAtlasedTextureSlot* StartNode )
{
	if (StartNode->Left)
	{
		DestroyNodes(StartNode->Left);
	}

	if (StartNode->Right)
	{
		DestroyNodes(StartNode->Right);
	}

	delete StartNode;
}


void FSlateTextureAtlas::InitAtlasData()
{
	check(RootNode == nullptr && AtlasData.Num() == 0);

	RootNode = new FAtlasedTextureSlot(0, 0, AtlasWidth, AtlasHeight, GetPaddingAmount());
	AtlasData.Reserve(AtlasWidth * AtlasHeight * BytesPerPixel);
	AtlasData.AddZeroed(AtlasWidth * AtlasHeight * BytesPerPixel);

	check(IsInGameThread() || IsInRenderingThread());
	AtlasOwnerThread = (IsInGameThread()) ? ESlateTextureAtlasOwnerThread::Game : ESlateTextureAtlasOwnerThread::Render;

	INC_MEMORY_STAT_BY(STAT_SlateTextureAtlasMemory, AtlasData.GetAllocatedSize());
}


void FSlateTextureAtlas::CopyRow( const FCopyRowData& CopyRowData )
{
	const uint8* Data = CopyRowData.SrcData;
	uint8* Start = CopyRowData.DestData;
	const uint32 SourceWidth = CopyRowData.SrcTextureWidth;
	const uint32 DestWidth = CopyRowData.DestTextureWidth;
	const uint32 SrcRow = CopyRowData.SrcRow;
	const uint32 DestRow = CopyRowData.DestRow;
	// this can only be one or zero
	const uint32 Padding = GetPaddingAmount();

	const uint8* SourceDataAddr = &Data[(SrcRow * SourceWidth) * BytesPerPixel];
	uint8* DestDataAddr = &Start[(DestRow * DestWidth + Padding) * BytesPerPixel];
	FMemory::Memcpy(DestDataAddr, SourceDataAddr, SourceWidth * BytesPerPixel);

	if (Padding > 0)
	{ 
		uint8* DestPaddingPixelLeft = &Start[(DestRow * DestWidth) * BytesPerPixel];
		uint8* DestPaddingPixelRight = DestPaddingPixelLeft + ((CopyRowData.RowWidth - 1) * BytesPerPixel);
		if (PaddingStyle == ESlateTextureAtlasPaddingStyle::DilateBorder)
		{
			const uint8* FirstPixel = SourceDataAddr; 
			const uint8* LastPixel = SourceDataAddr + ((SourceWidth - 1) * BytesPerPixel);
			FMemory::Memcpy(DestPaddingPixelLeft, FirstPixel, BytesPerPixel);
			FMemory::Memcpy(DestPaddingPixelRight, LastPixel, BytesPerPixel);
		}
		else
		{
			FMemory::Memzero(DestPaddingPixelLeft, BytesPerPixel);
			FMemory::Memzero(DestPaddingPixelRight, BytesPerPixel);
		}

	} 
}

void FSlateTextureAtlas::ZeroRow( const FCopyRowData& CopyRowData )
{
	const uint32 SourceWidth = CopyRowData.SrcTextureWidth;
	const uint32 DestWidth = CopyRowData.DestTextureWidth;
	const uint32 DestRow = CopyRowData.DestRow;

	uint8* DestDataAddr = &CopyRowData.DestData[DestRow * DestWidth * BytesPerPixel];
	FMemory::Memzero(DestDataAddr, CopyRowData.RowWidth * BytesPerPixel);
}


void FSlateTextureAtlas::CopyDataIntoSlot( const FAtlasedTextureSlot* SlotToCopyTo, const TArray<uint8>& Data )
{
	// Copy pixel data to the texture
	uint8* Start = &AtlasData[SlotToCopyTo->Y*AtlasWidth*BytesPerPixel + SlotToCopyTo->X*BytesPerPixel];
	
	// Account for same padding on each sides
	const uint32 Padding = GetPaddingAmount();
	const uint32 AllPadding = Padding*2;

	// Make sure the actual slot is not zero-area (otherwise padding could corrupt other images in the atlas)
	check(SlotToCopyTo->Width > AllPadding);
	check(SlotToCopyTo->Height > AllPadding);

	// The width of the source texture without padding (actual width)
	const uint32 SourceWidth = SlotToCopyTo->Width - AllPadding; 
	const uint32 SourceHeight = SlotToCopyTo->Height - AllPadding;

	FCopyRowData CopyRowData;
	CopyRowData.DestData = Start;
	CopyRowData.SrcData = Data.GetData();
	CopyRowData.DestTextureWidth = AtlasWidth;
	CopyRowData.SrcTextureWidth = SourceWidth;
	CopyRowData.RowWidth = SlotToCopyTo->Width;

	// Apply the padding for bilinear filtering. 
	// Not used if no padding (assumes sampling outside boundaries of the sub texture is not possible)
	if (Padding > 0)
	{
		// Copy first color row into padding.  
		CopyRowData.SrcRow = 0;
		CopyRowData.DestRow = 0;

		if (PaddingStyle == ESlateTextureAtlasPaddingStyle::DilateBorder)
		{
			CopyRow(CopyRowData);
		}
		else
		{
			ZeroRow(CopyRowData);
		}
	}

	// Copy each row of the texture
	for (uint32 Row = Padding; Row < SlotToCopyTo->Height-Padding; ++Row)
	{
		CopyRowData.SrcRow = Row-Padding;
		CopyRowData.DestRow = Row;

		CopyRow(CopyRowData);
	}

	if (Padding > 0)
	{
		// Copy last color row into padding row for bilinear filtering
		CopyRowData.SrcRow = SourceHeight - 1;
		CopyRowData.DestRow = SlotToCopyTo->Height - Padding;

		if (PaddingStyle == ESlateTextureAtlasPaddingStyle::DilateBorder)
		{
			CopyRow(CopyRowData);
		}
		else
		{
			ZeroRow(CopyRowData);
		}
	}
}


const FAtlasedTextureSlot* FSlateTextureAtlas::FindSlotForTexture( FAtlasedTextureSlot& Start, uint32 InWidth, uint32 InHeight )
{
	// If there are left and right slots there are empty regions around this slot.  
	// It also means this slot is occupied by a texture
	if (Start.Left || Start.Right)
	{
		// Recursively search left for the smallest empty slot that can fit the texture
		if (Start.Left)
		{
			const FAtlasedTextureSlot* NewSlot = FindSlotForTexture(*Start.Left, InWidth, InHeight);
			if (NewSlot)
			{
				return NewSlot;
			}
		}

		// Recursively search right for the smallest empty slot that can fit the texture
		if (Start.Right)
		{
			const FAtlasedTextureSlot* NewSlot = FindSlotForTexture(*Start.Right, InWidth, InHeight);
			if(NewSlot)
			{
				return NewSlot;
			}
		}

		// Not enough space
		return nullptr;
	}

	// Account for padding on both sides
	const uint32 Padding = GetPaddingAmount();
	const uint32 TotalPadding = Padding * 2;
	const uint32 PaddedWidth = InWidth + TotalPadding;
	const uint32 PaddedHeight = InHeight + TotalPadding;

	// This slot can't fit the character
	if ((PaddedWidth > Start.Width) || (PaddedHeight > Start.Height))
	{
		// not enough space
		return nullptr;
	}
	
	// The width and height of the new child node
	const uint32 RemainingWidth =  FMath::Max<int32>(0, Start.Width - PaddedWidth);
	const uint32 RemainingHeight = FMath::Max<int32>(0, Start.Height - PaddedHeight);

	// Split the remaining area around this slot into two children.
	if (RemainingHeight <= RemainingWidth)
	{
		// Split vertically
		Start.Left = new FAtlasedTextureSlot(Start.X, Start.Y + PaddedHeight, PaddedWidth, RemainingHeight, Padding);
		Start.Right = new FAtlasedTextureSlot(Start.X + PaddedWidth, Start.Y, RemainingWidth, Start.Height, Padding);
	}
	else
	{
		// Split horizontally
		Start.Left = new FAtlasedTextureSlot(Start.X + PaddedWidth, Start.Y, RemainingWidth, PaddedHeight, Padding);
		Start.Right = new FAtlasedTextureSlot(Start.X, Start.Y + PaddedHeight, Start.Width, RemainingHeight, Padding);
	}

	// Shrink the slot to the remaining area.
	Start.Width = PaddedWidth;
	Start.Height = PaddedHeight;

	return &Start;
}
