// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


// Core includes.
#include "CoreUObjectPrivate.h"
#include "UObject/UObjectThreadContext.h"
//#include "CookStats.h"
DEFINE_LOG_CATEGORY_STATIC(LogRedirectors, Log, All);

static TArray<FString> StringAssetRefFilenameStack;

void FRedirectCollector::OnRedirectorFollowed(const FString& InString, UObject* InObject)
{
	check(InObject);

	// the object had better be a redir
	UObjectRedirector& Redirector = dynamic_cast<UObjectRedirector&>(*InObject);

	// save the info if it matches the parameters we need to match on:
	// if the string matches the package we were loading
	// AND
	// we aren't matching a particular package || the string matches the package
	if (!FileToFixup.Len() || FileToFixup == FPackageName::FilenameToLongPackageName(Redirector.GetLinker()->Filename))
	{
		FRedirection Redir;
		Redir.PackageFilename = InString;
		Redir.RedirectorName = Redirector.GetFullName();
		Redir.RedirectorPackageFilename = Redirector.GetLinker()->Filename;
		Redir.DestinationObjectName = Redirector.DestinationObject->GetFullName();
		// we only want one of each redirection reported
		Redirections.AddUnique(Redir);

		// and add a fake reference caused by a string redirectors
		if (StringAssetRefFilenameStack.Num())
		{
			Redir.PackageFilename = StringAssetRefFilenameStack[StringAssetRefFilenameStack.Num() - 1];
			UE_LOG(LogRedirectors, Verbose, TEXT("String Asset Reference fake redir %s(%s) -> %s"), *Redir.RedirectorName, *Redir.PackageFilename, *Redir.DestinationObjectName);
			if (!Redir.PackageFilename.IsEmpty())
			{
				Redirections.AddUnique(Redir);
			}
		}
	}
}

void FRedirectCollector::OnStringAssetReferenceLoaded(const FString& InString)
{
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	FPackagePropertyPair ContainingPackageAndProperty;

	if (InString.IsEmpty())
	{
		// No need to track empty strings
		return;
	}

	if (ThreadContext.SerializedObject)
	{
		FLinkerLoad* Linker = ThreadContext.SerializedObject->GetLinker();
		if (Linker)
		{
			ContainingPackageAndProperty.SetPackage(FName(*Linker->Filename));
			if (Linker->GetSerializedProperty())
			{
				ContainingPackageAndProperty.SetProperty( FName(*FString::Printf(TEXT("%s:%s"), *ThreadContext.SerializedObject->GetPathName(), *Linker->GetSerializedProperty()->GetName())));
			}
#if WITH_EDITORONLY_DATA
			ContainingPackageAndProperty.SetReferencedByEditorOnlyProperty( Linker->IsEditorOnlyPropertyOnTheStack() );
#endif
		}
	}
	StringAssetReferences.AddUnique(FName(*InString), ContainingPackageAndProperty);
}

FString FRedirectCollector::OnStringAssetReferenceSaved(const FString& InString)
{
	FString* Found = StringAssetRemap.Find(InString);
	if (Found)
	{
		return *Found;
	}
	return InString;
}


#define REDIRECT_TIMERS 1
#if REDIRECT_TIMERS
struct FRedirectScopeTimer
{
	double& Time;
	double StartTime;
	FRedirectScopeTimer(double& InTime) : Time(InTime)
	{
		StartTime = FPlatformTime::Seconds();
	}
	~FRedirectScopeTimer()
	{
		Time += FPlatformTime::Seconds() - StartTime;
	}
};

double ResolveTimeTotal;
double ResolveTimeLoad;
double ResolveTimeDelegate;

#define SCOPE_REDIRECT_TIMER(TimerName) FRedirectScopeTimer Timer##TimerName(TimerName);

#define LOG_REDIRECT_TIMER( TimerName) UE_LOG(LogRedirectors, Display, TEXT("Timer %ls %f"), TEXT(#TimerName), TimerName);

#define LOG_REDIRECT_TIMERS() \
	LOG_REDIRECT_TIMER(ResolveTimeLoad); \
	LOG_REDIRECT_TIMER(ResolveTimeDelegate);\
	LOG_REDIRECT_TIMER(ResolveTimeTotal);\

	


#else
#define SCOPE_REDIRECT_TIMER(TimerName)
#define LOG_REDIRECT_TIMERS()
#endif


void FRedirectCollector::LogTimers() const
{
	LOG_REDIRECT_TIMERS();
}

