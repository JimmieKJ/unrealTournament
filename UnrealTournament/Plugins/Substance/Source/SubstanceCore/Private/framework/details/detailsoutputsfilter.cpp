//! @file detailsoutputsfilter.cpp
//! @brief Substance Framework Outputs filtering structure impl.
//! @author Christophe Soum - Allegorithmic
//! @date 20111107
//! @copyright Allegorithmic. All rights reserved.
//!

#include "SubstanceCorePrivatePCH.h"

#include "framework/details/detailsoutputsfilter.h"
#include "framework/details/detailsrenderjob.h"


//! @brief Constructor from RenderJob contains the list of outputs to filter
Substance::Details::OutputsFilter::OutputsFilter(const RenderJob& src)
{
	src.fill(*this);
}

