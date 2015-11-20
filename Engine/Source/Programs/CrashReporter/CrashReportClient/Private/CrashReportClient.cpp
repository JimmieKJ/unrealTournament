// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashReportClientApp.h"
#include "CrashReportUtil.h"

#include "CrashDescription.h"

#include "CrashReportClient.h"
#include "UniquePtr.h"

#include "TaskGraphInterfaces.h"

#define LOCTEXT_NAMESPACE "CrashReportClient"

struct FCrashReportUtil
{
	/** Formats processed diagnostic text by adding additional information about machine and user. */
	static FText FormatDiagnosticText( const FText& DiagnosticText )
	{
		const FString MachineId = FPrimaryCrashProperties::Get()->MachineId.AsString();
		const FString EpicAccountId = FPrimaryCrashProperties::Get()->EpicAccountId.AsString();
		return FText::Format( LOCTEXT( "CrashReportClientCallstackPattern", "MachineId:{0}\nEpicAccountId:{1}\n\n{2}" ), FText::FromString( MachineId ), FText::FromString( EpicAccountId ), DiagnosticText );
	}
};

#if !CRASH_REPORT_UNATTENDED_ONLY

FCrashReportClient::FCrashReportClient(const FPlatformErrorReport& InErrorReport)
	: DiagnosticText( LOCTEXT("ProcessingReport", "Processing crash report ...") )
	, DiagnoseReportTask(nullptr)
	, ErrorReport( InErrorReport )
	, Uploader( FCrashReportClientConfig::Get().GetReceiverAddress() )
	, bBeginUploadCalled(false)
	, bShouldWindowBeHidden(false)
	, bSendData(false)
{
	if (FPrimaryCrashProperties::Get()->IsValid())
	{
		bool bUsePrimaryData = false;
		if (FPrimaryCrashProperties::Get()->HasProcessedData())
		{
			bUsePrimaryData = true;
		}
		else
		{
			if (!ErrorReport.TryReadDiagnosticsFile() && !FParse::Param( FCommandLine::Get(), TEXT( "no-local-diagnosis" ) ))
			{
				DiagnoseReportTask = new FAsyncTask<FDiagnoseReportWorker>( this );
				DiagnoseReportTask->StartBackgroundTask();
			}
			else
			{
				bUsePrimaryData = true;
			}
		}

		if (bUsePrimaryData)
		{
			const FString CallstackString = FPrimaryCrashProperties::Get()->CallStack.AsString();
			const FString ReportString = FString::Printf( TEXT( "%s\n\n%s" ), *FPrimaryCrashProperties::Get()->ErrorMessage.AsString(), *CallstackString );
			DiagnosticText = FText::FromString( ReportString );

			FormattedDiagnosticText = FCrashReportUtil::FormatDiagnosticText( FText::FromString( ReportString ) );
		}
	}
}


FCrashReportClient::~FCrashReportClient()
{
	if( DiagnoseReportTask )
	{
		DiagnoseReportTask->EnsureCompletion();
		delete DiagnoseReportTask;
	}
}

FReply FCrashReportClient::CloseWithoutSending()
{
	GIsRequestingExit = true;
	return FReply::Handled();
}

FReply FCrashReportClient::Submit()
{
	if (ErrorReport.HasFilesToUpload())
	{
		// Send analytics.
		FPrimaryCrashProperties::Get()->SendAnalytics();
	}

	bSendData = true;
	StoreCommentAndUpload();
	bShouldWindowBeHidden = true;
	return FReply::Handled();
}

FReply FCrashReportClient::SubmitAndRestart()
{
	Submit();

	const FString CrashedAppPath = ErrorReport.FindCrashedAppPath();
	const FString CommandLineArguments = FPrimaryCrashProperties::Get()->CommandLine.AsString();

	FPlatformProcess::CreateProc(*CrashedAppPath, *CommandLineArguments, true, false, false, NULL, 0, NULL, NULL);

	return FReply::Handled();
}

FReply FCrashReportClient::CopyCallstack()
{
	FPlatformMisc::ClipboardCopy(*DiagnosticText.ToString());
	return FReply::Handled();
}

FText FCrashReportClient::GetDiagnosticText() const
{
	return FormattedDiagnosticText;
}

void FCrashReportClient::UserCommentChanged(const FText& Comment, ETextCommit::Type CommitType)
{
	UserComment = Comment;

	// Implement Shift+Enter to commit shortcut
	if (CommitType == ETextCommit::OnEnter && FSlateApplication::Get().GetModifierKeys().IsShiftDown())
	{
		Submit();
	}
}

