// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#ifndef __DistCurveEditorModule_h__
#define __DistCurveEditorModule_h__

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "IDistCurveEditor.h"

class UInterpCurveEdSetup;

extern const FName DistCurveEditorAppIdentifier;


/*-----------------------------------------------------------------------------
   IDistributionCurveEditorModule
-----------------------------------------------------------------------------*/

class IDistributionCurveEditorModule : public IModuleInterface
{
public:
	/**  */
	virtual TSharedRef<IDistributionCurveEditor> CreateCurveEditorWidget(UInterpCurveEdSetup* EdSetup, FCurveEdNotifyInterface* NotifyObject) = 0;
	virtual TSharedRef<IDistributionCurveEditor> CreateCurveEditorWidget(UInterpCurveEdSetup* EdSetup, FCurveEdNotifyInterface* NotifyObject, IDistributionCurveEditor::FCurveEdOptions Options) = 0;
};

#endif // __DistCurveEditorModule_h__
