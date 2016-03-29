// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IStereoLayers.h: Abstract interface for adding in stereoscopically projected
	layers on top of the world
=============================================================================*/

#pragma once

class IStereoLayers
{
public:

	/**
	 * Creates a new layer from a given texture resource, which is projected on top of the world as a quad
	 *
	 * @param	InTexture		A reference to the texture resource to be used on the quad
	 * @param	InPriority		A sorting priority for this layer versus other layers
	 * @param	bInFixedToFace	Whether or not the quad should be stationary in the world, or follow the head rotation
	 * @return	A unique identifier for the layer created
	 */
	virtual uint32 CreateLayer(UTexture2D* InTexture, int32 InPrioirity, bool bInFixedToFace = false) = 0;
	
	/**
	 * Destroys the specified layer, stopping it from rendering over the world
	 *
	 * @param	LayerId		The ID of layer to be destroyed
	 */
	virtual void DestroyLayer(uint32 LayerId) = 0;

	/**
	 * Set the transform of the specified layer relative to the head mounted display
	 *
	 * @param	LayerId		The ID of layer to be set the transform for
	 * @param	InTransform	The transform of the layer, relative to the head mounted display
	 */
	virtual void SetTransform(uint32 LayerId, const FTransform& InTransform) = 0;

	/**
	 * Set the size of the quad being layered over the world
	 *
	 * @param	LayerId		The ID of layer to be set the size of for
	 * @param	InSize		The size of the layer
	 */
	virtual void SetQuadSize(uint32 LayerId, const FVector2D& InSize) = 0;

	/**
	 * Sets the viewport (displayed area) of the texture for the layer
	 *
	 * @param	LayerId		The ID of layer to be set the viewport of for
	 * @param	UVRect		The UV area of the texture to be displayed on the layer
	 */
	virtual void SetTextureViewport(uint32 LayerId, const FBox2D& UVRect) = 0;
};
