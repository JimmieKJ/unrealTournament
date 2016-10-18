//! @file inputimage.h
//! @brief Substance image class used into image inputs
//! @author Christophe Soum - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include "substance_public.h"

namespace Substance
{

namespace Details
{
	struct ImageInputToken;
}


//! @brief Substance image used into image inputs
//! ImageInput are used by InputInstanceImage (input.h).
//! It contains image content used as input of graph instances.
//! Use ImageInput::ScopedAccess to read/write texture buffer
class ImageInput
{
public:
	//! @brief Shared pointer on InputImage instance type
	typedef std::shared_ptr<ImageInput> SPtr;

	//! @brief User data, member only provided for user convenience
	//std::string mUserData;

	//! @brief Build a new InputImage from texture description
	//! BLEND (default: textures in system memory) Substance Engine API 
	//! platform version ONLY.
	//! @param texture SubstanceTexture that describes the texture.
	//! @param bufferSize Size in bytes of the texture data buffer. Required
	//!		 and only used variable size formats (Substance_PF_JPEG).
	//!
	//!	A new buffer is automatically allocated to the proper 
	//!	size. If texture.buffer is NULL the internal buffer is not
	//!	initialized (use ScopedAccess to modify its content). Otherwise,
	//!	texture.buffer is not NULL, its content is copied into the 
	//!	internal buffer (texture.buffer is no more useful after this call).
	//!	If format is Substance_PF_JPEG, level0Width and level0Height members
	//!	are ignored (texture size is read from JPEG header) and bufferSize
	//!	is required.
	//! @see substance/texture.h
	//! @return Return an ImageInput instance or NULL pointer if size or
	//!		format are not valid.
	static SPtr create(
		const SubstanceTexture_& texture,
		size_t bufferSize = 0);
	
	//! @brief Mutexed texture scoped access
	struct ScopedAccess
	{
		//! @brief Constructor from InputImage to access (thread safe)
		//! @param inputImage The input image to lock, cannot be NULL pointer 
		//! @post The access to ImageInput is locked until destructor call.		
		ScopedAccess(const SPtr& inputImage);

		//! @brief Destructor, unlock ImageInput access
		//! @warning Do not modify buffer outside ScopedAccess scope
		~ScopedAccess();
	
		//! @brief Accessor on texture description
		//! @warning Do not delete buffer pointer. However its content can be 
		//!		freely modified inside ScopedAccess scope.
		const SubstanceTextureInput* operator->() const;
		
		//! @brief Helper: returns buffer content size in bytes
		//! @pre Only valid w/ BLEND Substance Engine API platform
		//!		(system memory texture).
		size_t getSize() const;

	protected:
		const SPtr mInputImage;
	};
	
	//! @brief Destructor
	//! @warning Do not delete this class directly if it was already set
	//!		into a InputImageImage: it can be still used in rendering process.
	//!		Use shared pointer mechanism instead.
	~ImageInput();

	bool resolveDirty();                            //!< Internal use only
	Details::ImageInputToken* getToken() const;     //!< Internal use only

protected:

	bool mDirty;
	std::shared_ptr<Details::ImageInputToken> mImageInputToken;

	ImageInput(std::shared_ptr<Details::ImageInputToken>);

private:
	ImageInput(const ImageInput&);
	const ImageInput& operator=(const ImageInput&);
};

} // namespace Substance
