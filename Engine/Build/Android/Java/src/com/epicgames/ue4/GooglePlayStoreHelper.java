// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
//This file needs to be here so the "ant" build step doesnt fail when looking for a /src folder.

package com.epicgames.ue4;

import java.util.ArrayList;

import android.os.RemoteException;
import org.json.JSONException;
import android.os.Handler;
import android.os.Looper;
import android.content.Intent;
import android.content.IntentSender.SendIntentException;

import com.android.vending.billing.util.IabHelper;
import com.android.vending.billing.util.IabResult;
import com.android.vending.billing.util.Inventory;
import com.android.vending.billing.util.Purchase;
import com.android.vending.billing.util.Base64;

public class GooglePlayStoreHelper extends StoreHelper
{
	private IabHelper inAppPurchaseHelper;
	
	private GameActivity gameActivity;

	private Logger Log;

	private String ProductKey;

	
	
	/*********************************************************************
	 ** IAP SETUP WORKFLOW...
	 *********************************************************************/
	private boolean bIsIapSetup = false;

	
	// Constructor...
	public GooglePlayStoreHelper(String InProductKey, GameActivity InGameActivity, final Logger InLog)
	{
		ProductKey = InProductKey;
		gameActivity = InGameActivity;
		Log = InLog;

		// Create an Iab Helper.
		SetupIapService();
	}
	

	/**
	 *	Initialize the Iab helper functionality for our application.
	 */
	public void SetupIapService()
	{
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::SetupIapService");
		inAppPurchaseHelper = new IabHelper(gameActivity, ProductKey);

		inAppPurchaseHelper.startSetup(new IabHelper.OnIabSetupFinishedListener() 
		{
			public void onIabSetupFinished(IabResult result)
			{
				Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onIabSetupFinished");

				bIsIapSetup = result.isSuccess();

				if (bIsIapSetup)
				{
					Log.debug("[GooglePlayStoreHelper] - In-app purchasing helper has been setup");
				}
				else
				{
					Log.debug("[GooglePlayStoreHelper] - Problem setting up In-app purchasing helper: " + result);
				}
			}
		});
	}

	
	/*********************************************************************
	 ** QUERY PURCHASES WORKFLOW...
	 *********************************************************************/
	private String[] cachedQueryProductIds;
	private boolean[] cachedQueryConsumableFlags;
	
	
	// Iab helper functions for the store interface and query functionality
	IabHelper.QueryInventoryFinishedListener GotInventoryListener = new IabHelper.QueryInventoryFinishedListener()
	{
		public void onQueryInventoryFinished(IabResult result, Inventory inventory)
		{
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onQueryInventoryFinished - " + result);

			if (result.isSuccess())
			{			
				ArrayList<String> titles = new ArrayList<String>();
				ArrayList<String> descriptions = new ArrayList<String>();
				ArrayList<String> prices = new ArrayList<String>();
				for( int Idx = 0; Idx < cachedQueryProductIds.length; Idx++ )
				{	
					String productId = cachedQueryProductIds[Idx];
					Log.debug("[GooglePlayStoreHelper] - Checking details for " + productId);

					if( inventory.hasDetails(productId) )
					{
					Log.debug("[GooglePlayStoreHelper] - Found details for " + productId);
						titles.add( inventory.getSkuDetails(productId).getTitle() );
						descriptions.add( inventory.getSkuDetails(productId).getDescription() );
						prices.add( inventory.getSkuDetails(productId).getPrice() );
					}
					
					// If we have this item, but somehow failed to consume it on purchase.
					// make sure to consume it now. TPMB - TODO: make this part of the purchase workflow?
					if (inventory.hasPurchase(productId))
					{
						try {
							Purchase purchase = inventory.getPurchase(productId);
							inAppPurchaseHelper.consumeAsync(purchase, ConsumeProductFinishedCallback);
						} catch(Exception e) {
							Log.debug("[GooglePlayStoreHelper] - consumeAsync - Exception caught - " + e.getMessage());
						}
					}
				}

				if( titles.size() > 0 )
				{
					if( titles.size() != cachedQueryProductIds.length )
					{
						Log.debug("[GooglePlayStoreHelper] - Mismatch in the number of products returned vs those queried.");
						Log.debug("[GooglePlayStoreHelper] - Requested: "+ cachedQueryProductIds.length + ", Received information for: " + titles.size());
					}
					Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onQueryInventoryFinished - Succeeded.");
					nativeQueryComplete(true, cachedQueryProductIds, titles.toArray(new String[titles.size()]), descriptions.toArray(new String[descriptions.size()]), prices.toArray(new String[prices.size()])); 
				}
				else
				{
					Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onQueryInventoryFinished - Failed. " + result.getMessage());
					nativeQueryComplete(false, cachedQueryProductIds, null, null, null);
				}
			}
			else
			{
				Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onQueryInventoryFinished - Failed. " + result.getMessage());
				nativeQueryComplete(false, cachedQueryProductIds, null, null, null);
			}
		}
	};


