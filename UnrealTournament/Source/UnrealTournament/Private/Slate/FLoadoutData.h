#pragma once

// The Loadout Data
class FLoadoutData 
{
public:
	TWeakObjectPtr<AUTReplicatedLoadoutInfo> LoadoutInfo;
	TWeakObjectPtr<AUTInventory> DefaultObject;
	TSharedPtr<FSlateDynamicImageBrush> Image;

	FLoadoutData(TWeakObjectPtr<AUTReplicatedLoadoutInfo> inLoadoutInfo)
		: LoadoutInfo(inLoadoutInfo)
	{
		if (LoadoutInfo.IsValid())
		{
			DefaultObject = LoadoutInfo->ItemClass->GetDefaultObject<AUTInventory>();
			if (DefaultObject.IsValid() && DefaultObject->MenuGraphic)
			{
				FString ImageName = DefaultObject->GetName() + TEXT("_Img");

				Image = MakeShareable(new FSlateDynamicImageBrush(DefaultObject->MenuGraphic, FVector2D(256.0,128.0),FName(*ImageName)));
			}
		}
	};

	static TSharedRef<FLoadoutData> Make(TWeakObjectPtr<AUTReplicatedLoadoutInfo> inLoadoutInfo)
	{
		return MakeShareable( new FLoadoutData( inLoadoutInfo) );
	}
};

struct FCompareByCost 
{
	FORCEINLINE bool operator()( const TSharedPtr< FLoadoutData > A, const TSharedPtr< FLoadoutData > B ) const 
	{
/*
		if (A->LoadoutInfo->CurrentCost == B->LoadoutInfo->CurrentCost)
		{
			if (A->DefaultObject->IsA(AUTWeapon::StaticClass()) && 
	

			if (A->DefaultWeaponObject->Group == B->DefaultWeaponObject->Group)
			{
				return A->DefaultWeaponObject->GroupSlot < B->DefaultWeaponObject->GroupSlot;
			}
			else
			{
				return A->DefaultWeaponObject->Group < B->DefaultWeaponObject->Group;
			}
		}
*/
		return ( A->LoadoutInfo->CurrentCost < B->LoadoutInfo->CurrentCost );
	}
};
