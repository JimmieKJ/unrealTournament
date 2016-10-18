//! @file detailsinputimagetoken.h
//! @brief Substance Framework ImageInputToken definition
//! @author Christophe Soum - Allegorithmic
//! @date 20111122
//! @copyright Allegorithmic. All rights reserved.
//!
 
#ifndef _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSINPUTIMAGETOKEN_H
#define _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSINPUTIMAGETOKEN_H

#include "detailssync.h"

#include "substance_public.h"

namespace Substance
{
namespace Details
{

struct ImageInputToken;

//! @brief Definition of the platform agnostic texture input description
//! Extend agnostic texture input holder.
//! Pointer on parent image input token (to retrieve token from texture input)
struct TextureInput : SubstanceTextureInput
{
	//! @brief Pointer on parent token, used to retrieve token inside callbacks
	ImageInputToken*const parentToken;


	//! @brief Default constructor, 0 filled texture input data
	TextureInput(ImageInputToken* parent) : parentToken(parent) {}
};


//! @brief Definition of the class used for scoped image access sync
//! Contains texture input description
//! Use a simple mutex (to replace w/ R/W mutex)
struct ImageInputToken
{
	TextureInput texture;

	//! @brief Default constructor
	ImageInputToken() : texture(this) {}

	//! @brief Virtual destructor, allows polymorphism
	virtual ~ImageInputToken() {}

	//! @brief Lock access
	void lock() { mMutex.lock(); }
	
	//! @brief Unlock access
	void unlock()  { mMutex.unlock(); }

	//! @brief Blend platform binary data
	aligned_binary_t bufferData;

	//! @brief Size in bytes of the texture data buffer.
	//!	Required and only used variable size formats (Substance_PF_JPEG).
	size_t bufferSize;

protected:
	Sync::mutex mMutex;           //!< Main mutex
	
private:
	ImageInputToken(const ImageInputToken&);
	const ImageInputToken& operator=(const ImageInputToken&);

};  // class InputImageToken


} // namespace Details
} // namespace Substance

#endif // ifndef _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSINPUTIMAGETOKEN_H
