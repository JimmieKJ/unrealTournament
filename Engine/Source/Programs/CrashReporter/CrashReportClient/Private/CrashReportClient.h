// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CrashUpload.h"
#include "PlatformErrorReport.h"
#include "CrashReportUtil.h"

/**
 * Helper task class to process a crash report in the background
 */
class FDiagnoseReportWorker 
{
public:
	/** Diagnostics from crash report text to fill in. */
	FText& DiagnosticText;

	/** Error report files to use. */
	const FPlatformErrorReport& ErrorReport;

	/** Machine ID. */
	const FString MachineId;

	/** Epic Account ID. */
	const FString EpicAccountId;

	/** User name without dot. */
	const FString UserNameNoDot;

	/** Initialization constructor. */
	FDiagnoseReportWorker( FText* InDiagnosticText, const FString InMachineId, const FString InEpicAccountId, const FString InUserNameNoDot, const FPlatformErrorReport* InErrorReport )
		: DiagnosticText( *InDiagnosticText )		
		, ErrorReport( *InErrorReport )
		, MachineId(InMachineId)
		, EpicAccountId(InEpicAccountId)
		, UserNameNoDot(InUserNameNoDot)
	{}

	/**
	 * Do platform-specific work to get information about the crash.
	 */
	void DoWork();

	/** 
	 * @return The name to display in external event viewers
	 */
	static const TCHAR* Name()
	{
		return TEXT( "FDiagnoseCrashWorker" );
	}
};

#if !CRASH_REPORT_UNATTENDED_ONLY

/**
 * Main implementation of the crash report client application
 */
class FCrashReportClient : public TSharedFromThis<FCrashReportClient>
{
public:
	/**
	 * Constructor: sets up background diagnosis
	 * @param ErrorReport Error report to upload
	 */
	FCrashReportClient( const FPlatformErrorReport& InErrorReport );

	/**
	 * Respond to the user pressing Submit
	 * @return Whether the request was handled
	 */
	FReply Submit();

	/**
	 * Respond to the user pressing Cancel or Close
	 * @return Whether the request was handled
	 */
	FReply Cancel();

	/**
	 * Respond to the user requesting the callstack to be copied to the clipboard
	 * @return Whether the request was handled
	 */
	FReply CopyCallstack();

	/**
	 * Provide text to display at the bottom of the app
	 * @return Localized text to display
	 */
	FText GetStatusText() const;

	/**
	 * Tell UI to display 'Cancel' or 'Close'
	 * @return Localized text to display
	 */
	FText GetCancelButtonText() const;

	/**
	 * Pass on exception and callstack from the platform error report code
	 * @return Localized text to display
	 */
	FText GetDiagnosticText() const;

	/**
	 * Indicate availability of Submit button
	 * @return Whether button should be visible
	 */
	EVisibility SubmitButtonVisibility() const;

	/**
	 * Access to name (which usually includes the build config) of the app that crashed
	 * @return Name of executable
	 */
	FString GetCrashedAppName() const;

	/**
	 * Handle the user updating the user comment text
	 * @param Comment Text provided by the user
	 * @param CommitType Event that caused this update
	 */
	void UserCommentChanged(const FText& Comment, ETextCommit::Type CommitType);

	/**
	 * Indication of whether the app should be visible
	 * @return Whether the main window should be hidden
	 */
	bool ShouldWindowBeHidden() const;

	/**
	 * Handle user closing the main window: same behaviour as Cancel
	 * @param Window Main window
	 */
	void RequestCloseWindow(const TSharedRef<SWindow>& Window);

private:
	/**
	 * Write the user's comment to the report and begin uploading the entire report 
	 */
	void StoreCommentAndUpload();

	/**
	 * Update received every second
	 * @param DeltaTime Time since last update, unused
	 * @return Whether the updates should continue
	 */
	bool UIWillCloseTick(float DeltaTime);

	/**
	 * Begin calling UIWillCloseTick once a second
	 */
	void StartUIWillCloseTicker();

	/** State enum to keep track of what the app is doing */
	struct EApplicationState
	{
		enum Type
		{
			Ready,				/** Waiting for user input */
			CountingDown,		/** Submit pressed; counting down */
			Closing				/** UI hidden, possibly waiting for tasks to finish */
		};
	};

	/** What the app is currently doing */
	EApplicationState::Type AppState;

	/** Count-down until closing the app after user has pressed Submit */
	int SubmittedCountdown;

	/** Comment provided by the user */
	FText UserComment;

	/** Exception and call-stack to show, valid once diagnosis task is complete */
	FText DiagnosticText;

	/** Flag to remember having told the crash uploader to send the diagnostic file */
	bool bDiagnosticFileSent;

	/** Background worker to get a callstack from the report */
	TOneShotTaskUsingDedicatedThread<FDiagnoseReportWorker> DiagnoseReportTask;

	/** Platform code for accessing the report */
	FPlatformErrorReport ErrorReport;

	/** Object that uploads report files to the server */
	FCrashUpload Uploader;

	/** Text for Cancel/Close button, including count-down value */
	FText CancelButtonText;
};

#endif // !CRASH_REPORT_UNATTENDED_ONLY
