//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include "ModuleInterface.h"

/**
 * The public interface of the SubstanceCore module
 */
class ISubstanceCore : public IModuleInterface
{
public:
	virtual unsigned int GetMaxOutputTextureSize() const = 0;
};

