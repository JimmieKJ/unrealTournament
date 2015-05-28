// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "StandaloneRendererPrivate.h"
#include "Windows/D3D/SlateD3DRenderer.h"
#include "Windows/D3D/SlateD3DTextures.h"

void FSlateD3DTexture::Init( DXGI_FORMAT InFormat, D3D11_SUBRESOURCE_DATA* InitalData, D3D11_USAGE Usage, uint32 InCPUAccessFlags, uint32 BindFlags )
{
	// Create the texture resource
	D3D11_TEXTURE2D_DESC TexDesc;
	TexDesc.Width = SizeX;
	TexDesc.Height = SizeY;
	TexDesc.MipLevels = 1;
	TexDesc.ArraySize = 1;
	TexDesc.Format = InFormat;
	TexDesc.SampleDesc.Count = 1;
	TexDesc.SampleDesc.Quality = 0;
	TexDesc.Usage = Usage;
	TexDesc.BindFlags = BindFlags;
	TexDesc.CPUAccessFlags = InCPUAccessFlags;
	TexDesc.MiscFlags = 0;

	HRESULT Hr = GD3DDevice->CreateTexture2D( &TexDesc, InitalData, D3DTexture.GetInitReference() );
	checkf( SUCCEEDED(Hr), TEXT("D3D11 Error Result %X"), Hr );

	// Create the shader accessable view of the texture
	D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc;
	SrvDesc.Format = TexDesc.Format;
	SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SrvDesc.Texture2D.MostDetailedMip = 0;
	SrvDesc.Texture2D.MipLevels = 1;

	// Create the shader resource view.
	Hr = GD3DDevice->CreateShaderResourceView( D3DTexture, &SrvDesc, ShaderResource.GetInitReference() );
	checkf( SUCCEEDED(Hr), TEXT("D3D11 Error Result %X"), Hr );
}

void FSlateD3DTexture::ResizeTexture(uint32 Width, uint32 Height)
{
	// Seems only way to resize d3d texture is recreate it
	SizeX = Width;
	SizeY = Height;
	Init(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, NULL, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
}

void FSlateD3DTexture::UpdateTexture(const TArray<uint8>& Bytes)
{
	// TODO: Improve the memory copying here, have tried using UpdateSubresource but it doesn't seem to work
	D3D11_MAPPED_SUBRESOURCE Resource;
	GD3DDeviceContext->Map(D3DTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	for (uint32 Row = 0; Row < SizeY; ++Row)
	{
		FMemory::Memcpy((uint8*)Resource.pData + Row * Resource.RowPitch, Bytes.GetData() + Row * SizeX*4, SizeX*4);
	}
	GD3DDeviceContext->Unmap(D3DTexture, 0);
}

FSlateTextureAtlasD3D::FSlateTextureAtlasD3D( uint32 Width, uint32 Height, uint32 StrideBytes, ESlateTextureAtlasPaddingStyle PaddingStyle )
	: FSlateTextureAtlas( Width, Height, StrideBytes, PaddingStyle )
	, AtlasTexture( new FSlateD3DTexture( Width, Height ) )
{
}

FSlateTextureAtlasD3D::~FSlateTextureAtlasD3D()
{
	if( AtlasTexture )
	{
		delete AtlasTexture;
	}
}



void FSlateTextureAtlasD3D::InitAtlasTexture( int32 Index )
{
	check( AtlasTexture );

	D3D11_SUBRESOURCE_DATA InitData;
	InitData.pSysMem = AtlasData.GetData();
	InitData.SysMemPitch = AtlasWidth * 4;

	AtlasTexture->Init( DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, &InitData );
}

void FSlateTextureAtlasD3D::ConditionalUpdateTexture()
{
	// Not yet supported
}

FSlateFontAtlasD3D::FSlateFontAtlasD3D( uint32 Width, uint32 Height )
	: FSlateFontAtlas( Width, Height ) 
{
	FontTexture = new FSlateD3DTexture( Width, Height );
	FontTexture->Init( DXGI_FORMAT_A8_UNORM, NULL, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE );
}

FSlateFontAtlasD3D::~FSlateFontAtlasD3D()
{
	delete FontTexture;
}

void FSlateFontAtlasD3D::ConditionalUpdateTexture()
{
	if( bNeedsUpdate )
	{
		D3D11_MAPPED_SUBRESOURCE Resource;
		GD3DDeviceContext->Map( FontTexture->GetTextureResource(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource );
		FMemory::Memcpy( Resource.pData, AtlasData.GetData(), sizeof(uint8)*AtlasWidth*AtlasHeight);
		GD3DDeviceContext->Unmap( FontTexture->GetTextureResource(), 0 );

		bNeedsUpdate = false;
	}
}
