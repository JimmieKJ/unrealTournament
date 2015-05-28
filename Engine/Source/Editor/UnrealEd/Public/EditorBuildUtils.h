// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EditorBuildUtils.h: Utilities for building in the editor
=============================================================================*/

#ifndef __EditorBuildUtils_h__
#define __EditorBuildUtils_h__

namespace EBuildOptions
{
	enum Type
	{
		/** Build all geometry */
		BuildGeometry,
		/** Build only visible geometry */
		BuildVisibleGeometry,
		/** Build lighting */
		BuildLighting,
		/** Build all AI paths */
		BuildAIPaths,
		/** Build only selected AI paths */
		BuildSelectedAIPaths,
		/** Build everything */
		BuildAll,
		/** Build everything and submit to source control */
		BuildAllSubmit,
		/** Build everything except for paths only build seleced */
		BuildAllOnlySelectedPaths,
		/** Build Hierarchical LOD system - need WorldSetting setup*/
		BuildHierarchicalLOD, 
	};
}

/** Utility class to hold functionality for building within the editor */
class FEditorBuildUtils
{
public:
	/** Enumeration representing automated build behavior in the event of an error */
	enum EAutomatedBuildBehavior
	{
		ABB_PromptOnError,	// Modally prompt the user about the error and ask if the build should proceed
		ABB_FailOnError,	// Fail and terminate the automated build in response to the error
		ABB_ProceedOnError	// Acknowledge the error but continue with the automated build in spite of it
	};

	/** Helper struct to specify settings for an automated editor build */
	struct UNREALED_API FEditorAutomatedBuildSettings
	{
		/** Constructor */
		FEditorAutomatedBuildSettings();

		/** Behavior to take when a map build results in map check errors */
		EAutomatedBuildBehavior BuildErrorBehavior;

		/** Behavior to take when a map file cannot be checked out for some reason */
		EAutomatedBuildBehavior UnableToCheckoutFilesBehavior;

		/** Behavior to take when a map is discovered which has never been saved before */
		EAutomatedBuildBehavior NewMapBehavior;

		/** Behavior to take when a saveable map fails to save correctly */
		EAutomatedBuildBehavior FailedToSaveBehavior;

		/** If true, built map files not already in the source control depot will be added */
		bool bAutoAddNewFiles;

		/** If true, the editor will shut itself down upon completion of the automated build */
		bool bShutdownEditorOnCompletion;

		/** If true, the editor will check in all checked out packages */
		bool bCheckInPackages;

		/** Populate list with selected packages to check in */
		TArray<FString> PackagesToCheckIn;

		/** Changelist description to use for the submission of the automated build */
		FString ChangeDescription;
	};

	/**
	 * Start an automated build of all current maps in the editor. Upon successful conclusion of the build, the newly
	 * built maps will be submitted to source control.
	 *
	 * @param	BuildSettings		Build settings used to dictate the behavior of the automated build
	 * @param	OutErrorMessages	Error messages accumulated during the build process, if any
	 *
	 * @return	true if the build/submission process executed successfully; false if it did not
	 */
	static UNREALED_API bool EditorAutomatedBuildAndSubmit( const FEditorAutomatedBuildSettings& BuildSettings, FText& OutErrorMessages );

	/**
	 * Perform an editor build with behavior dependent upon the specified id
	 *
	 * @param	InWorld				WorldContext
	 * @param	Id					Action Id specifying what kind of build is requested
	 * @param	bAllowLightingDialog True if the build lighting dialog should be displayed if we're building lighting only
	 *
	 * @return	true if the build completed successfully; false if it did not (or was manually canceled)
	 */
	static UNREALED_API bool EditorBuild( UWorld* InWorld, EBuildOptions::Type Id, const bool bAllowLightingDialog = true );

	/** 
	* check if navigation build was was triggered from editor as user request
	*
	* @return	true if the build was triggered as user request; false if it did not 
	*/
	static UNREALED_API bool IsBuildingNavigationFromUserRequest() { return bBuildingNavigationFromUserRequest; }

	/** 
	* call it to notify that navigation builder finished building 
	*/
	static UNREALED_API void PathBuildingFinished() { bBuildingNavigationFromUserRequest = false; }
private:

	/**
	 * Private helper method to log an error both to GWarn and to the build's list of accumulated errors
	 *
	 * @param	InErrorMessage			Message to log to GWarn/add to list of errors
	 * @param	OutAccumulatedErrors	List of errors accumulated during a build process so far
	 */
	static void LogErrorMessage( const FText& InErrorMessage, FText& OutAccumulatedErrors );

	/**
	 * Helper method to handle automated build behavior in the event of an error. Depending on the specified behavior, one of three
	 * results are possible:
	 *	a) User is prompted on whether to proceed with the automated build or not,
	 *	b) The error is regarded as a build-stopper and the method returns failure,
	 *	or
	 *	c) The error is acknowledged but not regarded as a build-stopper, and the method returns success.
	 * In any event, the error is logged for the user's information.
	 *
	 * @param	InBehavior				Behavior to use to respond to the error
	 * @param	InErrorMsg				Error to log
	 * @param	OutAccumulatedErrors	List of errors accumulated from the build process so far; InErrorMsg will be added to the list
	 *
	 * @return	true if the build should proceed after processing the error behavior; false if it should not
	 */
	static bool ProcessAutomatedBuildBehavior( EAutomatedBuildBehavior InBehavior, const FText& InErrorMsg, FText& OutAccumulatedErrors );

	/**
	 * Helper method designed to perform the necessary preparations required to complete an automated editor build
	 *
	 * @param	BuildSettings		Build settings that will be used for the editor build
	 * @param	OutPkgsToSubmit		Set of packages that need to be saved and submitted after a successful build
	 * @param	OutErrorMessages	Errors that resulted from the preparation (may or may not force the build to stop, depending on build settings)
	 *
	 * @return	true if the preparation was successful and the build should continue; false if the preparation failed and the build should be aborted
	 */
	static bool PrepForAutomatedBuild( const FEditorAutomatedBuildSettings& BuildSettings, TSet<UPackage*>& OutPkgsToSubmit, FText& OutErrorMessages );

	/**
	 * Helper method to submit packages to source control as part of the automated build process
	 *
	 * @param	InPkgsToSubmit	Set of packages which should be submitted to source control
	 * @param	BuildSettings	Build settings used during the automated build
	 */
	static void SubmitPackagesForAutomatedBuild( const TSet<UPackage*>& InPkgsToSubmit, const FEditorAutomatedBuildSettings& BuildSettings );

	/** 
	 * Trigger navigation builder to (re)generate NavMesh 
	 *
	 * @param	InWorld			WorldContext
	 * @param	BuildSettings	Build settings that will be used for the editor build
	 */
	static void TriggerNavigationBuilder(UWorld* InWorld, EBuildOptions::Type Id);

	/** 
	 * Trigger LOD builder to (re)generate LODActors
	 *
	 * @param	InWorld			WorldContext
	 * @param	BuildSettings	Build settings that will be used for the editor build
	 */
	static void TriggerHierarchicalLODBuilder(UWorld* InWorld, EBuildOptions::Type Id);

	/** Intentionally hide constructors, etc. to prevent instantiation */
	FEditorBuildUtils();
	~FEditorBuildUtils();
	FEditorBuildUtils( const FEditorBuildUtils& );
	FEditorBuildUtils operator=( const FEditorBuildUtils& );

	/** static variable to cache data about user request. navigation builder works in a background and we have to cache this information */
	static bool bBuildingNavigationFromUserRequest;
};

#endif // #ifndef __EditorBuildUtils_h__
