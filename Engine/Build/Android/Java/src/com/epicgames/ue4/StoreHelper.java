//This file needs to be here so the "ant" build step doesnt fail when looking for a /src folder.

package com.epicgames.ue4;

import android.content.Intent;

public abstract class StoreHelper
{
	public StoreHelper()
	{
	}
	
	public abstract void SetupIapService();
	public abstract boolean QueryInAppPurchases(String[] ProductIDs, boolean[] bConsumable);
	public abstract boolean BeginPurchase(String ProductID, boolean bConsumable);
	public abstract boolean IsAllowedToMakePurchases();
	public abstract boolean onActivityResult(int requestCode, int resultCode, Intent data);
}