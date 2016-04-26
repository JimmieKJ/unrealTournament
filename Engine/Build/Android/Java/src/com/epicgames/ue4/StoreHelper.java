//This file needs to be here so the "ant" build step doesnt fail when looking for a /src folder.

package com.epicgames.ue4;

import android.content.Intent;
import android.os.Bundle;

public interface StoreHelper
{
	public boolean QueryInAppPurchases(String[] ProductIDs, boolean[] bConsumable);
	public boolean BeginPurchase(String ProductID, boolean bConsumable);
	public boolean IsAllowedToMakePurchases();
	public boolean RestorePurchases();
	public void	onDestroy();
	public boolean onActivityResult(int requestCode, int resultCode, Intent data);
}