// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Texture.h: Implementation of D3D12 Texture
=============================================================================*/
#pragma once

/** Texture base class. */
class FD3D12TextureBase : public FD3D12BaseShaderResource
{
public:

	FD3D12TextureBase(
	class FD3D12Device* InParent,
		FD3D12ResourceLocation* InResource,
		int32 InRTVArraySize,
		bool bInCreatedRTVsPerSlice,
		const TArray<TRefCountPtr<FD3D12RenderTargetView> >& InRenderTargetViews,
		TRefCountPtr<FD3D12DepthStencilView>* InDepthStencilViews
		)
		: FD3D12BaseShaderResource(InResource, InParent)
		, MemorySize(0)
		, BaseShaderResource(this)
		, RTVArraySize(InRTVArraySize)
		, bCreatedRTVsPerSlice(bInCreatedRTVsPerSlice)
		, RenderTargetViews(InRenderTargetViews)
		, NumDepthStencilViews(0)
	{
		// Set the DSVs for all the access type combinations
		if (InDepthStencilViews != nullptr)
		{
			for (uint32 Index = 0; Index < FExclusiveDepthStencil::MaxIndex; Index++)
			{
				DepthStencilViews[Index] = InDepthStencilViews[Index];
				// New Monolithic Graphics drivers have optional "fast calls" replacing various D3d functions
				// You can't use fast version of XXSetShaderResources (called XXSetFastShaderResource) on dynamic or d/s targets
				if (DepthStencilViews[Index] != NULL)
					NumDepthStencilViews++;
			}
		}
	}

	virtual ~FD3D12TextureBase() {}

	int32 GetMemorySize() const
	{
		return MemorySize;
	}

	void SetMemorySize(int32 InMemorySize)
	{
		MemorySize = InMemorySize;
	}

	// Accessors.
	FD3D12Resource* GetResource() const { return ResourceLocation->GetResource(); }
	uint64 GetOffset() const { return ResourceLocation->GetOffset(); }
	FD3D12ShaderResourceView* GetShaderResourceView() const { return ShaderResourceView; }
	FD3D12BaseShaderResource* GetBaseShaderResource() const { return BaseShaderResource; }
	void SetShaderResourceView(FD3D12ShaderResourceView* InShaderResourceView) { ShaderResourceView = InShaderResourceView; }
	void SetRenderTargetViews(const TArray<TRefCountPtr<FD3D12RenderTargetView> >& InRenderTargetViews) { RenderTargetViews = InRenderTargetViews; }

	/**
	* Get the render target view for the specified mip and array slice.
	* An array slice of -1 is used to indicate that no array slice should be required.
	*/
	FD3D12RenderTargetView* GetRenderTargetView(int32 MipIndex, int32 ArraySliceIndex) const
	{
		int32 ArrayIndex = MipIndex;

		if (bCreatedRTVsPerSlice)
		{
			check(ArraySliceIndex >= 0);
			ArrayIndex = MipIndex * RTVArraySize + ArraySliceIndex;
		}
		else
		{
			// Catch attempts to use a specific slice without having created the texture to support it
			check(ArraySliceIndex == -1 || ArraySliceIndex == 0);
		}

		if ((uint32)ArrayIndex < (uint32)RenderTargetViews.Num())
		{
			return RenderTargetViews[ArrayIndex];
		}
		return 0;
	}
	FD3D12DepthStencilView* GetDepthStencilView(FExclusiveDepthStencil AccessType) const
	{
		return DepthStencilViews[AccessType.GetIndex()];
	}

	// New Monolithic Graphics drivers have optional "fast calls" replacing various D3d functions
	// You can't use fast version of XXSetShaderResources (called XXSetFastShaderResource) on dynamic or d/s targets
	bool HasDepthStencilView()
	{
		return (NumDepthStencilViews > 0);
	}

	void AliasResources(FD3D12TextureBase* Texture)
	{
		ResourceLocation = Texture->ResourceLocation;
		BaseShaderResource = Texture->BaseShaderResource;
		ShaderResourceView = Texture->ShaderResourceView;
		RenderTargetViews = Texture->RenderTargetViews;

		for (uint32 Index = 0; Index < FExclusiveDepthStencil::MaxIndex; Index++)
		{
			DepthStencilViews[Index] = Texture->DepthStencilViews[Index];
		}
	}

protected:

	/** Amount of memory allocated by this texture, in bytes. */
	int32 MemorySize;

	/** Pointer to the base shader resource. Usually the object itself, but not for texture references. */
	FD3D12BaseShaderResource* BaseShaderResource;

	/** A shader resource view of the texture. */
	TRefCountPtr<FD3D12ShaderResourceView> ShaderResourceView;

	/** A render targetable view of the texture. */
	TArray<TRefCountPtr<FD3D12RenderTargetView> > RenderTargetViews;

