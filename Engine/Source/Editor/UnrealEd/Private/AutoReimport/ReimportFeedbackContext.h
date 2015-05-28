// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FeedbackContext.h"
#include "SNotificationList.h"
#include "INotificationWidget.h"
#include "MessageLog.h"
#include "AutoReimportUtilities.h"

class SWidgetStack;

/** Feedback context that overrides GWarn for import operations to prevent popup spam */
class SReimportFeedback : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SReimportFeedback){}
	SLATE_END_ARGS()

	/** Construct this widget */
	void Construct(const FArguments& InArgs, FText InMainText);

	/** Add a widget to this feedback's widget stack */
	void Add(const TSharedRef<SWidget>& Widget);

	/** Disable input to this widget's dynamic content (except the message log hyperlink) */
	void Disable();

	/** Set the main text of this widget */
	void SetMainText(FText InText);
	
	/** Get the main text of this widget */
	FText GetMainText() const;

private:
	
	/** Get the visibility of the hyperlink to open the message log */
	EVisibility GetHyperlinkVisibility() const;

private:

	/** Cached main text for the notification */
	FText MainText;

	/** The widget stack, displaying contextural information about the current state of the process */
	TSharedPtr<SWidgetStack> WidgetStack;
};

/** Feedback context that overrides GWarn for import operations to prevent popup spam */
class FReimportFeedbackContext : public FFeedbackContext, public INotificationWidget, public TSharedFromThis<FReimportFeedbackContext>
{
public:
	/** Constructor */
	FReimportFeedbackContext();

	/** Initialize this reimport context with the specified widget */
	void Initialize(TSharedRef<SReimportFeedback> Widget);

	/** Destroy this reimport context */
	void Destroy();

	TSharedPtr<SReimportFeedback> GetContent() { return NotificationContent; }

	/** Get the message log that this context is using */
	FMessageLog& GetMessageLog() { return MessageLog; }

	/** Add a message to the context (and message log) */
	void AddMessage(EMessageSeverity::Type Severity, const FText& Message);

	/** Add a custom widget to the context */
	void AddWidget(const TSharedRef<SWidget>& Widget);

	/** INotificationWidget and FFeedbackContext overrides */
	virtual void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category ) override {}
	virtual void OnSetCompletionState(SNotificationItem::ECompletionState State) override {}
	virtual TSharedRef<SWidget> AsWidget() override { return NotificationContent.ToSharedRef(); };
	virtual void StartSlowTask( const FText& Task, bool bShowCancelButton=false) override;

	/** True if we will suppress slow task messages from being shown on the UI, false if otherwise */
	bool bSuppressSlowTaskMessages;

private:
	/** Message log for output of errors and log messages */
	FMessageLog MessageLog;

	/** The notification that is shown when the context is active */
	TSharedPtr<class SNotificationItem> Notification;

	/** The notification content */
	TSharedPtr<SReimportFeedback> NotificationContent;
};