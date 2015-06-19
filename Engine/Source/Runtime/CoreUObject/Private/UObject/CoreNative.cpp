// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "ModuleManager.h"
#include "StringAssetReference.h"
#include "StringClassReference.h"
#include "Linker.h"

void UClassRegisterAllCompiledInClasses();
bool IsInAsyncLoadingThreadCoreUObjectInternal();
bool IsAsyncLoadingCoreUObjectInternal();
void SuspendAsyncLoadingInternal();
void ResumeAsyncLoadingInternal();

// CoreUObject module. Handles UObject system pre-init (registers init function with Core callbacks).
class FCoreUObjectModule : public FDefaultModuleImpl
{
public:
	virtual void StartupModule() override
	{
		// Register all classes that have been loaded so far. This is required for CVars to work.		
		UClassRegisterAllCompiledInClasses();

		void InitUObject();
		FCoreDelegates::OnInit.AddStatic(InitUObject);

		// Substitute Core version of async loading functions with CoreUObject ones.
		IsInAsyncLoadingThread = &IsInAsyncLoadingThreadCoreUObjectInternal;
		IsAsyncLoading = &IsAsyncLoadingCoreUObjectInternal;
		SuspendAsyncLoading = &SuspendAsyncLoadingInternal;
		ResumeAsyncLoading = &ResumeAsyncLoadingInternal;

		// Make sure that additional content mount points can be registered after CoreUObject loads
		FPackageName::EnsureContentPathsAreRegistered();		
	}
};
IMPLEMENT_MODULE( FCoreUObjectModule, CoreUObject );

// if we are not using compiled in natives, we still need this as a base class for intrinsics
#if !USE_COMPILED_IN_NATIVES
IMPLEMENT_CLASS(UObject, 0);
COREUOBJECT_API class UClass* Z_Construct_UClass_UObject();
UClass* Z_Construct_UClass_UObject()
{
	static UClass* OuterClass = NULL;
	if (!OuterClass)
	{
		OuterClass = UObject::StaticClass();
		UObjectForceRegistration(OuterClass);
		UObjectBase::EmitBaseReferences(OuterClass);
		OuterClass->StaticLink();
	}
	check(OuterClass->GetClass());
	return OuterClass;
}
#endif

/*-----------------------------------------------------------------------------
	FObjectInstancingGraph.
-----------------------------------------------------------------------------*/

FObjectInstancingGraph::FObjectInstancingGraph(bool bDisableInstancing)
	: SourceRoot(NULL)
	, DestinationRoot(NULL)
	, bCreatingArchetype(false)
	, bEnableSubobjectInstancing(!bDisableInstancing)
	, bLoadingObject(false)
{
}

FObjectInstancingGraph::FObjectInstancingGraph( UObject* DestinationSubobjectRoot )
	: SourceRoot(NULL)
	, DestinationRoot(NULL)
	, bCreatingArchetype(false)
	, bEnableSubobjectInstancing(true)
	, bLoadingObject(false)
{
	SetDestinationRoot(DestinationSubobjectRoot);
}

void FObjectInstancingGraph::SetDestinationRoot(UObject* DestinationSubobjectRoot, UObject* InSourceRoot /*= nullptr*/)
{
	DestinationRoot = DestinationSubobjectRoot;
	check(DestinationRoot);

	SourceRoot = InSourceRoot ? InSourceRoot : DestinationRoot->GetArchetype();
	check(SourceRoot);

	// add the subobject roots to the Source -> Destination mapping
	SourceToDestinationMap.Add(SourceRoot, DestinationRoot);

	bCreatingArchetype = DestinationSubobjectRoot->HasAnyFlags(RF_ArchetypeObject);
}

UObject* FObjectInstancingGraph::GetDestinationObject(UObject* SourceObject)
{
	check(SourceObject);
	return SourceToDestinationMap.FindRef(SourceObject);
}

