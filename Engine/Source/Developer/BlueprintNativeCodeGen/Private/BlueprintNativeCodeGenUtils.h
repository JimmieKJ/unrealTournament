// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FeedbackContext.h"

// Forward declares
struct FNativeCodeGenCommandlineParams;
class  SBuildProgressWidget;

DECLARE_LOG_CATEGORY_EXTERN(LogBlueprintCodeGen, Log, All);

/**  */
struct FBlueprintNativeCodeGenUtils
{
public:
	/**
	 * 
	 * 
	 * @param  CommandParams    
	 * @return 
	 */
	static bool GeneratePlugin(const FNativeCodeGenCommandlineParams& CommandParams);

	/**
	 * Recompiles the bytecode of a blueprint only. Should only be run for
	 * recompiling dependencies during compile on load.
	 * 
	 * @param  Obj    
	 * @param  OutHeaderSource    
	 * @param  OutCppSource    
	 */
	static void GenerateCppCode(UObject* Obj, TSharedPtr<FString> OutHeaderSource, TSharedPtr<FString> OutCppSource);

public: 
	/** 
	 * 
	 */
	class FScopedFeedbackContext : public FFeedbackContext
	{
	public:
		FScopedFeedbackContext();
		virtual ~FScopedFeedbackContext();

		/**  */
		bool HasErrors();

		// FOutputDevice interface
		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override;
		virtual void Flush() override;
		virtual void TearDown() override { OldContext->TearDown();  }
		virtual bool CanBeUsedOnAnyThread() const override { return false; }
		// End FOutputDevice interface

		// FFeedbackContext interface
		virtual bool YesNof(const FText& Question) override { return OldContext->YesNof(Question); }
		virtual bool ReceivedUserCancel() override { return OldContext->ReceivedUserCancel(); }
		virtual FContextSupplier* GetContext() const override { return OldContext->GetContext(); }
		virtual void SetContext(FContextSupplier* InSupplier) override { OldContext->SetContext(InSupplier); }
		virtual TWeakPtr<SBuildProgressWidget> ShowBuildProgressWindow() override { return OldContext->ShowBuildProgressWindow(); }
		virtual void CloseBuildProgressWindow() override { OldContext->CloseBuildProgressWindow(); }

	protected:
		//virtual void StartSlowTask(const FText& Task, bool bShowCancelButton = false) override { OldContext->StartSlowTask(Task, bShowCancelButton); }
		//virtual void FinalizeSlowTask() override { OldContext->FinalizeSlowTask(); }
		//virtual void ProgressReported(const float TotalProgressInterp, FText DisplayMessage) override { OldContext->ProgressReported(TotalProgressInterp, DisplayMessage); }
		// End FFeedbackContext interface

	private:
		FFeedbackContext* OldContext;
		int32 ErrorCount;
		int32 WarningCount;
	};
};

