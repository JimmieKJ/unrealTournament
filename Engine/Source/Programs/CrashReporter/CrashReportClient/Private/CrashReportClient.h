// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CrashUpload.h"
#include "PlatformErrorReport.h"
#include "CrashReportUtil.h"

#if !CRASH_REPORT_UNATTENDED_ONLY

class FCrashReportClient;

/**
 * Helper task class to process a crash report in the background
 */
class FDiagnoseReportWorker  : public FNonAbandonableTask
{
public:
	/** Pointer to the crash report client, used to store the results. */
	FCrashReportClient* CrashReportClient;

	/** Initialization constructor. */
	FDiagnoseReportWorker( FCrashReportClient* InCrashReportClient );

	/**
	 * Do platform-specific work to get information about the crash.
	 */
	void DoWork();

	FORCEINLINE TStatId GetStatId() const
	{
		return TStatId();
	}

	/** 
	 * @return The name to display in external event viewers
	 */
	static const TCHAR* Name()
	{
		return TEXT( "FDiagnoseCrashWorker" );
	}
};

/**
 * Main implementation of the crash report client application
 */
class FCrashReportClient : public TSharedFromThis<FCrashReportClient>
{
	friend class FDiagnoseReportWorker;

public:
	/**
	 * Constructor: sets up background diagnosis
	 * @param ErrorReport Error report to upload
	 */
	FCrashReportClient( const FPlatformErrorReport& InErrorReport );

	/** Destructor. */
	virtual ~FCrashReportClient();

	/**
	 * Respond to the user pressing Submit
	 * @return Whether the request was handled
	 */
	FReply Submit();

	/**
	* Respond to the user pressing Submit and Restart
	* @return Whether the request was handled
	*/
	FReply SubmitAndRestart();

	/**
	 * Respond to the user requesting the callstack to be copied to the clipboard
	 * @return Whether the request was handled
	 */
	FReply CopyCallstack();

	/**
	 * Pass on exception and callstack from the platform error report code
	 * @return Localized text to display
	 */
	FText GetDiagnosticText() const;

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
	 * Handle user closing the main window
	 * @param Window Main window
	 */
	void RequestCloseWindow(const TSharedRef<SWindow>& Window);

	/** Whether the main window should be hidden. */
	bool ShouldWindowBeHidden() const
	{
		return bShouldWindowBeHidden;
	}

	/** Whether the app should enable widgets related to the displayed callstack. */
	bool AreCallstackWidgetsEnabled() const;

	/** Whether the throbber should be visible while processing the callstack. */
	EVisibility IsThrobberVisible() const;

	void SCrashReportClient_OnCheckStateChanged( ECheckBoxState NewRadioState );

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
	bool Tick(float DeltaTime);

	/**
	 * Begin calling Tick once a second
	 */
	void StartTicker();

	/** Enqueued from the diagnose report worker thread to be executed on the game thread. */
	void FinalizeDiagnoseReportWorker( FText ReportText );

	/**
	 * @return true if we are still processing a callstack
	 */
	bool IsProcessingCallstack() const;

	/** Comment provided by the user */
	FText UserComment;

	/** Exception and call-stack to show, valid once diagnosis task is complete */
	FText DiagnosticText;

	/** Background worker to get a callstack from the report */
	FAsyncTask<FDiagnoseReportWorker>* DiagnoseReportTask;

	/** Platform code for accessing the report */
	FPlatformErrorReport ErrorReport;

	/** Object that uploads report files to the server */
	FCrashUpload Uploader;

	/** Whether BeginUpload has been called. */
	bool bBeginUploadCalled;

	/** Whether the main window should be hidden. */
	bool bShouldWindowBeHidden;

	/** Whether the user allowed us to be contacted. */
	bool bAllowToBeContacted;

	/** Whether we send the data. */
	bool bSendData;

};

#endif // !CRASH_REPORT_UNATTENDED_ONLY
