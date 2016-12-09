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

	private final int UndefinedFailureResponse = -1;

	public interface PurchaseLaunchCallback
	{
		void launchForResult(PendingIntent pendingIntent, int requestCode);
	}

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
	public boolean QueryInAppPurchases(String[] InProductIDs, boolean[] bConsumable)
	{
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryInAppPurchases");
		ArrayList<String> skuList = new ArrayList<String> ();
		
		for (String productId : InProductIDs)
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
				ArrayList<String> productIds = new ArrayList<String>();
				ArrayList<String> titles = new ArrayList<String>();
				ArrayList<String> descriptions = new ArrayList<String>();
				ArrayList<String> prices = new ArrayList<String>();
				ArrayList<Float> pricesRaw = new ArrayList<Float>();
				ArrayList<String> currencyCodes = new ArrayList<String>();

				ArrayList<String> responseList = skuDetails.getStringArrayList("DETAILS_LIST");
				for (String thisResponse : responseList)
				{
					JSONObject object = new JSONObject(thisResponse);
				
					String productId = object.getString("productId");
					productIds.add(productId);
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

					Float priceRaw = (float) object.getInt("price_amount_micros") / 1000000.0f;
					pricesRaw.add(priceRaw);
					Log.debug("[GooglePlayStoreHelper] - price_amount_micros: " + priceRaw.toString());

					String currencyCode = object.getString("price_currency_code");
					currencyCodes.add(currencyCode);
					Log.debug("[GooglePlayStoreHelper] - price_currency_code: " + currencyCode);
				}

				float[] pricesRawPrimitive = new float[pricesRaw.size()];
				for(int i = 0; i < pricesRaw.size(); i++)
					pricesRawPrimitive[i] = pricesRaw.get(i);

				Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryInAppPurchases - Success!");
				nativeQueryComplete(response, productIds.toArray(new String[productIds.size()]), titles.toArray(new String[titles.size()]), descriptions.toArray(new String[descriptions.size()]), prices.toArray(new String[prices.size()]), pricesRawPrimitive, currencyCodes.toArray(new String[currencyCodes.size()]));
			}
			else
			{
				Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryInAppPurchases - Failed!");
				nativeQueryComplete(response, null, null, null, null, null, null);
			}
		}
		catch(Exception e)
		{
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryInAppPurchases - Failed! " + e.getMessage());
			nativeQueryComplete(UndefinedFailureResponse, null, null, null, null, null, null);
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
			Bundle buyIntentBundle = null;
			int response = -1;

			if (gameActivity.IsInVRMode()) {
				Bundle bundle = new Bundle();
				bundle.putBoolean("vr", true);
				response =  mService.isBillingSupportedExtraParams(7, gameActivity.getPackageName(),"inapp", bundle);
				if (response == 0) {
					Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::BeginPurchase - v7 VR purchase" + ProductID);
					buyIntentBundle = mService.getBuyIntentExtraParams(7, gameActivity.getPackageName(), ProductID, "inapp", devPayload, bundle);
				} else {
					Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::BeginPurchase - v3 IAB purchase:" + ProductID);
					buyIntentBundle = mService.getBuyIntent(3, gameActivity.getPackageName(), ProductID, "inapp", devPayload);
				}
			} else {
				Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::BeginPurchase - v3 IAB purchase:" + ProductID);
				buyIntentBundle = mService.getBuyIntent(3, gameActivity.getPackageName(), ProductID, "inapp", devPayload);
			}
			response = buyIntentBundle.getInt("RESPONSE_CODE");
			if (response == 0)
			{
				Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::BeginPurchase - Starting Intent to buy " + ProductID);
				PendingIntent pendingIntent = buyIntentBundle.getParcelable("BUY_INTENT");
				PurchaseLaunchCallback callback =  gameActivity.getPurchaseLaunchCallback();
				if (callback != null) {
					callback.launchForResult(pendingIntent, purchaseIntentIdentifier);
				} else {
					gameActivity.startIntentSenderForResult(pendingIntent.getIntentSender(), purchaseIntentIdentifier, new Intent(), Integer.valueOf(0), Integer.valueOf(0), Integer.valueOf(0));
				}

				InProgressPurchases.add(new InAppPurchase(ProductID, bConsumable));
			}
			else
			{
				Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::BeginPurchase - Failed with error: " + response);
				nativePurchaseComplete(response, ProductID, "", "");
			}
		}
		catch(Exception e)
		{
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::BeginPurchase - Failed! " + e.getMessage());
			nativePurchaseComplete(UndefinedFailureResponse, ProductID, "", "");
		}

		return true;
	}

	/**
	 * Restore previous purchases the user may have made.
	 */
	public boolean RestorePurchases(String[] InProductIDs, boolean[] bConsumable)
	{
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases");
		ArrayList<String> ownedSkus = new ArrayList<String>();
		ArrayList<String> purchaseDataList = new ArrayList<String>();
		ArrayList<String> signatureList = new ArrayList<String>();
		
		// On first pas the continuation token should be null. 
		// This will allow us to gather large sets of purchased items recursively
		int responseCode = GatherOwnedPurchaseData(ownedSkus, purchaseDataList, signatureList, null);
		if (responseCode == 0)
		{
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - User has previously purchased " + ownedSkus.size() + " inapp products" );

			final ArrayList<String> f_ownedSkus = ownedSkus;
			final ArrayList<String> f_purchaseDataList = purchaseDataList;
			final ArrayList<String> f_signatureList = signatureList;
			final String[] RestoreProductIDs = InProductIDs;
			final boolean[] bRestoreConsumableFlags = bConsumable;

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
						int cachedResponse = 0;

						for (int Idx = 0; Idx < f_ownedSkus.size(); Idx++)
						{
							String purchaseData = f_purchaseDataList.get(Idx);
							String dataSignature = f_signatureList.get(Idx);
							Purchase purchase = new Purchase("inapp", purchaseData, dataSignature);
							boolean bTryToConsume = false;
							int consumeResponse = 0;
							
							// This is assuming that all purchases should be consumed. Consuming a purchase that is meant to be a one-time purchase makes it so the
							// user is able to buy it again. Also, it makes it so the purchase will not be able to be restored again in the future.

							for(int Idy = 0; Idy < RestoreProductIDs.length; Idy++)
							{
								if( purchase.getSku().equals(RestoreProductIDs[Idy]) )
								{
									if(Idy < bRestoreConsumableFlags.length)
									{
										bTryToConsume = bRestoreConsumableFlags[Idy];
										Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - Found Consumable Flag for Product " + purchase.getSku() + " bConsumable = " + bTryToConsume);
									}
									break;
								}
							}

							if (bTryToConsume)
							{
								Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - Attempting to consume " + purchase.getSku());
								consumeResponse = mService.consumePurchase(3, gameActivity.getPackageName(), purchase.getToken());
							}

							if (consumeResponse == 0)
							{
								Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - Purchase restored for " + purchase.getSku());
								String receipt = Base64.encode(purchase.getOriginalJson().getBytes());
								receipts.add(receipt);
							}
							else
							{
								Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - consumePurchase failed for " + purchase.getSku() + " with error " + consumeResponse);
								receipts.add("");
								cachedResponse = consumeResponse;
							}
						}
						Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - Success!");
						nativeRestorePurchasesComplete(cachedResponse, f_ownedSkus.toArray(new String[f_ownedSkus.size()]), receipts.toArray(new String[receipts.size()]), f_signatureList.toArray(new String[f_signatureList.size()]));
					}
					catch (Exception e)
					{
						Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - consumePurchase failed. " + e.getMessage());
						nativeRestorePurchasesComplete(UndefinedFailureResponse, null, null, null);
					}
				}
			});
		}
		else
		{
			nativeRestorePurchasesComplete(responseCode, null, null, null);
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
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onActivityResult - Processing purchase result. Response Code: " + TranslateServerResponseCode(responseCode));
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
										consumeResponse = mService.consumePurchase(3, gameActivity.getPackageName(), purchaseToken);
									}

									if (consumeResponse == 0)
									{
										Purchase purchase = new Purchase("inapp", purchaseData, dataSignature);
										String receipt = Base64.encode(purchase.getOriginalJson().getBytes());
										nativePurchaseComplete(consumeResponse, sku, receipt, purchase.getSignature());
									}
									else
									{
										Log.debug("[GooglePlayStoreHelper] - consumePurchase failed with error " + TranslateServerResponseCode(consumeResponse));
										nativePurchaseComplete(consumeResponse, sku, "", "");
									}
								}
								catch(Exception e)
								{
									Log.debug("[GooglePlayStoreHelper] - consumePurchase failed. " + e.getMessage());
									nativePurchaseComplete(UndefinedFailureResponse, sku, "", "");
								}
							}
						});
					}
					else
					{
						Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onActivityResult - Failed for verify developer payload for " + sku);
						nativePurchaseComplete(UndefinedFailureResponse, sku, "", "");
					}
				}
				catch (JSONException e)
				{
					Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onActivityResult - Failed for purchase request! " + e.getMessage());
					nativePurchaseComplete(UndefinedFailureResponse, "", "", "");
				}
			}
			else
			{
				Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onActivityResult - Failed for purchase request!. " + TranslateServerResponseCode(responseCode));
				nativePurchaseComplete(responseCode, "", "", "");
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
	private int GatherOwnedPurchaseData(ArrayList<String> inOwnedSkus, ArrayList<String> inPurchaseDataList, ArrayList<String> inSignatureList, String inContinuationToken)
	{
		int responseCode = UndefinedFailureResponse;
		try
		{
			Bundle ownedItems = mService.getPurchases(3, gameActivity.getPackageName(), "inapp", inContinuationToken);

			responseCode = ownedItems.getInt("RESPONSE_CODE");
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - getPurchases result. Response Code: " + TranslateServerResponseCode(responseCode));
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
		}
		catch (Exception e)
		{
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onActivityResult - Failed for purchase request!. " + e.getMessage());
		}

		return responseCode;
	}


	// Callback that notify the C++ implementation that a task has completed
	public native void nativeQueryComplete(int responseCode, String[] productIDs, String[] titles, String[] descriptions, String[] prices, float[] pricesRaw, String[] currencyCodes );
	public native void nativePurchaseComplete(int responseCode, String ProductId, String ReceiptData, String Signature);
	public native void nativeRestorePurchasesComplete(int responseCode, String[] ProductIds, String[] ReceiptsData, String[] Signatures);

}

