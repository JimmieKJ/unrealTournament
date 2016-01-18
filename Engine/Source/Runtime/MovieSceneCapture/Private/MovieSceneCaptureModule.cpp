// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCapturePCH.h"
#include "MovieSceneCapture.h"
#include "MovieSceneCaptureModule.h"
#include "JsonObjectConverter.h"
#include "ActiveMovieSceneCaptures.h"

#define LOCTEXT_NAMESPACE "MovieSceneCapture"

class FMovieSceneCaptureModule : public IMovieSceneCaptureModule
{
private:

	/** Handle to a movie capture implementation created from the command line, to be initialized once a world is loaded */
	FMovieSceneCaptureHandle StartupMovieCaptureHandle;

	/** End Tickable interface */
	virtual void StartupModule() override
	{
		FCoreDelegates::OnPreExit.AddRaw(this, &FMovieSceneCaptureModule::PreExit);
		FCoreUObjectDelegates::PostLoadMap.AddRaw(this, &FMovieSceneCaptureModule::OnPostLoadMap );
	}

	void PreExit()
	{
		FActiveMovieSceneCaptures::Get().Shutdown();
	}

	void OnPostLoadMap()
	{
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		if (GameEngine && StartupMovieCaptureHandle.IsValid())
		{
			IMovieSceneCaptureInterface* StartupCaptureInterface = RetrieveMovieSceneInterface(StartupMovieCaptureHandle);
			StartupCaptureInterface->Initialize(GameEngine->SceneViewport.ToSharedRef());
			StartupCaptureInterface->SetupFrameRange();
		}

		StartupMovieCaptureHandle = FMovieSceneCaptureHandle();
		FCoreUObjectDelegates::PostLoadMap.RemoveAll(this);
	}

	virtual void PreUnloadCallback() override
	{
		FCoreDelegates::OnPreExit.RemoveAll(this);
		PreExit();
	}

	virtual IMovieSceneCaptureInterface* InitializeFromCommandLine() override
	{
		if (GIsEditor)
		{
			return nullptr;
		}

		FString TypeName;
		if( FParse::Value( FCommandLine::Get(), TEXT( "-MovieSceneCaptureType=" ), TypeName ) )
		{
			// OK, they specified the type of capture they want on the command-line.  Manifests are now optional!
		}

		FString ManifestPath;
		if (!FParse::Value(FCommandLine::Get(), TEXT("-MovieSceneCaptureManifest="), ManifestPath) || ManifestPath.IsEmpty())
		{
			// Allow capturing without a manifest.  Command-line parameters for individual options will be necessary.
			if( TypeName.IsEmpty() )
			{
				return nullptr;
			}
		}

		UMovieSceneCapture* Capture = nullptr;
		if( !ManifestPath.IsEmpty() )
		{
			FString Json;
			if (FFileHelper::LoadFileToString(Json, *ManifestPath))
			{
				TSharedPtr<FJsonObject> RootObject;
				TSharedRef<TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(Json);
				if (FJsonSerializer::Deserialize(JsonReader, RootObject) && RootObject.IsValid())
				{
					auto TypeField = RootObject->TryGetField(TEXT("Type"));
					if (!TypeField.IsValid())
					{
						return nullptr;
					}

					TypeName = TypeField->AsString();
					UClass* Class = FindObject<UClass>(nullptr, *TypeName);
					if (!Class)
					{
						return nullptr;
					}

					Capture = NewObject<UMovieSceneCapture>(GetTransientPackage(), Class);
					if (!Capture)
					{
						return nullptr;
					}

					auto DataField = RootObject->TryGetField(TEXT("Data"));
					if (!DataField.IsValid())
					{
						return nullptr;
					}

					if (!FJsonObjectConverter::JsonAttributesToUStruct(DataField->AsObject()->Values, Class, Capture, 0, 0))
					{
						return nullptr;
					}
				}
			}
		}
		else if( !TypeName.IsEmpty() )
		{
			UClass* Class = FindObject<UClass>( nullptr, *TypeName );
			if( !Class )
			{
				return nullptr;
			}

			Capture = NewObject<UMovieSceneCapture>( GetTransientPackage(), Class );
			if( !Capture )
			{
				return nullptr;
			}
		}

		check( Capture != nullptr );
		FActiveMovieSceneCaptures::Get().Add( Capture );
		StartupMovieCaptureHandle = Capture->GetHandle();
		return Capture;
	}

	virtual IMovieSceneCaptureInterface* CreateMovieSceneCapture(TWeakPtr<FSceneViewport> InSceneViewport) override
	{
		UMovieSceneCapture* Capture = NewObject<UMovieSceneCapture>(GetTransientPackage());
		FActiveMovieSceneCaptures::Get().Add(Capture);
		Capture->Initialize(InSceneViewport);
		Capture->SetupFrameRange();
		Capture->StartCapture();
		return Capture;
	}

	virtual IMovieSceneCaptureInterface* RetrieveMovieSceneInterface(FMovieSceneCaptureHandle Handle)
	{
		for (auto* Existing : FActiveMovieSceneCaptures::Get().GetActiveCaptures())
		{
			if (Existing->GetHandle() == Handle)
			{
				return Existing;
			}
		}
		return nullptr;
	}

	IMovieSceneCaptureInterface* GetFirstActiveMovieSceneCapture()
	{
		for (auto* Existing : FActiveMovieSceneCaptures::Get().GetActiveCaptures())
		{
			return Existing;
		}

		return nullptr;
	}

	virtual void DestroyMovieSceneCapture(FMovieSceneCaptureHandle Handle)
	{
		for (auto* Existing : FActiveMovieSceneCaptures::Get().GetActiveCaptures())
		{
			if (Existing->GetHandle() == Handle)
			{
				Existing->Close();
				break;
			}
		}
	}
};

IMPLEMENT_MODULE( FMovieSceneCaptureModule, MovieSceneCapture )

#undef LOCTEXT_NAMESPACE