	/**
	 *	Execute a query available purchases workflow for the provided product Id
	 */
	public boolean QueryInAppPurchases(final String[] ProductIds, final boolean[] bConsumableFlags)
	{
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryInAppPurchases");

		// Cache the input parameters for cross threading
		cachedQueryProductIds = ProductIds;
		cachedQueryConsumableFlags = bConsumableFlags;
		
		// We can launch a purchase workflow if we have provided product Ids and the IabHelper is set-up.
		boolean bIsAbleToSendQuery = bIsIapSetup && (cachedQueryProductIds.length > 0 );
		if( bIsAbleToSendQuery )
		{
			// Run the query pruchases command.
			Log.debug("[GooglePlayStoreHelper] - queryInventoryAsync. - start");
			try {
				inAppPurchaseHelper.queryInventoryAsync(GotInventoryListener);
			} catch(Exception e) {
				Log.debug("[GooglePlayStoreHelper] - queryInventoryAsync - Exception caught - " + e.getMessage());
			}
			Log.debug("[GooglePlayStoreHelper] - queryInventoryAsync. - end");

		}
		else
		{
			if( bIsIapSetup == false )
			{
				Log.debug("[GooglePlayStoreHelper] - Failed to launch a query inventory flow, Store Helper is not set-up.");
			}
			else if(cachedQueryProductIds.length <= 0 )
			{
				Log.debug("[GooglePlayStoreHelper] - Failed to launch a query inventory flow, Cannot query without product Ids.");
			}

			nativeQueryComplete(false, null, null, null, null);
		}

		return bIsAbleToSendQuery;
	}

	
	/*********************************************************************
	 ** PURCHASE WORKFLOW...
	 *********************************************************************/
	String cachedPurchaseProductId;
	boolean cachedPurchaseConsumableFlag;

	// Iab helper functions for the store interface
	IabHelper.OnConsumeFinishedListener ConsumeProductFinishedCallback =  new IabHelper.OnConsumeFinishedListener() 
	{
		public void onConsumeFinished(Purchase purchase, IabResult result) 
		{
			Log.debug("[GooglePlayStoreHelper] - ConsumeProductFinishedCallback::onConsumeFinished");
			if (result.isSuccess()) 
			{
				Log.debug("[GooglePlayStoreHelper] - onConsumeFinished: Success");
				String Receipt = Base64.encode(purchase.getOriginalJson().getBytes());
				nativePurchaseComplete(true, purchase.getSku(), Receipt);
			}
			else
			{
				// Something went wrong, cannot consume product...send back success, but will be unable to purchase again
				Log.debug("[GooglePlayStoreHelper] - ERROR: Could not successfully consume item. Will attempt again on next inventory update.");
				nativePurchaseComplete(false, purchase.getSku(), "");
			}
		}
	};
	