UObject* FObjectInstancingGraph::GetInstancedSubobject( UObject* SourceSubobject, UObject* CurrentValue, UObject* CurrentObject, bool bDoNotCreateNewInstance, bool bAllowSelfReference )
{
	checkSlow(SourceSubobject);

	UObject* InstancedSubobject = INVALID_OBJECT;

	if ( SourceSubobject != NULL && CurrentValue != NULL )
	{
		bool bAllowedSelfReference = bAllowSelfReference && SourceSubobject == SourceRoot;

		bool bShouldInstance = bAllowedSelfReference || SourceSubobject->IsIn(SourceRoot);
		if ( !bShouldInstance && CurrentValue->GetOuter() == CurrentObject->GetArchetype() )
		{
			// this code is intended to catch cases where SourceRoot contains subobjects assigned to instanced object properties, where the subobject's class
			// contains components, and the class of the subobject is outside of the inheritance hierarchy of the SourceRoot, for example, a weapon
			// class which contains UIObject subobject definitions in its defaultproperties, where the property referencing the UIObjects is marked instanced.
			bShouldInstance = true;

			// if this case is triggered, ensure that the CurrentValue of the component property is still pointing to the template component.
			check(SourceSubobject == CurrentValue);
		}

		if ( bShouldInstance )
		{
			// search for the unique component instance that corresponds to this component template
			InstancedSubobject = GetDestinationObject(SourceSubobject);
			if ( InstancedSubobject == NULL )
			{
				if (bDoNotCreateNewInstance)
				{
					InstancedSubobject = INVALID_OBJECT; // leave it unchanged
				}
				else
				{
					// if the Outer for the component currently assigned to this property is the same as the object that we're instancing components for,
					// the component does not need to be instanced; otherwise, there are two possiblities:
					// 1. CurrentValue is a template and needs to be instanced
					// 2. CurrentValue is an instanced component, in which case it should already be in InstanceGraph, UNLESS the component was created
					//		at runtime (editinline export properties, for example).  If that is the case, CurrentValue will be an instance that is not linked
					//		to the component template referenced by CurrentObject's archetype, and in this case, we also don't want to re-instance the component template

					bool bIsRuntimeInstance = CurrentValue != SourceSubobject && CurrentValue->GetOuter() == CurrentObject;
					if ( bDoNotCreateNewInstance || bIsRuntimeInstance )
					{
						InstancedSubobject = CurrentValue; 
					}
					else
					{
						// If the component template is relevant in this context(client vs server vs editor), instance it.
						const bool bShouldLoadForClient = SourceSubobject->NeedsLoadForClient();
						const bool bShouldLoadForServer = SourceSubobject->NeedsLoadForServer();
						const bool bShouldLoadForEditor = ( GIsEditor && ( bShouldLoadForClient || !CurrentObject->RootPackageHasAnyFlags(PKG_PlayInEditor) ) );

						if ( ((GIsClient && bShouldLoadForClient) || (GIsServer && bShouldLoadForServer) || bShouldLoadForEditor) )
						{
							// this is the first time the instance corresponding to SourceSubobject has been requested

							// get the object instance corresponding to the source component's Outer - this is the object that
							// will be used as the Outer for the destination component
							UObject* SubobjectOuter = GetDestinationObject(SourceSubobject->GetOuter());

							checkf(SubobjectOuter, TEXT("No corresponding destination object found for '%s' while attempting to instance component '%s'"), *SourceSubobject->GetOuter()->GetFullName(), *SourceSubobject->GetFullName());

							FName SubobjectName = SourceSubobject->GetFName();

							// final archetype archetype will be the archetype of the template
							UObject* FinalSubobjectArchetype = CurrentValue->GetArchetype();

							// Don't seach for the existing subobjects on Blueprint-generated classes. What we'll find is a subobject
							// created by the constructor which may not have all of its fields initialized to the correct value (which
							// should be coming from a blueprint).
							// NOTE: Since this function is called ONLY for Blueprint-generated classes, we may as well delete this 'if'.
							if (!SubobjectOuter->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
							{
								InstancedSubobject = StaticFindObjectFast(NULL, SubobjectOuter, SubobjectName);
							}

							if (InstancedSubobject && IsCreatingArchetype())
							{
								// since we are updating an archetype, this needs to reconstruct as that is the mechanism used to copy properties
								// it will destroy the existing object and overwrite it
								InstancedSubobject = NULL;
							}

							if (!InstancedSubobject)
							{
								// finally, create the component instance
								InstancedSubobject = StaticConstructObject_Internal(SourceSubobject->GetClass(), SubobjectOuter,
									SubobjectName, SubobjectOuter->GetMaskedFlags(RF_PropagateToSubObjects), SourceSubobject,
									true, this);
							}
						}
					}
				}
			}
			else if ( IsLoadingObject() && InstancedSubobject->GetClass()->HasAnyClassFlags(CLASS_HasInstancedReference) )
			{
				/* When loading an object from disk, in some cases we have a component which has a reference to another component in DestinationObject which
					wasn't serialized and hasn't yet been instanced.  For example, the PointLight class declared two component templates:

						Begin DrawLightRadiusComponent0
						End
						Components.Add(DrawLightRadiusComponent0)

						Begin MyPointLightComponent
							SomeProperty=DrawLightRadiusComponent
						End
						LightComponent=MyPointLightComponent

					The components array will be processed by UClass::InstanceSubobjectTemplates after the LightComponent property is processed.  If the instance
					of DrawLightRadiusComponent0 that was created during the last session (i.e. when this object was saved) was identical to the component template
					from the PointLight class's defaultproperties, and the instance of MyPointLightComponent was serialized, then the MyPointLightComponent instance will
					exist in the InstanceGraph, but the instance of DrawLightRadiusComponent0 will not.  To handle this case and make sure that the SomeProperty variable of
					the MyPointLightComponent instance is correctly set to the value of the DrawLightRadiusComponent0 instance that will be created as a result of calling
					InstanceSubobjectTemplates on the PointLight actor from ConditionalPostLoad, we must call ConditionalPostLoad on each existing component instance that we
					encounter, while we still have access to all of the component instances owned by the PointLight.
				*/
				InstancedSubobject->ConditionalPostLoadSubobjects(this);
			}
		}
	}

	return InstancedSubobject;
}


UObject* FObjectInstancingGraph::InstancePropertyValue( class UObject* ComponentTemplate, class UObject* CurrentValue, class UObject* Owner, bool bIsTransient, bool bCausesInstancing, bool bAllowSelfReference )
{
	UObject* NewValue = CurrentValue;

	check(CurrentValue);

	if (CurrentValue->GetClass()->HasAnyClassFlags(CLASS_DefaultToInstanced))
	{
		bCausesInstancing = true; // these are always instanced no matter what
	}

	if (!IsSubobjectInstancingEnabled() || // if instancing is off
		(!bCausesInstancing && // or if this class isn't forced to be instanced and this var did not have the instance keyword
		!bAllowSelfReference)) // and this isn't a delegate
	{
		return NewValue; // not instancing
	}

	// if the object we're instancing the components for (Owner) has the current component's outer in its archetype chain, and its archetype has a NULL value
	// for this component property it means that the archetype didn't instance its component, so we shouldn't either.

	if (ComponentTemplate == NULL && CurrentValue != NULL && (Owner && Owner->IsBasedOnArchetype(CurrentValue->GetOuter())))
	{
		NewValue = NULL;
	}
	else
	{
		if ( ComponentTemplate == NULL )
		{
			// should only be here if our archetype doesn't contain this component property
			ComponentTemplate = CurrentValue;
		}

		UObject* MaybeNewValue = GetInstancedSubobject(ComponentTemplate, CurrentValue, Owner, bAllowSelfReference, bAllowSelfReference);
		if ( MaybeNewValue != INVALID_OBJECT )
		{
			NewValue = MaybeNewValue;
		}
	}
	return NewValue;
}


void FObjectInstancingGraph::AddNewObject(class UObject* ObjectInstance, UObject* InArchetype /*= nullptr*/)
{
	if (HasDestinationRoot())
	{
		AddNewInstance(ObjectInstance, InArchetype);
	}
	else
	{
		SetDestinationRoot(ObjectInstance, InArchetype);
	}
}

void FObjectInstancingGraph::AddNewInstance(UObject* ObjectInstance, UObject* InArchetype /*= nullptr*/)
{
	check(SourceRoot);
	check(DestinationRoot);

	if (ObjectInstance != nullptr)
	{
		UObject* SourceObject = InArchetype ? InArchetype : ObjectInstance->GetArchetype();
		check(SourceObject);

		SourceToDestinationMap.Add(SourceObject, ObjectInstance);
	}
}

void FObjectInstancingGraph::RetrieveObjectInstances( UObject* SearchOuter, TArray<UObject*>& out_Objects )
{
	if ( HasDestinationRoot() && SearchOuter != NULL && (SearchOuter == DestinationRoot || SearchOuter->IsIn(DestinationRoot)) )
	{
		for ( TMap<UObject*,UObject*>::TIterator It(SourceToDestinationMap); It; ++It )
		{
			UObject* InstancedObject = It.Value();
			if ( InstancedObject->GetOuter() == SearchOuter )
			{
				out_Objects.AddUnique(InstancedObject);
			}
		}
	}
}