void FRedirectCollector::ResolveStringAssetReference(FString FilterPackage)
{
	SCOPE_REDIRECT_TIMER(ResolveTimeTotal);
	
	
	FName FilterPackageFName = NAME_None;
	if (!FilterPackage.IsEmpty())
	{
		FilterPackageFName = FName(*FilterPackage);
	}

	TMultiMap<FName, FPackagePropertyPair> SkippedReferences;
	SkippedReferences.Empty(StringAssetReferences.Num());
	while ( StringAssetReferences.Num())
	{

		TMultiMap<FName, FPackagePropertyPair> CurrentReferences;
		Swap(StringAssetReferences, CurrentReferences);

		for (const auto& CurrentReference : CurrentReferences)
		{
			const FName& ToLoadFName = CurrentReference.Key;
			const FPackagePropertyPair& RefFilenameAndProperty = CurrentReference.Value;

			if ((FilterPackageFName != NAME_None) && // not using a filter
				(FilterPackageFName != RefFilenameAndProperty.GetCachedPackageName()) && // this is the package we are looking for
				(RefFilenameAndProperty.GetCachedPackageName() != NAME_None) // if we have an empty package name then process it straight away
				)
			{
				// If we have a valid filter and it doesn't match, skip this reference
				SkippedReferences.Add(ToLoadFName, RefFilenameAndProperty);
				continue;
			}

			const FString ToLoad = ToLoadFName.ToString();

			if (FCoreDelegates::LoadStringAssetReferenceInCook.IsBound())
			{
				SCOPE_REDIRECT_TIMER(ResolveTimeDelegate);
				if (FCoreDelegates::LoadStringAssetReferenceInCook.Execute(ToLoad) == false)
				{
					// Skip this reference
					continue;
				}
			}
			
			if (ToLoad.Len() > 0 )
			{
				SCOPE_REDIRECT_TIMER(ResolveTimeLoad);

				UE_LOG(LogRedirectors, Verbose, TEXT("String Asset Reference '%s'"), *ToLoad);
				UE_CLOG(RefFilenameAndProperty.GetProperty().ToString().Len(), LogRedirectors, Verbose, TEXT("    Referenced by '%s'"), *RefFilenameAndProperty.GetProperty().ToString());

				StringAssetRefFilenameStack.Push(RefFilenameAndProperty.GetPackage().ToString());

				UObject *Loaded = LoadObject<UObject>(NULL, *ToLoad, NULL, RefFilenameAndProperty.GetReferencedByEditorOnlyProperty() ? LOAD_EditorOnly : LOAD_None, NULL);
				StringAssetRefFilenameStack.Pop();

				UObjectRedirector* Redirector = dynamic_cast<UObjectRedirector*>(Loaded);
				if (Redirector)
				{
					UE_LOG(LogRedirectors, Verbose, TEXT("    Found redir '%s'"), *Redirector->GetFullName());
					FRedirection Redir;
					Redir.PackageFilename = RefFilenameAndProperty.GetPackage().ToString();
					Redir.RedirectorName = Redirector->GetFullName();
					Redir.RedirectorPackageFilename = Redirector->GetLinker()->Filename;
					CA_SUPPRESS(28182)
						Redir.DestinationObjectName = Redirector->DestinationObject->GetFullName();
					Redirections.AddUnique(Redir);
					Loaded = Redirector->DestinationObject;
				}
				if (Loaded)
				{
					if (FCoreUObjectDelegates::PackageLoadedFromStringAssetReference.IsBound())
					{
						FCoreUObjectDelegates::PackageLoadedFromStringAssetReference.Broadcast(Loaded->GetOutermost()->GetFName());
					}
					FString Dest = Loaded->GetPathName();
					UE_LOG(LogRedirectors, Verbose, TEXT("    Resolved to '%s'"), *Dest);
					if (Dest != ToLoad)
					{
						StringAssetRemap.Add(ToLoad, Dest);
					}
				}
				else
				{
					const FString Referencer = RefFilenameAndProperty.GetProperty().ToString().Len() ? RefFilenameAndProperty.GetProperty().ToString() : TEXT("Unknown");
					UE_LOG(LogRedirectors, Warning, TEXT("String Asset Reference '%s' was not found! (Referencer '%s')"), *ToLoad, *Referencer);
				}
			}

		}
	}

	check(StringAssetReferences.Num() == 0);
	// Add any skipped references back into the map for the next time this is called
	Swap(StringAssetReferences, SkippedReferences);
	// we shouldn't have any references left if we decided to resolve them all
	check((StringAssetReferences.Num() == 0) || (FilterPackageFName != NAME_None));
}



FRedirectCollector GRedirectCollector;


