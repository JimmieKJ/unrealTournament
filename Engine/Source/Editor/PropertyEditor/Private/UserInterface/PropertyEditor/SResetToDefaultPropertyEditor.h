// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Presentation/PropertyEditor/PropertyEditor.h"
#include "IDetailPropertyRow.h"

// Forward decl
class FResetToDefaultOverride;

/** Widget showing the reset to default value button */
class SResetToDefaultPropertyEditor : public SCompoundWidget
{

public:

	SLATE_BEGIN_ARGS( SResetToDefaultPropertyEditor )
		: _NonVisibleState( EVisibility::Hidden )
		{}
		SLATE_ARGUMENT( EVisibility, NonVisibleState )
		SLATE_ARGUMENT(TOptional<FResetToDefaultOverride>, CustomResetToDefault)
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const TSharedRef< class FPropertyEditor>& InPropertyEditor );


private:
	FText GetResetToolTip() const;

	EVisibility GetDiffersFromDefaultAsVisibility() const;

	void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

	FReply OnDefaultResetClicked();
	FReply OnCustomResetClicked();

	void UpdateDiffersFromDefaultState();
private:
	TOptional<FResetToDefaultOverride> OptionalCustomResetToDefault;

	TSharedPtr< class FPropertyEditor > PropertyEditor;

	EVisibility NonVisibleState;

	bool bValueDiffersFromDefault;
};
