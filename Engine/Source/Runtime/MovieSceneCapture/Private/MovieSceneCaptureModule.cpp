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
		FWorldDelegates::OnPostWorldInitialization.AddRaw(this, &FMovieSceneCaptureModule::OnPostWorldInitialization);
	}

	void PreExit()
	{
		FActiveMovieSceneCaptures::Get().Shutdown();
	}

	void OnPostWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS)
	{
		if (!World)
		{
			return;
		}

		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		if (GameEngine && StartupMovieCaptureHandle.IsValid())
		{
			IMovieSceneCaptureInterface* StartupCaptureInterface = RetrieveMovieSceneInterface(StartupMovieCaptureHandle);
			StartupCaptureInterface->Initialize(GameEngine->SceneViewport.ToSharedRef());
		}

		StartupMovieCaptureHandle = FMovieSceneCaptureHandle();
		FWorldDelegates::OnPostWorldInitialization.RemoveAll(this);
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

		FString ManifestPath;
		if (!FParse::Value(FCommandLine::Get(), TEXT("-MovieSceneCaptureManifest="), ManifestPath) || ManifestPath.IsEmpty())
		{
			return nullptr;
		}

		UMovieSceneCapture* Capture = nullptr;
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

				UClass* Class = FindObject<UClass>(nullptr, *TypeField->AsString());
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
				
				FActiveMovieSceneCaptures::Get().Add(Capture);
				StartupMovieCaptureHandle = Capture->GetHandle();
				return Capture;
			}
		}

		return nullptr;
	}

	virtual IMovieSceneCaptureInterface* CreateMovieSceneCapture(TWeakPtr<FSceneViewport> InSceneViewport) override
	{
		UMovieSceneCapture* Capture = NewObject<UMovieSceneCapture>(GetTransientPackage());
		FActiveMovieSceneCaptures::Get().Add(Capture);
		Capture->Initialize(InSceneViewport);
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