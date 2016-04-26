// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
//This file needs to be here so the "ant" build step doesnt fail when looking for a /src folder.

package com.epicgames.ue4;

import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender.SendIntentException;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import com.android.vending.billing.util.Base64;
import com.android.vending.billing.IInAppBillingService;
import com.android.vending.billing.util.Purchase;
import java.util.ArrayList;
import org.json.JSONException;
import org.json.JSONObject;


public class GooglePlayStoreHelper implements StoreHelper
{
	private class InAppPurchase
	{
		public String ProductId;
		public boolean bConsumable;

		public InAppPurchase(String InProductId, boolean InConsumable)
		{
			ProductId = InProductId;
			bConsumable = InConsumable;
		}
	}

	private ArrayList<InAppPurchase> InProgressPurchases;

	// Our IAB helper interface provided by google.
	private IInAppBillingService mService;

	// Flag that determines whether the store is ready to use.
	private boolean bIsIapSetup;

	// Output device for log messages.
	private Logger Log;

	// Cache access to the games activity.
	private GameActivity gameActivity;

	// The google play license key.
	private String productKey;
	
	public GooglePlayStoreHelper(String InProductKey, GameActivity InGameActivity, final Logger InLog)
	{
		// IAP is not ready to use until the service is instantiated.
		bIsIapSetup = false;

		Log = InLog;
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::GooglePlayStoreHelper");

		gameActivity = InGameActivity;
		productKey = InProductKey;
		
		Intent serviceIntent = new Intent("com.android.vending.billing.InAppBillingService.BIND");
		serviceIntent.setPackage("com.android.vending");
		gameActivity.bindService(serviceIntent, mServiceConn, Context.BIND_AUTO_CREATE);

		InProgressPurchases = new ArrayList<InAppPurchase>();
	}

	
	///////////////////////////////////////////////////////
	// The StoreHelper interfaces implementation for Google Play Store.

	/**
	 * Determine whether the store is ready for purchases.
	 */
	public boolean IsAllowedToMakePurchases()
	{
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::IsAllowedToMakePurchases");
		return bIsIapSetup;
	}

	/**
	 * Query product details for the provided skus
	 */
	public boolean QueryInAppPurchases(String[] ProductIDs, boolean[] bConsumable)
	{
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryInAppPurchases");
		ArrayList<String> skuList = new ArrayList<String> ();
		
		for (String productId : ProductIDs)
		{
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryInAppPurchases - Querying " + productId);
			skuList.add(productId);
		}

		Bundle querySkus = new Bundle();
		querySkus.putStringArrayList("ITEM_ID_LIST", skuList);

		try
		{
			Bundle skuDetails = mService.getSkuDetails(3, gameActivity.getPackageName(), "inapp", querySkus);

			int response = skuDetails.getInt("RESPONSE_CODE");
			if (response == 0)
			{
				ArrayList<String> titles = new ArrayList<String>();
				ArrayList<String> descriptions = new ArrayList<String>();
				ArrayList<String> prices = new ArrayList<String>();
			
				ArrayList<String> responseList = skuDetails.getStringArrayList("DETAILS_LIST");
				for (String thisResponse : responseList)
				{
					JSONObject object = new JSONObject(thisResponse);
				
					String productId = object.getString("productId");
					Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryInAppPurchases - Parsing details for: " + productId);
				
					String title = object.getString("title");
					titles.add(title);
					Log.debug("[GooglePlayStoreHelper] - title: " + title);

					String description = object.getString("description");
					descriptions.add(description);
					Log.debug("[GooglePlayStoreHelper] - description: " + description);

					String price = object.getString("price");
					prices.add(price);
					Log.debug("[GooglePlayStoreHelper] - price: " + price);
				}

				Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryInAppPurchases - Success!");
				nativeQueryComplete(true, ProductIDs, titles.toArray(new String[titles.size()]), descriptions.toArray(new String[descriptions.size()]), prices.toArray(new String[prices.size()]));
			}
			else
			{
				Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryInAppPurchases - Failed!");
				nativeQueryComplete(false, null, null, null, null);
			}
		}
		catch(Exception e)
		{
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryInAppPurchases - Failed! " + e.getMessage());
			nativeQueryComplete(false, null, null, null, null);
		}

		return true;
	}
	