	bool bCreatedRTVsPerSlice;

	int32 RTVArraySize;

	/** A depth-stencil targetable view of the texture. */
	TRefCountPtr<FD3D12DepthStencilView> DepthStencilViews[FExclusiveDepthStencil::MaxIndex];

	/** Number of Depth Stencil Views - used for fast call tracking. */
	uint32	NumDepthStencilViews;

};

/** 2D texture (vanilla, cubemap or 2D array) */
template<typename BaseResourceType>
class TD3D12Texture2D : public BaseResourceType, public FD3D12TextureBase
{
public:

	/** Flags used when the texture was created */
	uint32 Flags;

	/** Initialization constructor. */
	TD3D12Texture2D(
	class FD3D12Device* InParent,
		FD3D12ResourceLocation* InResource,
		bool bInCreatedRTVsPerSlice,
		int32 InRTVArraySize,
		const TArray<TRefCountPtr<FD3D12RenderTargetView> >& InRenderTargetViews,
		TRefCountPtr<FD3D12DepthStencilView>* InDepthStencilViews,
		uint32 InSizeX,
		uint32 InSizeY,
		uint32 InSizeZ,
		uint32 InNumMips,
		uint32 InNumSamples,
		EPixelFormat InFormat,
		bool bInCubemap,
		uint32 InFlags,
		bool bInPooled,
		const FClearValueBinding& InClearValue
#if PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
		, void* InRawTextureMemory = nullptr
#endif
		)
		: BaseResourceType(
			InSizeX,
			InSizeY,
			InSizeZ,
			InNumMips,
			InNumSamples,
			InFormat,
			InFlags,
			InClearValue
			)
		, FD3D12TextureBase(
			InParent,
			InResource,
			InRTVArraySize,
			bInCreatedRTVsPerSlice,
			InRenderTargetViews,
			InDepthStencilViews
			)
		, Flags(InFlags)
		, bCubemap(bInCubemap)
		, bPooled(bInPooled)
#if PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
		, RawTextureMemory(InRawTextureMemory)
#endif
	{
		FMemory::Memzero(ReadBackHeapDesc);
	}

	virtual ~TD3D12Texture2D();

	/**
	* Locks one of the texture's mip-maps.
	* @return A pointer to the specified texture data.
	*/
	void* Lock(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride);

	/** Unlocks a previously locked mip-map. */
	void Unlock(uint32 MipIndex, uint32 ArrayIndex);

	// Accessors.
	FD3D12Resource* GetResource() const { return (FD3D12Resource*)FD3D12TextureBase::GetResource(); }
	const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& GetReadBackHeapDesc() const
	{
		// This should only be called if SetReadBackHeapDesc() was called with actual contents
		check(ReadBackHeapDesc.Footprint.Width > 0 && ReadBackHeapDesc.Footprint.Height > 0);

		return ReadBackHeapDesc;
	}

	FD3D12CLSyncPoint GetReadBackSyncPoint() const { return ReadBackSyncPoint; }
	bool IsCubemap() const { return bCubemap; }

	/** FRHITexture override.  See FRHITexture::GetNativeResource() */
	virtual void* GetNativeResource() const override final
	{
		return GetResource();
	}

	virtual void* GetNativeShaderResourceView() const override final
	{
		return GetShaderResourceView();
	}

	virtual void* GetTextureBaseRHI() override final
	{
		return static_cast<FD3D12TextureBase*>(this);
	}

	// Modifiers.
	void SetReadBackHeapDesc(const D3D12_PLACED_SUBRESOURCE_FOOTPRINT &newReadBackHeapDesc) { ReadBackHeapDesc = newReadBackHeapDesc; }
	void SetReadBackListHandle(FD3D12CommandListHandle listToWaitFor) { ReadBackSyncPoint = listToWaitFor; }

	// IRefCountedObject interface.
	virtual uint32 AddRef() const
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const
	{
		return FRHIResource::GetRefCount();
	}
#if PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
	void* GetRawTextureMemory() const
	{
		return RawTextureMemory;
	}
#endif

private:
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT ReadBackHeapDesc;
	FD3D12CLSyncPoint ReadBackSyncPoint;

	/** Whether the texture is a cube-map. */
	const uint32 bCubemap : 1;
	/** Whether the texture can be pooled. */
	const uint32 bPooled : 1;
#if PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
	void* RawTextureMemory;
#endif
};

/** 3D Texture */
class FD3D12Texture3D : public FRHITexture3D, public FD3D12TextureBase
{
public:

