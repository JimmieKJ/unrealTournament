// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "CoreUObject.h"
#include "Json.h"
#include "SlateCore.h"
#include "SlateClasses.h"
#include "SlateOptMacros.h"

#include "IPlatformTextField.h"
#if PLATFORM_IOS
	#include "IOS/IOSPlatformTextField.h"
#elif PLATFORM_ANDROID
	#include "Android/AndroidPlatformTextField.h"
#elif PLATFORM_PS4
	#include "PS4/PS4PlatformTextField.h"
#else
	#include "GenericPlatformTextField.h"
#endif

#include "InputCore.h"


#include "SlateDelegates.h"
#include "SlateFwd.h"

// Application
#include "SlateApplication.h"
#include "SlateIcon.h"

// Commands
#include "InputChord.h"
#include "UIAction.h"
#include "UICommandInfo.h"
#include "InputBindingManager.h"
#include "Commands.h"
#include "UICommandList.h"

// Legacy
class SMissingWidget;
class FSlateStyleRegistry;
class FUICommandDragDropOp;
class FInertialScrollManager;
class FGenericCommands;
#include "SWeakWidget.h"
#include "TextLayoutEngine.h"
#include "SPanel.h"
#include "SCompoundWidget.h"
#include "SFxWidget.h"
#include "SBorder.h"
#include "SSeparator.h"
#include "SSpacer.h"
#include "SWrapBox.h"
#include "SImage.h"
#include "SSpinningImage.h"
#include "SProgressBar.h"
#include "SCanvas.h"
#include "STextBlock.h"
#include "TextDecorators.h"
#include "SRichTextBlock.h"
#include "SBox.h"
#include "SHeader.h"
#include "SGridPanel.h"
#include "SUniformGridPanel.h"
#include "SMenuAnchor.h"
#include "MultiBoxDefs.h"
////
#include "MultiBox.h"
////
#include "MultiBoxBuilder.h"
#include "MultiBoxExtender.h"
#include "SMultiLineEditableText.h"
#include "SMultiLineEditableTextBox.h"
#include "SEditableText.h"
#include "SEditableTextBox.h"
class SSearchBox;
#include "SButton.h"
#include "SToolTip.h"
#include "SScrollBar.h"
#include "SScrollBorder.h"
#include "SErrorText.h"
#include "SErrorHint.h"
#include "SPopUpErrorText.h"
#include "SSplitter.h"
#include "TableViewTypeTraits.h"
#include "SExpanderArrow.h"
#include "ITypedTableView.h"
#include "STableViewBase.h"
#include "SHeaderRow.h"
#include "STableRow.h"
#include "SListView.h"
#include "STileView.h"
#include "STreeView.h"
#include "SScrollBox.h"
#include "SViewport.h"
#include "SColorBlock.h"
#include "SCheckBox.h"
#include "SSpinBox.h"
#include "SSlider.h"
class SVolumeControl;
class SColorSpectrum;
class SColorWheel;

class FToolBarButtonBlock;
class SToolBarButtonBlock;
class FToolBarComboButtonBlock;
class SToolBarComboButtonBlock;
class SPopup;
///
#include "SComboButton.h"
#include "SComboBox.h"
class SHyperlink;
class SRichTextHyperlink;
class SThrobber;
class SCircularThrobber;
class STextEntryPopup;
class STextComboPopup;
class SExpandableButton;
class SExpandableArea;
class SNotificationItem;
class SNotificationList;
class SWidgetSwitcher;
class SSuggestionTextBox;
template <typename ItemType> class SBreadcrumbTrail;
class STextComboBox;
template<typename NumericType> class SNumericEntryBox;
template<typename OptionType> class SEditableComboBox;
class FSlateNotificationManager;
class SDPIScaler;
class SInlineEditableTextBlock;
class SVirtualKeyboardEntry;
class SSafeZone;
struct FMarqueeRect;
class SRotatorInputBox;
class SVectorInputBox;
class SVirtualJoystick;
class SSearchBox;

// Docking Framework
#include "WorkspaceItem.h"
#include "TabManager.h"
#include "LayoutService.h"

enum ETabActivationCause : uint8;
enum ETabRole : uint8;
class SDockTab;