	/**
	 * Start the purchase flow for a particular sku
	 */
	final int purchaseIntentIdentifier = 1001;
	public boolean BeginPurchase(String ProductID, boolean bConsumable)
	{
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::BeginPurchase");
		
		try
		{
			String devPayload = GenerateDevPayload(ProductID);
			Bundle buyIntentBundle = mService.getBuyIntent(3, gameActivity.getPackageName(), ProductID, "inapp", devPayload);
			int response = buyIntentBundle.getInt("RESPONSE_CODE");
			if (response == 0)
			{
				Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::BeginPurchase - Starting Intent to buy " + ProductID);
				PendingIntent pendingIntent = buyIntentBundle.getParcelable("BUY_INTENT");
				gameActivity.startIntentSenderForResult(pendingIntent.getIntentSender(), purchaseIntentIdentifier, new Intent(), Integer.valueOf(0), Integer.valueOf(0), Integer.valueOf(0));
				
				InProgressPurchases.add(new InAppPurchase(ProductID, bConsumable));
			}
			else
			{
				Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::BeginPurchase - Failed with error: " + response);
				nativePurchaseComplete(false, ProductID, "");
			}
		}
		catch(Exception e)
		{
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::BeginPurchase - Failed! " + e.getMessage());
			nativePurchaseComplete(false, ProductID, "");
		}

		return true;
	}

	/**
	 * Restore previous purchases the user may have made.
	 */
	public boolean RestorePurchases()
	{
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases");
		ArrayList<String> ownedSkus = new ArrayList<String>();
		ArrayList<String> purchaseDataList = new ArrayList<String>();
		ArrayList<String> signatureList = new ArrayList<String>();
		
		// On first pas the continuation token should be null. 
		// This will allow us to gather large sets of purchased items recursively
		if (GatherOwnedPurchaseData(ownedSkus, purchaseDataList, signatureList, null))
		{
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - User has previously purchased " + ownedSkus.size() + " inapp products" );

			final ArrayList<String> f_ownedSkus = ownedSkus;
			final ArrayList<String> f_purchaseDataList = purchaseDataList;
			final ArrayList<String> f_signatureList = signatureList;

			Handler mainHandler = new Handler(Looper.getMainLooper());
			mainHandler.post(new Runnable()
			{
				@Override
				public void run()
				{
					try
					{
						Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - Now consuming any purchases that may have been missed.");
						ArrayList<String> receipts = new ArrayList<String>();

						for (int Idx = 0; Idx < f_ownedSkus.size(); Idx++)
						{
							String purchaseData = f_purchaseDataList.get(Idx);
							String dataSignature = f_signatureList.get(Idx);
							Purchase purchase = new Purchase("inapp", purchaseData, dataSignature);
							
							// This is assuming that all purchases should be consumed. Consuming a purchase that is meant to be a one-time purchase makes it so the
							// user is able to buy it again. Also, it makes it so the purchase will not be able to be restored again in the future.

							// @todo : If we do want to be able to consume purchases that are restored that may have been missed, a configurable list of InAppPurchases
							// with product Ids and consumable flags will need to be loaded and checked against here so we know whether or not to consume a particular product
							// - jwaldron

							//Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - Attempting to consume " + purchase.getSku());
							//int consumeResponse = mService.consumePurchase(3, gameActivity.getPackageName(), purchase.getToken());
							//if (consumeResponse == 0)
							//{
							//	Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - consumePurchase succeeded for " + purchase.getSku());
								String receipt = Base64.encode(purchase.getOriginalJson().getBytes());
								receipts.add(receipt);
							//}
							//else
							//{
							//	Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - consumePurchase failed for " + purchase.getSku() + " with error " + consumeResponse);
							//	receipts.add("");
							//}
						}
						Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - Success!");
						nativeRestorePurchasesComplete(true, f_ownedSkus.toArray(new String[f_ownedSkus.size()]), receipts.toArray(new String[receipts.size()]));
					}
					catch(Exception e)
					{
						Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - consumePurchase failed. " + e.getMessage());
						nativeRestorePurchasesComplete(false, null, null);
					}
				}
			});
		}
		else
		{
			nativeRestorePurchasesComplete(false, null, null);
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - Failed to collect existing purchases");
			return false;
		}

		return true;
	}

	
	///////////////////////////////////////////////////////
	// IInAppBillingService service registration and unregistration

