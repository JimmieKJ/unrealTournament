// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * This file exists for backward compatibility. Slate.h has been split from one monolithic header
 * into SlateBasics.h and SlateExtras.h. Over time more files will be shifted from SlateBasic.h to
 * SlateExtras.h so older code using Slate.h will not be affected.
 * 
 * However, no one should include SlateExtras.h. It exists purely to assist migration of legacy projects.
 */

#include "SlateBasics.h"

// Legacy
#include "SMissingWidget.h"
#include "SlateScrollHelper.h"
#include "SlateStyleRegistry.h"
#include "UICommandDragDropOp.h"
#include "InertialScrollManager.h"
#include "GenericCommands.h"

#include "SSearchBox.h"
#include "SVolumeControl.h"
#include "SColorSpectrum.h"
#include "SColorWheel.h"
///
#include "SToolBarButtonBlock.h"
#include "SToolBarComboButtonBlock.h"
///
#include "SHyperlink.h"
#include "SRichTextHyperlink.h"
#include "SThrobber.h"
#include "STextEntryPopup.h"
#include "STextComboPopup.h"
#include "SExpandableButton.h"
#include "SExpandableArea.h"
#include "SNotificationList.h"
#include "SWidgetSwitcher.h"
#include "SSuggestionTextBox.h"
#include "SBreadcrumbTrail.h"
#include "STextComboBox.h"
#include "SNumericEntryBox.h"
#include "SEditableComboBox.h"
#include "NotificationManager.h"
#include "SDPIScaler.h"
#include "SInlineEditableTextBlock.h"
#include "SVirtualKeyboardEntry.h"
#include "ScrollyZoomy.h"
#include "SSafeZone.h"
#include "MarqueeRect.h"
#include "SRotatorInputBox.h"
#include "SVectorInputBox.h"
#include "SVirtualJoystick.h"

// Docking Framework
#include "SDockTab.h"
#include "SDockableTab.h"
#include "SDockTabStack.h"