	/** Initialization constructor. */
	FD3D12Texture3D(
	class FD3D12Device* InParent,
		FD3D12ResourceLocation* InResource,
		const TArray<TRefCountPtr<FD3D12RenderTargetView> >& InRenderTargetViews,
		uint32 InSizeX,
		uint32 InSizeY,
		uint32 InSizeZ,
		uint32 InNumMips,
		EPixelFormat InFormat,
		uint32 InFlags,
		const FClearValueBinding& InClearValue
		)
		: FRHITexture3D(InSizeX, InSizeY, InSizeZ, InNumMips, InFormat, InFlags, InClearValue)
		, FD3D12TextureBase(
			InParent,
			InResource,
			1,
			false,
			InRenderTargetViews,
			NULL
			)
	{
	}

	virtual ~FD3D12Texture3D();

	// Accessors.
	FD3D12Resource* GetResource() const { return (FD3D12Resource*)FD3D12TextureBase::GetResource(); }

	virtual void* GetTextureBaseRHI() override final
	{
		return static_cast<FD3D12TextureBase*>(this);
	}

	// IRefCountedObject interface.
	virtual uint32 AddRef() const
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const
	{
		return FRHIResource::GetRefCount();
	}
};

class FD3D12BaseTexture2D : public FRHITexture2D
{
public:
	FD3D12BaseTexture2D(uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ, uint32 InNumMips, uint32 InNumSamples, EPixelFormat InFormat, uint32 InFlags, const FClearValueBinding& InClearValue)
		: FRHITexture2D(InSizeX, InSizeY, InNumMips, InNumSamples, InFormat, InFlags, InClearValue)
	{}
	uint32 GetSizeZ() const { return 0; }
};

class FD3D12BaseTexture2DArray : public FRHITexture2DArray
{
public:
	FD3D12BaseTexture2DArray(uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ, uint32 InNumMips, uint32 InNumSamples, EPixelFormat InFormat, uint32 InFlags, const FClearValueBinding& InClearValue)
		: FRHITexture2DArray(InSizeX, InSizeY, InSizeZ, InNumMips, InFormat, InFlags, InClearValue)
	{
		check(InNumSamples == 1);
	}
};

class FD3D12BaseTextureCube : public FRHITextureCube
{
public:
	FD3D12BaseTextureCube(uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ, uint32 InNumMips, uint32 InNumSamples, EPixelFormat InFormat, uint32 InFlags, const FClearValueBinding& InClearValue)
		: FRHITextureCube(InSizeX, InNumMips, InFormat, InFlags, InClearValue)
	{
		check(InNumSamples == 1);
	}
	uint32 GetSizeX() const { return GetSize(); }
	uint32 GetSizeY() const { return GetSize(); }
	uint32 GetSizeZ() const { return 0; }
};

typedef TD3D12Texture2D<FRHITexture>              FD3D12Texture;
typedef TD3D12Texture2D<FD3D12BaseTexture2D>      FD3D12Texture2D;
typedef TD3D12Texture2D<FD3D12BaseTexture2DArray> FD3D12Texture2DArray;
typedef TD3D12Texture2D<FD3D12BaseTextureCube>    FD3D12TextureCube;

/** Texture reference class. */
class FD3D12TextureReference : public FRHITextureReference, public FD3D12TextureBase
{
public:
	FD3D12TextureReference(class FD3D12Device* InParent, FLastRenderTimeContainer* LastRenderTime)
		: FRHITextureReference(LastRenderTime)
		, FD3D12TextureBase(InParent, NULL, 0, false, TArray<TRefCountPtr<FD3D12RenderTargetView> >(), NULL)
	{
		BaseShaderResource = NULL;
	}

	void SetReferencedTexture(FRHITexture* InTexture, FD3D12BaseShaderResource* InBaseShaderResource, FD3D12ShaderResourceView* InSRV)
	{
		ShaderResourceView = InSRV;
		BaseShaderResource = InBaseShaderResource;
		FRHITextureReference::SetReferencedTexture(InTexture);
	}

	virtual void* GetTextureBaseRHI() override final
	{
		return static_cast<FD3D12TextureBase*>(this);
	}

	// IRefCountedObject interface.
	virtual uint32 AddRef() const
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const
	{
		return FRHIResource::GetRefCount();
	}
};

/** Given a pointer to a RHI texture that was created by the D3D12 RHI, returns a pointer to the FD3D12TextureBase it encapsulates. */
FORCEINLINE FD3D12TextureBase* GetD3D12TextureFromRHITexture(FRHITexture* Texture)
{
	if (!Texture)
	{
		return NULL;
	}
	
	FD3D12TextureBase* Result((FD3D12TextureBase*)Texture->GetTextureBaseRHI());
	check(Result);
	return Result;
}

template<>
struct TD3D12ResourceTraits<FRHITexture3D>
{
	typedef FD3D12Texture3D TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHITexture>
{
	typedef FD3D12Texture TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHITexture2D>
{
	typedef FD3D12Texture2D TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHITexture2DArray>
{
	typedef FD3D12Texture2DArray TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHITextureCube>
{
	typedef FD3D12TextureCube TConcreteType;
};