	private ServiceConnection mServiceConn = new ServiceConnection()
	{
		@Override
		public void onServiceDisconnected(ComponentName name)
		{
			Log.debug("[GooglePlayStoreHelper] - ServiceConnection::onServiceDisconnected");
			mService = null;
			bIsIapSetup = false;
		}

		@Override
		public void onServiceConnected(ComponentName name, IBinder service)
		{
			Log.debug("[GooglePlayStoreHelper] - ServiceConnection::onServiceConnected");
			mService = IInAppBillingService.Stub.asInterface(service);
			bIsIapSetup = true;
		}
	};

	
	///////////////////////////////////////////////////////
	// Game Activity/Context driven methods we need to listen for.
	
	/**
	 * On Destory we should unbind our IInAppBillingService service
	 */
	public void onDestroy()
	{
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onDestroy");

		if (mService != null)
		{
			gameActivity.unbindService(mServiceConn);
		}
	}
	
	/**
	 * Route taken by the Purchase workflow. We listen for our purchaseIntentIdentifier request code and
	 * handle the response accordingly
	 */
	public boolean onActivityResult(int requestCode, int resultCode, Intent data)
	{
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onActivityResult");

		if (requestCode == purchaseIntentIdentifier)
		{
			int responseCode = data.getIntExtra("RESPONSE_CODE", 0);
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onActivityResult - Processing purchase result. Response Code: " + responseCode);
			if (responseCode == 0)
			{
				try
				{
					final String purchaseData = data.getStringExtra("INAPP_PURCHASE_DATA");
					final String dataSignature = data.getStringExtra("INAPP_DATA_SIGNATURE");
					JSONObject jo = new JSONObject(purchaseData);
					final String sku = jo.getString("productId");
					String developerPayload = jo.getString("developerPayload");
					if (VerifyPayload(developerPayload, sku))
					{
						final String purchaseToken = jo.getString("purchaseToken");
						
						Handler mainHandler = new Handler(Looper.getMainLooper());
						mainHandler.post(new Runnable()
						{
							@Override
							public void run()
							{
								try
								{
									boolean bTryToConsume = true;
									int consumeResponse = 0;

									Log.debug("sku = " + sku + " InProgressPurchases.size() = " + InProgressPurchases.size());

									for(int Idx = 0; Idx < InProgressPurchases.size(); ++Idx)
									{
										Log.debug("InProgressPurchases.get(Idx).ProductId = " + InProgressPurchases.get(Idx).ProductId);
										if( sku.equals( InProgressPurchases.get(Idx).ProductId ) )
										{
											bTryToConsume = InProgressPurchases.get(Idx).bConsumable;
											InProgressPurchases.remove(Idx);
											Log.debug("Found InProgressPurchase for Product " + sku + " bConsumable = " + bTryToConsume);
											break;
										}
									}

									if(bTryToConsume)
									{
										mService.consumePurchase(3, gameActivity.getPackageName(), purchaseToken);
									}

									if (consumeResponse == 0)
									{
										Purchase purchase = new Purchase("inapp", purchaseData, dataSignature);
										String receipt = Base64.encode(purchase.getOriginalJson().getBytes());
										nativePurchaseComplete(true, sku, receipt);
									}
									else
									{
										Log.debug("[GooglePlayStoreHelper] - consumePurchase failed with error " + consumeResponse);
										nativePurchaseComplete(false, sku, "" );
									}
								}
								catch(Exception e)
								{
									Log.debug("[GooglePlayStoreHelper] - consumePurchase failed. " + e.getMessage());
									nativePurchaseComplete(false, sku, "" );
								}
							}
						});
					}
					else
					{
						Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onActivityResult - Failed for verify developer payload for " + sku);
						nativePurchaseComplete(false, sku, "");
					}
				}
				catch (JSONException e)
				{
					Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onActivityResult - Failed for purchase request! " + e.getMessage());
					nativePurchaseComplete(false, "", "");
				}
			}
			else
			{
				Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onActivityResult - Failed for purchase request!. " + TranslateServerResponseCode(responseCode));
				nativePurchaseComplete(false, "", "");
			}
		}

		return true;
	}

	
	///////////////////////////////////////////////////////
	// Internal helper functions that deal assist with various IAB related events
	
	/**
	 * Create a UE4 specific unique string that will be used to verify purchases are legit.
	 */
	private String GenerateDevPayload( String ProductId )
	{
		return "ue4."+ProductId;
	}
	