void FCrashReportClient::RequestCloseWindow(const TSharedRef<SWindow>& Window)
{
	// Don't send the data.
	bSendData = false;

	// We may still processing minidump etc. so start the main ticker.
	StartTicker();
	bShouldWindowBeHidden = true;
}

bool FCrashReportClient::AreCallstackWidgetsEnabled() const
{
	return !IsProcessingCallstack();
}

EVisibility FCrashReportClient::IsThrobberVisible() const
{
	return IsProcessingCallstack() ? EVisibility::Visible : EVisibility::Hidden;
}

void FCrashReportClient::AllowToBeContacted_OnCheckStateChanged( ECheckBoxState NewRadioState )
{
	FCrashReportClientConfig::Get().SetAllowToBeContacted( NewRadioState == ECheckBoxState::Checked );

	// Refresh PII based on the bAllowToBeContacted flag.
	FPrimaryCrashProperties::Get()->UpdateIDs();

	// Save updated properties.
	FPrimaryCrashProperties::Get()->Save();

	// Update diagnostics text.
	FormattedDiagnosticText = FCrashReportUtil::FormatDiagnosticText( DiagnosticText );
}

void FCrashReportClient::SendLogFile_OnCheckStateChanged( ECheckBoxState NewRadioState )
{
	FCrashReportClientConfig::Get().SetSendLogFile( NewRadioState == ECheckBoxState::Checked );
}

void FCrashReportClient::StartTicker()
{
	FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &FCrashReportClient::Tick), 1.f);
}

void FCrashReportClient::StoreCommentAndUpload()
{
	// Write user's comment
	ErrorReport.SetUserComment( UserComment );
	StartTicker();
}

bool FCrashReportClient::Tick(float UnusedDeltaTime)
{
	// We are waiting for diagnose report task to complete.
	if( IsProcessingCallstack() )
	{
		return true;
	}
	
	if( bSendData )
	{
		if( !bBeginUploadCalled )
		{
			// Can be called only when we have all files.
			Uploader.BeginUpload( ErrorReport );
			bBeginUploadCalled = true;
		}

		// IsWorkDone will always return true here (since uploader can't finish until the diagnosis has been sent), but it
		//  has the side effect of joining the worker thread.
		if( !Uploader.IsFinished() )
		{
			// More ticks, please
			return true;
		}
	}

	FPlatformMisc::RequestExit(false);
	return false;
}

FString FCrashReportClient::GetCrashDirectory() const
{
	return ErrorReport.GetReportDirectory();
}

void FCrashReportClient::FinalizeDiagnoseReportWorker()
{
	// Update properties for the crash.
	ErrorReport.SetPrimaryCrashProperties( *FPrimaryCrashProperties::Get() );

	FString CallstackString = FPrimaryCrashProperties::Get()->CallStack.AsString();
	if (CallstackString.IsEmpty())
	{
		DiagnosticText = LOCTEXT( "NoDebuggingSymbols", "You do not have any debugging symbols required to display the callstack for this crash." );
	}
	else
	{
		const FString ReportString = FString::Printf( TEXT( "%s\n\n%s" ), *FPrimaryCrashProperties::Get()->ErrorMessage.AsString(), *CallstackString );
		DiagnosticText = FText::FromString( ReportString );
	}

	FormattedDiagnosticText = FCrashReportUtil::FormatDiagnosticText( DiagnosticText );
}


bool FCrashReportClient::IsProcessingCallstack() const
{
	return DiagnoseReportTask && !DiagnoseReportTask->IsWorkDone();
}

FDiagnoseReportWorker::FDiagnoseReportWorker( FCrashReportClient* InCrashReportClient ) 
	: CrashReportClient( InCrashReportClient )
{}

void FDiagnoseReportWorker::DoWork()
{
	CrashReportClient->ErrorReport.DiagnoseReport();

	// Inform the game thread that we are done.
	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
	(
		FSimpleDelegateGraphTask::FDelegate::CreateRaw( CrashReportClient, &FCrashReportClient::FinalizeDiagnoseReportWorker ),
		TStatId(), nullptr, ENamedThreads::GameThread
	);
}

#endif // !CRASH_REPORT_UNATTENDED_ONLY

#undef LOCTEXT_NAMESPACE
