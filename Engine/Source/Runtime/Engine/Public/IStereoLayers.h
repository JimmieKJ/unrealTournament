// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IStereoLayers.h: Abstract interface for adding in stereoscopically projected
	layers on top of the world
=============================================================================*/

#pragma once

class IStereoLayers
{
public:

	enum ELayerType
	{
		WorldLocked,
		TrackerLocked,
		FaceLocked
	};

	enum ELayerFlags
	{
		LAYER_FLAG_TEX_CONTINUOUS_UPDATE	= 0x00000001, // Internally copies the texture on every frame for video, etc.
		LAYER_FLAG_TEX_NO_ALPHA_CHANNEL		= 0x00000002, // Ignore the textures alpha channel, this makes the stereo layer opaque
		LAYER_FLAG_QUAD_PRESERVE_TEX_RATIO	= 0x00000004, // Quad Y component will be calculated based on the texture dimensions
	};

	/**
	 * Structure describing the visual appearance of a single stereo layer
	 */
	struct FLayerDesc
	{
		FTransform		Transform	= FTransform::Identity;									// View space transform
		FVector2D		QuadSize	= FVector2D(1.0f, 1.0f);								// Size of rendered quad
		FBox2D			UVRect		= FBox2D(FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f));	// UVs of rendered quad
		int32			Priority	= 0;													// Render order priority, higher priority render on top of lower priority
		ELayerType		Type		= ELayerType::FaceLocked;								// Which space the quad is locked within
		FTextureRHIRef	Texture		= nullptr;												// Texture mapped to the quad
		uint32			Flags		= 0;													// Uses LAYER_FLAG_...
	};

	/**
	 * Creates a new layer from a given texture resource, which is projected on top of the world as a quad
	 *
	 * @param	InLayerDesc		A reference to the texture resource to be used on the quad
	 * @return	A unique identifier for the layer created
	 */
	virtual uint32 CreateLayer(const FLayerDesc& InLayerDesc) = 0;
	
	/**
	 * Destroys the specified layer, stopping it from rendering over the world
	 *
	 * @param	LayerId		The ID of layer to be destroyed
	 */
	virtual void DestroyLayer(uint32 LayerId) = 0;

	/**
	 * Set the a new layer description
	 *
	 * @param	LayerId		The ID of layer to be set the description
	 * @param	InLayerDesc	The new description to be set
	 */
	virtual void SetLayerDesc(uint32 LayerId, const FLayerDesc& InLayerDesc) = 0;

	/**
	 * Get the currently set layer description
	 *
	 * @param	LayerId			The ID of layer to be set the description
	 * @param	OutLayerDesc	The returned layer description
	 * @return	Whether the returned layer description is valid
	 */
	virtual bool GetLayerDesc(uint32 LayerId, FLayerDesc& OutLayerDesc) = 0;

	/**
	 * Marks this layers texture for update
	 *
	 * @param	LayerId			The ID of layer to be set the description
	 */
	virtual void MarkTextureForUpdate(uint32 LayerId) = 0;
};