	/**
	 * Check the returned payload matches one for the product we are buying.
	 */
	private boolean VerifyPayload( String ExistingPayload, String ProductId )
	{
		String GeneratedPayload = GenerateDevPayload(ProductId);
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::VerifyPayload, ExistingPayload: " + ExistingPayload);
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::VerifyPayload, GeneratedPayload: " + GeneratedPayload);

		return ExistingPayload.equals(GeneratedPayload);
	}
	
	/**
	 * Get a text tranlation of the Response Codes returned by google play.
	 */
	private String TranslateServerResponseCode(final int serverResponseCode)
	{
		// taken from http://developer.android.com/google/play/billing/billing_reference.html
		switch(serverResponseCode)
		{
			case 0: // BILLING_RESPONSE_RESULT_OK
				return "Success";
			case 1: // BILLING_RESPONSE_RESULT_USER_CANCELED
				return "User pressed back or canceled a dialog";
			case 2: // BILLING_RESPONSE_RESULT_SERVICE_UNAVAILABLE
				return "Network connection is down";
			case 3: // BILLING_RESPONSE_RESULT_BILLING_UNAVAILABLE
				return "Billing API version is not supported for the type requested";
			case 4: // BILLING_RESPONSE_RESULT_ITEM_UNAVAILABLE
				return "Requested product is not available for purchase";
			case 5: // BILLING_RESPONSE_RESULT_DEVELOPER_ERROR
				return "Invalid arguments provided to the API. This error can also indicate that the application was not correctly signed or properly set up for In-app Billing in Google Play, or does not have the necessary permissions in its manifest";
			case 6: // BILLING_RESPONSE_RESULT_ERROR
				return "Fatal error during the API action";
			case 7: // BILLING_RESPONSE_RESULT_ITEM_ALREADY_OWNED
				return "Failure to purchase since item is already owned";
			case 8: // BILLING_RESPONSE_RESULT_ITEM_NOT_OWNED
				return "Failure to consume since item is not owned";
			default:
				return "Unknown Server Response Code";
		}
	}
	
	/**
	 * Recursive functionality to gather all of the purchases owned by a user.
	 * if the user owns a lot of products then we need to getPurchases again with a continuationToken
	 */
	private boolean GatherOwnedPurchaseData(ArrayList<String> inOwnedSkus, ArrayList<String> inPurchaseDataList, ArrayList<String> inSignatureList, String inContinuationToken)
	{
		try
		{
			Bundle ownedItems = mService.getPurchases(3, gameActivity.getPackageName(), "inapp", inContinuationToken);

			int responseCode = ownedItems.getInt("RESPONSE_CODE");
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - getPurchases result. Response Code: " + responseCode);
			if (responseCode == 0)
			{
				ArrayList<String> ownedSkus = ownedItems.getStringArrayList("INAPP_PURCHASE_ITEM_LIST");
				ArrayList<String> purchaseDataList = ownedItems.getStringArrayList("INAPP_PURCHASE_DATA_LIST");
				ArrayList<String> signatureList = ownedItems.getStringArrayList("INAPP_DATA_SIGNATURE_LIST");
				String continuationToken = ownedItems.getString("INAPP_CONTINUATION_TOKEN");

				for (int Idx = 0; Idx < purchaseDataList.size(); Idx++)
				{
					String sku = ownedSkus.get(Idx);
					inOwnedSkus.add(sku);

					String purchaseData = purchaseDataList.get(Idx);
					inPurchaseDataList.add(purchaseData);

					String signature = signatureList.get(Idx);
					inSignatureList.add(signature);
				}

				// if continuationToken != null, call getPurchases again
				// and pass in the token to retrieve more items
				if (continuationToken != null)
				{
					return GatherOwnedPurchaseData(inOwnedSkus, inPurchaseDataList, inSignatureList, inContinuationToken);
				}
			}
			else
			{
				Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onActivityResult - Failed for purchase request!. " + TranslateServerResponseCode(responseCode));
				return false;
			}
		}
		catch (Exception e)
		{
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onActivityResult - Failed for purchase request!. " + e.getMessage());
			return false;
		}

		return true;
	}


	// Callback that notify the C++ implementation that a task has completed
	public native void nativeQueryComplete(boolean bSuccess, String[] productIDs, String[] titles, String[] descriptions, String[] prices );
	public native void nativePurchaseComplete(boolean bSuccess, String ProductId, String ReceiptData);
	public native void nativeRestorePurchasesComplete(boolean bSuccess, String[] ProductIds, String[] ReceiptsData);

}

