//! @file ImageInput.cpp
//! @brief Substance image class used into image inputs
//! @author Christophe Soum - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceCorePrivatePCH.h"

#include "framework/imageinput.h"

#include "framework/details/detailsinputimagetoken.h"

#include <assert.h>
#include <memory.h>

//! @brief Constructor from ImageInput to access (thread safe)
//! @param ImageInput The input image to lock, cannot be NULL pointer 
//! @post The access to ImageInput is locked until destructor call.		
Substance::ImageInput::ScopedAccess::ScopedAccess(
		const std::shared_ptr<ImageInput>& ImageInput) :
			mInputImage(ImageInput)
{
	check(mInputImage);
	mInputImage->mImageInputToken->lock();
	mInputImage->mDirty = true;
}


//! @brief Destructor, unlock ImageInput access
//! @warning Do not modify buffer outside ScopedAccess scope
Substance::ImageInput::ScopedAccess::~ScopedAccess()
{
	mInputImage->mImageInputToken->unlock();
}


//! @brief Accessor on texture description
//! @warning Do not delete buffer pointer. However its content can be 
//!		freely modified inside ScopedAccess scope.
const SubstanceTextureInput* 
Substance::ImageInput::ScopedAccess::operator->() const
{
	SubstanceTextureInput* texinp = &(mInputImage->mImageInputToken->texture);
	return texinp;
}


//! @brief Helper: returns buffer content size in bytes
//! Only valid w/ BLEND Substance Engine API platform
//!		(system memory texture).
size_t Substance::ImageInput::ScopedAccess::getSize() const
{
	const Details::ImageInputToken*const tknblend =
		dynamic_cast<const Details::ImageInputToken*>(
		mInputImage->mImageInputToken.get());
	assert(tknblend != NULL);

	return tknblend != NULL ? tknblend->bufferSize : 0;
}


//! @brief Destructor
//! @warning Do not delete this class directly if it was already set
//!		into a ImageInput: it can be still used in rendering process.
//!		Use shared pointer mechanism instead.
Substance::ImageInput::~ImageInput()
{

}


Substance::Details::ImageInputToken* 
	Substance::ImageInput::getToken() const
{
	return mImageInputToken.get();
}


Substance::ImageInput::ImageInput(
	std::shared_ptr<Details::ImageInputToken> token) :
	mDirty(true),
	mImageInputToken(token)
{
}


//! @brief Internal use only
bool Substance::ImageInput::resolveDirty()
{
	const bool res = mDirty;
	mDirty = false;
	return res;
}


std::shared_ptr<Substance::ImageInput> Substance::ImageInput::create(
	const SubstanceTexture& srctex,
	size_t bufferSize)
{
	std::shared_ptr<Details::ImageInputToken> tknblend(
		new Details::ImageInputToken);

	SubstanceTextureInput& dsttexinp = tknblend->texture;
	
	dsttexinp.mTexture = srctex;
	dsttexinp.level0Width = dsttexinp.mTexture.level0Width;
	dsttexinp.level0Height = dsttexinp.mTexture.level0Height;
	dsttexinp.pixelFormat = dsttexinp.mTexture.pixelFormat;
	dsttexinp.mipmapCount = dsttexinp.mTexture.mipmapCount;
	dsttexinp.mTexture.buffer = NULL;
	
	const size_t nbPixels = 
		dsttexinp.level0Width*dsttexinp.level0Height;
	const size_t nbDxtBlocks = 
		((dsttexinp.level0Width+3)>>2)*((dsttexinp.level0Height+3)>>2);
	
	switch (dsttexinp.pixelFormat&0x1F)
	{
		case Substance_PF_RGBA:
		case Substance_PF_RGBx: bufferSize = nbPixels * 4; break;
		case Substance_PF_RGB: bufferSize = nbPixels * 3; break;
		case Substance_PF_L: bufferSize = nbPixels;   break;
		case Substance_PF_RGBA|Substance_PF_16b:
		case Substance_PF_RGBx | Substance_PF_16b: bufferSize = nbPixels * 8; break;
		case Substance_PF_RGB | Substance_PF_16b: bufferSize = nbPixels * 6; break;
		case Substance_PF_L | Substance_PF_16b: bufferSize = nbPixels * 2; break;
		case Substance_PF_DXT1: bufferSize = nbDxtBlocks * 8;  break;
		case Substance_PF_DXT2:
		case Substance_PF_DXT3:
		case Substance_PF_DXT4:
		case Substance_PF_DXT5:
		case Substance_PF_DXTn: bufferSize = nbDxtBlocks * 16; break;
	}
	
	if (bufferSize>0)
	{
		tknblend->bufferData.resize(bufferSize + 0xF);
		tknblend->bufferSize = bufferSize;

		dsttexinp.mTexture.buffer = 
			(void*)((((size_t)&tknblend->bufferData[0]) + 0xF)&~(size_t)0xF);
			
		if (srctex.buffer)
		{
			memcpy(dsttexinp.mTexture.buffer, srctex.buffer, bufferSize);
		}
	}

	SPtr ptr;
	if (dsttexinp.mTexture.buffer != NULL)
	{
		ptr.reset(new Substance::ImageInput(
			std::shared_ptr<Details::ImageInputToken>(tknblend)));
	}

	return ptr; // Return NULL ptr if invalid
}

