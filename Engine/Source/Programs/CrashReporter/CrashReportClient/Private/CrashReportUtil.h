// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

template <class TWorker>
class TOneShotTaskUsingDedicatedThread : FRunnable
{
public:
	/**
	 * Constructor
	 */
	TOneShotTaskUsingDedicatedThread()
		: bIsDone(1)
	{
	}

	/**
	 * Destructor
	 */
	~TOneShotTaskUsingDedicatedThread()
	{
		CRASHREPORTCLIENT_CHECK(!!bIsDone);
	}


	template<typename T1, typename T2, typename T3, typename T4, typename T5>
	TWorker& GetTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4, T5 Arg5 )
	{
		Worker = new TWorker( Arg1, Arg2, Arg3, Arg4, Arg5 );
		return *Worker;
	}

	void StartBackgroundTask()
	{
		bIsDone = 0;
		Thread = FRunnableThread::Create( this, Worker->Name() );
	}

	/**
	 * No need to join the thread, since this can't be reused
	 */
	bool IsDone() const
	{
		return bIsDone == 1;
	}

private:
	virtual uint32 Run() override
	{
		Worker->DoWork();
		FPlatformAtomics::InterlockedIncrement(&bIsDone);
		return 0;
	}

	TAutoPtr<TWorker> Worker;
	TScopedPointer<FRunnableThread> Thread;
	volatile int32 bIsDone;	
};


/**
 * Helper class for MakeDirectoryVisitor
 */
template <class FunctorType>
class FunctorDirectoryVisitor : public IPlatformFile::FDirectoryVisitor
{
public:
	/**
	 * Pass a directory or filename on to the user-provided functor
	 * @param FilenameOrDirectory Full path to a file or directory
	 * @param bIsDirectory Whether the path refers to a file or directory
	 * @return Whether to carry on iterating
	 */
	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
	{
		return Functor(FilenameOrDirectory, bIsDirectory);
	}

	/**
	 * Move the provided functor into this object
	 */
	FunctorDirectoryVisitor(FunctorType&& FunctorInstance)
		: Functor(MoveTemp(FunctorInstance))
	{
	}

private:
	/** User-provided functor */
	FunctorType Functor;
};

/**
 * Convert a C++ functor into a IPlatformFile visitor object
 * @param FunctorInstance Function object to call for each directory item
 * @return Visitor object to be passed to platform directory visiting functions
 */
template <class Functor>
FunctorDirectoryVisitor<Functor> MakeDirectoryVisitor(Functor&& FunctorInstance)
{
	return FunctorDirectoryVisitor<Functor>(MoveTemp(FunctorInstance));
}

struct FCrashReportUtil
{
	/**
	 * Create a multi line string to display from an exception and callstack
	 * @param Exception Exception description string
	 * @param Assertion Assertion description string
	 * @param Callstack List of callstack entry strings
	 * @return Multiline text
	 */
	static inline FText FormatReportDescription( const FString& Exception, const FString& Assertion, const TArray<FString>& Callstack )
	{
		FString Diagnostic = Exception + "\n\n";

		if( !Assertion.IsEmpty() )
		{
			TArray<FString> MultilineAssertion;
			Assertion.ParseIntoArray( &MultilineAssertion, TEXT( "#" ), true );

			for( const auto& AssertionLine : MultilineAssertion )
			{
				Diagnostic += AssertionLine;
				Diagnostic += "\n";
			}

			Diagnostic += "\n";
		}

		for( const auto& Line : Callstack )
		{
			Diagnostic += Line + "\n";
		}
		return FText::FromString( Diagnostic );
	}

	/** Formats processed diagnostic text by adding additional information about machine and user. */
	static FText FormatDiagnosticText( const FText& DiagnosticText, const FString MachineId, const FString EpicAccountId, const FString UserNameNoDot );
};
