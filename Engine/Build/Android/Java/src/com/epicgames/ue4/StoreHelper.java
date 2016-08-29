//This file needs to be here so the "ant" build step doesnt fail when looking for a /src folder.

package com.epicgames.ue4;

import android.content.Intent;
import android.os.Bundle;

public interface StoreHelper
{
	public class InAppPurchase
	{
		public String ProductId;
		public boolean bConsumable;

		public InAppPurchase(String InProductId, boolean InConsumable)
		{
			ProductId = InProductId;
			bConsumable = InConsumable;
		}
	}

	public boolean QueryInAppPurchases(String[] ProductIDs, boolean[] bConsumable);
	public boolean BeginPurchase(String ProductID, boolean bConsumable);
	public boolean IsAllowedToMakePurchases();
	public boolean RestorePurchases(String[] InProductIDs, boolean[] bConsumable);
	public void	onDestroy();
	public boolean onActivityResult(int requestCode, int resultCode, Intent data);
}