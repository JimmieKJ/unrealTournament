#include "CorePrivatePCH.h"

#include "DDCStatsHelper.h"
#include "CookingStatsModule.h"
#include "ModuleManager.h"

namespace DDCStats
{

	ICookingStats* GetCookingStats()
	{
		static ICookingStats* CookingStats = nullptr;
		static bool bInitialized = false;
		if (bInitialized == false)
		{
			// FIXME: DanielL, this module needs to be loaded somewhere on the main thread first.
			FCookingStatsModule* CookingStatsModule = nullptr; //FModuleManager::LoadModulePtr<FCookingStatsModule>(TEXT("CookingStats"));
			if (CookingStatsModule)
			{
				CookingStats = &CookingStatsModule->Get();
			}
			bInitialized = true;
		}
		return CookingStats;
	}

	static uint32 GetValidTLSSlot()
	{
		uint32 TLSSlot = FPlatformTLS::AllocTlsSlot();
		check(FPlatformTLS::IsValidTlsSlot(TLSSlot));
		FPlatformTLS::SetTlsValue(TLSSlot, nullptr);
		return TLSSlot;
	}
	

	struct FDDCStatsTLSStore
	{

		FDDCStatsTLSStore() : TransactionGuid(NAME_None), CurrentIndex(0), RootScope(NULL) { }


		FName GenerateNewTransactionName()
		{
			++CurrentIndex;
			if (CurrentIndex <= 1)
			{
				FString TransactionGuidString = TEXT("DDCTransactionId");
				TransactionGuidString += FGuid::NewGuid().ToString();
				TransactionGuid = FName(*TransactionGuidString);
				CurrentIndex = 1;
			}
			check(TransactionGuid != NAME_None);

			return FName(TransactionGuid, CurrentIndex);
		}

		FName TransactionGuid;
		int32 CurrentIndex;
		FDDCScopeStatHelper *RootScope;
	};

	uint32 CookStatsFDDCStatsTLSStore = GetValidTLSSlot();

}; // namespace DDCStats


FDDCScopeStatHelper::FDDCScopeStatHelper(const TCHAR* CacheKey, const FName& FunctionName) : bHasParent(false)
{

	if (DDCStats::GetCookingStats() == nullptr)
		return;

	DDCStats::FDDCStatsTLSStore* TLSStore = (DDCStats::FDDCStatsTLSStore*)FPlatformTLS::GetTlsValue(DDCStats::CookStatsFDDCStatsTLSStore);
	if (TLSStore == nullptr)
	{
		TLSStore = new DDCStats::FDDCStatsTLSStore();
		FPlatformTLS::SetTlsValue(DDCStats::CookStatsFDDCStatsTLSStore, TLSStore);
	}

	TransactionGuid = TLSStore->GenerateNewTransactionName();


	FString Temp;
	AddTag(FunctionName, Temp);

	if (TLSStore->RootScope == nullptr)
	{
		TLSStore->RootScope = this;
		bHasParent = false;

		static const FName NAME_CacheKey = FName(TEXT("CacheKey"));
		AddTag(NAME_CacheKey, FString(CacheKey));
	}
	else
	{
		static const FName NAME_Parent = FName(TEXT("Parent"));
		AddTag(NAME_Parent, TLSStore->RootScope->TransactionGuid.ToString());

		bHasParent = true;
	}


	static const FName NAME_StartTime(TEXT("StartTime"));
	AddTag(NAME_StartTime, FDateTime::Now().ToString());

	StartTime = FPlatformTime::Seconds();

	// if there is a root scope then we want to put out stuff under a child key of the parent and link to the parent scope
}


FDDCScopeStatHelper::~FDDCScopeStatHelper()
{
	if (DDCStats::GetCookingStats() == nullptr)
		return;

	double EndTime = FPlatformTime::Seconds();
	float DurationMS = (EndTime - StartTime) * 1000.0f;

	static const FName NAME_Duration = FName(TEXT("Duration"));
	AddTag(NAME_Duration, FString::Printf(TEXT("%fms"), DurationMS));

	DDCStats::FDDCStatsTLSStore* TLSStore = (DDCStats::FDDCStatsTLSStore*)FPlatformTLS::GetTlsValue(DDCStats::CookStatsFDDCStatsTLSStore);
	check(TLSStore);

	if (TLSStore->RootScope == this)
	{
		TLSStore->RootScope = nullptr;
	}
}

void FDDCScopeStatHelper::AddTag(const FName& Tag, const FString& Value)
{
	if (DDCStats::GetCookingStats())
	{
		DDCStats::GetCookingStats()->AddTagValue(TransactionGuid, Tag, Value);
	}
}