	// Query inventory callback used to find an existing purchase and consume it.
	// Only used when we have a lingering purchase on the server that wasnt consumed.
	IabHelper.QueryInventoryFinishedListener ConsumeInventoryListener = new IabHelper.QueryInventoryFinishedListener()
	{
		public void onQueryInventoryFinished(IabResult result, Inventory inventory)
		{
			Log.debug("[GooglePlayStoreHelper] - ConsumeInventoryListener::onQueryInventoryFinished");
			if (result.isSuccess() && inventory.hasPurchase(cachedPurchaseProductId))
			{
				final Purchase UnconsumedPurchase = inventory.getPurchase(cachedPurchaseProductId);
				inAppPurchaseHelper.consumeAsync(UnconsumedPurchase, ConsumeProductFinishedCallback);										
			}
			else
			{
				Log.debug("[GooglePlayStoreHelper] - Purchase Unsuccessful. Failed to find the already purchased product on the server...");
				nativePurchaseComplete(false, cachedPurchaseProductId, "" );
			}
		}
	};

	
	// Iab helper functions for the store interface and purchase functionality
	IabHelper.OnIabPurchaseFinishedListener PurchaseFinishedCallback = new IabHelper.OnIabPurchaseFinishedListener() 
	{
		public void onIabPurchaseFinished(IabResult result, Purchase purchase)
		{
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onIabPurchaseFinished - " + result + ", purchase: " + purchase);
			if(result.isSuccess())
			{
				if( VerifyPayload(purchase.getDeveloperPayload(), purchase.getSku() ) == true )
				{
					// Consumable items must be consumed on the server prior to provisioning the item to the game
					if(cachedPurchaseConsumableFlag == true)
					{
						Log.debug("[GooglePlayStoreHelper] - Purchase Success. Requesting Item be consumed.");
						inAppPurchaseHelper.consumeAsync(purchase, ConsumeProductFinishedCallback);
					}
					// Non consumable can immediately notify the game of the purchase
					else
					{
						Log.debug("[GooglePlayStoreHelper] - Purchase Success.");
						String Receipt = Base64.encode(purchase.getOriginalJson().getBytes());
						nativePurchaseComplete(false, cachedPurchaseProductId, Receipt);
					}
				}
				else
				{
					Log.debug("[GooglePlayStoreHelper] - Purchase Unsuccessful. Validation of developer payload failed.");
					nativePurchaseComplete(false, cachedPurchaseProductId, "" );
				}
			}
			else if( result.getResponse() == IabHelper.BILLING_RESPONSE_RESULT_ITEM_ALREADY_OWNED)
			{
				Log.debug("[GooglePlayStoreHelper] - Item was already owned but not consumed, attempting to consume... ");
				
				// Post the consume command so it is spawned from the main looper, needed for an async operation
				Handler mainHandler = new Handler(Looper.getMainLooper());
				mainHandler.post(new Runnable()
				{
					@Override
					public void run()
					{
						try {
								inAppPurchaseHelper.queryInventoryAsync(ConsumeInventoryListener);										
							}
						catch(Exception e) {
							Log.debug("[GooglePlayStoreHelper] - queryInventoryAsync - Exception caught - " + e.getMessage());
						}
					}
				});
			}
			else
			{
				Log.debug("[GooglePlayStoreHelper] - Purchase Unsuccessful. " + result.getMessage());
				nativePurchaseComplete(false, cachedPurchaseProductId, "" );
			} 
		}
	};

	private String GenerateDevPayload( String ProductId )
	{
		return "ue4."+ProductId;
	}

	private boolean VerifyPayload( String ExistingPayload, String ProductId )
	{
		String GeneratedPayload = GenerateDevPayload(ProductId);
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::VerifyPayload, ExistingPayload: " + ExistingPayload);
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::VerifyPayload, GeneratedPayload: " + GeneratedPayload);

		return ExistingPayload.equals(GeneratedPayload);
	}

	/**
	 *	Execute a purchase workflow for the provided product Id
	 */
	public boolean BeginPurchase(final String ProductId, final boolean bConsumableFlag)
	{
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::BeginPurchase");
	
		// Cache the input parameters for cross threading
		cachedPurchaseProductId = ProductId;
		cachedPurchaseConsumableFlag = bConsumableFlag;

		// We can launch a purchase workflow if we have a provided product Id and the IabHelper is set-up.
		boolean bIsValidProductId = (cachedPurchaseProductId.length() > 0);
		if( bIsIapSetup && bIsValidProductId )
		{	
			try {
				String DevPayload = GenerateDevPayload(ProductId);
				inAppPurchaseHelper.launchPurchaseFlow(gameActivity, cachedPurchaseProductId, cachedPurchaseConsumableFlag ? 10001 : 10002, PurchaseFinishedCallback, DevPayload);
			} catch(Exception e) {
				Log.debug("[GooglePlayStoreHelper] - launchPurchaseFlow - Exception caught - " + e.getMessage());
			}
		}
		else
		{
			if( bIsIapSetup == false )
			{
				Log.debug("[GooglePlayStoreHelper] - Failed to launch a purchase flow, Store Helper is not set-up.");
			}
			else if( bIsValidProductId == false )
			{
				Log.debug("[GooglePlayStoreHelper] - Failed to launch a purchase flow, Cannot purchase without a valid product Id.");
			}

			nativePurchaseComplete(false, ProductId, "" );
		}
		
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::BeginPurchase - " + ((bIsIapSetup && bIsValidProductId) ? "true" : "false"));
		return bIsIapSetup && bIsValidProductId;
	}

	public boolean IsAllowedToMakePurchases()
	{
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::IsAllowedToMakePurchases");
		return bIsIapSetup;
	}

	public boolean onActivityResult(int requestCode, int resultCode, Intent data)
	{
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onActivityResult");
		return inAppPurchaseHelper != null && inAppPurchaseHelper.handleActivityResult(requestCode, resultCode, data);
	}

	public native void nativeQueryComplete(boolean bSuccess, String[] productIDs, String[] titles, String[] descriptions, String[] prices );

	public native void nativePurchaseComplete(boolean bSuccess, String ProductId, String ReceiptData);
}

