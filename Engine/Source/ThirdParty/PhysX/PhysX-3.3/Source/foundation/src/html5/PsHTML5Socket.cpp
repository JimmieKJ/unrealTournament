/*
 * Copyright (c) 2008-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.  
  

#include "PsFoundation.h"
#include "PsSocket.h"
#include "PxMemory.h"

#define INVALID_SOCKET -1

namespace physx
{
namespace shdfnd
{

const PxU32 Socket::DEFAULT_BUFFER_SIZE  = 32768;

// There's no real socket implementation for HTML5. 

class SocketImpl {};

Socket::Socket( bool inIsBuffering )
{
	shdfnd::getFoundation().error(PxErrorCode::eABORT, __FILE__, __LINE__, " PhysX HTML5: Socket Implementation doesn't exist ");
}

Socket::~Socket()
{
}


bool Socket::connect(const char* host, PxU16 port, PxU32 timeout)
{
	return false; 
}

bool Socket::listen(PxU16 port, Connection *callback)
{
	return false; 
}


void Socket::disconnect()
{

}


bool Socket::isConnected() const
{
	return false;
}


const char* Socket::getHost() const
{
	return NULL;
}


PxU16 Socket::getPort() const
{
	return 0;
}


bool Socket::flush()
{
	return false; 
}


PxU32 Socket::write(const PxU8* data, PxU32 length)
{
	return 0; 
}


PxU32 Socket::read(PxU8* data, PxU32 length)
{
	return 0; 
}


void Socket::setBlocking(bool blocking)
{
}


bool Socket::isBlocking() const
{
	return false; 
}

} // namespace shdfnd
} // namespace physx
