// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemIOSPrivatePCH.h"
#include "OnlineStoreInterface.h"
#include "OnlineSubsystemNames.h"

////////////////////////////////////////////////////////////////////
/// FStoreKitHelper implementation


@implementation FStoreKitHelper
@synthesize Request;
@synthesize AvailableProducts;


- (id)init
{
	self = [super init];
	return self;
}


- (void)paymentQueue:(SKPaymentQueue *)queue updatedTransactions:(NSArray *)transactions
{
	UE_LOG(LogOnline, Log, TEXT( "FStoreKitHelper::updatedTransactions" ));
    for (SKPaymentTransaction *transaction in transactions)
    {
        switch ([transaction transactionState])
        {
            case SKPaymentTransactionStatePurchased:
                [self completeTransaction:transaction];
                break;
            case SKPaymentTransactionStateFailed:
                [self failedTransaction:transaction];
                break;
            case SKPaymentTransactionStateRestored:
                [self restoreTransaction:transaction];
				break;
            case SKPaymentTransactionStatePurchasing:
                continue;
			default:
				break;
        }
    }
}

-(void)paymentQueueRestoreCompletedTransactionsFinished:(SKPaymentQueue *)queue
{
	UE_LOG(LogOnline, Log, TEXT("FStoreKitHelper::paymentQueueRestoreCompletedTransactionsFinished"));

	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get(IOS_SUBSYSTEM);
		FOnlineStoreInterfaceIOS* StoreInterface = (FOnlineStoreInterfaceIOS*)OnlineSub->GetStoreInterface().Get();

		if (StoreInterface->CachedPurchaseRestoreObject.IsValid())
		{
			StoreInterface->CachedPurchaseRestoreObject->ReadState = EOnlineAsyncTaskState::Done;
		}
		StoreInterface->TriggerOnInAppPurchaseRestoreCompleteDelegates(EInAppPurchaseState::Restored);
		// Ensure these flags are false to allow a subsequent transaction attempt to proceed
		StoreInterface->bIsProductRequestInFlight = false;
		StoreInterface->bIsPurchasing = false;
		StoreInterface->bIsRestoringPurchases = false;

		return true;
	}];
}

-(void)paymentQueue: (SKPaymentQueue *)queue restoreCompletedTransactionsFailedWithError : (NSError *)error
{
	UE_LOG(LogOnline, Log, TEXT("FStoreKitHelper::restoreCompletedTransactionsFailedWithError - %s"), *FString([error localizedDescription]));

	EInAppPurchaseState::Type CompletionState = EInAppPurchaseState::Unknown;
	switch (error.code)
	{
	case SKErrorPaymentCancelled:
		CompletionState = EInAppPurchaseState::Cancelled;
		break;
	case SKErrorClientInvalid:
	case SKErrorStoreProductNotAvailable:
	case SKErrorPaymentInvalid:
		CompletionState = EInAppPurchaseState::Invalid;
		break;
	case SKErrorPaymentNotAllowed:
		CompletionState = EInAppPurchaseState::NotAllowed;
		break;
	}

	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get(IOS_SUBSYSTEM);
		FOnlineStoreInterfaceIOS* StoreInterface = (FOnlineStoreInterfaceIOS*)OnlineSub->GetStoreInterface().Get();
		if (StoreInterface->CachedPurchaseRestoreObject.IsValid())
		{
			StoreInterface->CachedPurchaseRestoreObject->ReadState = EOnlineAsyncTaskState::Done;
		}

		StoreInterface->TriggerOnInAppPurchaseRestoreCompleteDelegates(CompletionState);
		// Ensure these flags are false to allow a subsequent transaction attempt to proceed
		StoreInterface->bIsProductRequestInFlight = false;
		StoreInterface->bIsPurchasing = false;
		StoreInterface->bIsRestoringPurchases = false;

		return true;
	}];
}

-(void)paymentQueue: (SKPaymentQueue *)queue requestProductDataFailedWithError : (NSError *)error
{
	UE_LOG(LogOnline, Log, TEXT("FStoreKitHelper::requestProductDataFailedWithError - %s"), *FString([error localizedDescription]));

	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get(IOS_SUBSYSTEM);
		FOnlineStoreInterfaceIOS* StoreInterface = (FOnlineStoreInterfaceIOS*)OnlineSub->GetStoreInterface().Get();
		
		StoreInterface->TriggerOnQueryForAvailablePurchasesCompleteDelegates( false );
		// Ensure these flags are false to allow a subsequent transaction attempt to proceed
		StoreInterface->bIsProductRequestInFlight = false;
		StoreInterface->bIsPurchasing = false;
		StoreInterface->bIsRestoringPurchases = false;

		return true;
	}];
}

- (void) completeTransaction: (SKPaymentTransaction *)transaction
{
	UE_LOG(LogOnline, Log, TEXT( "FStoreKitHelper::completeTransaction" ));

    
	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get(IOS_SUBSYSTEM);
		FOnlineStoreInterfaceIOS* StoreInterface = (FOnlineStoreInterfaceIOS*)OnlineSub->GetStoreInterface().Get();

		if (StoreInterface->CachedPurchaseStateObject.IsValid())
		{
			FString ReceiptData;

#ifdef __IPHONE_7_0
			if( [IOSAppDelegate GetDelegate].OSVersion >= 7.0 )
			{
				NSURL* nsReceiptUrl = [[NSBundle mainBundle] appStoreReceiptURL];
				NSData* nsReceiptData = [NSData dataWithContentsOfURL : nsReceiptUrl];
				NSString* nsEncodedReceiptData = [nsReceiptData base64EncodedStringWithOptions : NSDataBase64EncodingEndLineWithLineFeed];
     
				ReceiptData = nsEncodedReceiptData;
			}
			else
#endif
			{
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_7_0
				// If earlier than IOS 7, we will need to use the transactionReceipt
				NSString* nsEncodedReceiptData = [transaction.transactionReceipt base64EncodedStringWithOptions:NSDataBase64EncodingEndLineWithLineFeed];
				ReceiptData = nsEncodedReceiptData;
#endif
			}

			StoreInterface->CachedPurchaseStateObject->ProvidedProductInformation.ReceiptData = ReceiptData;
			StoreInterface->CachedPurchaseStateObject->ProvidedProductInformation.TransactionIdentifier = transaction.transactionIdentifier;
			StoreInterface->CachedPurchaseStateObject->ReadState = EOnlineAsyncTaskState::Done;
		}
     
		StoreInterface->TriggerOnInAppPurchaseCompleteDelegates(EInAppPurchaseState::Success);
		// Ensure these flags are false to allow a subsequent transaction attempt to proceed
 		StoreInterface->bIsProductRequestInFlight = false;
		StoreInterface->bIsPurchasing = false;
		StoreInterface->bIsRestoringPurchases = false;
	 
		return true;
	}];
 
	// Remove the transaction from the payment queue.
	[[SKPaymentQueue defaultQueue] finishTransaction: transaction];
}


- (void) restoreTransaction: (SKPaymentTransaction *)transaction
{
	UE_LOG(LogOnline, Log, TEXT( "FStoreKitHelper::restoreTransaction" ));

	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get(IOS_SUBSYSTEM);
		FOnlineStoreInterfaceIOS* StoreInterface = (FOnlineStoreInterfaceIOS*)OnlineSub->GetStoreInterface().Get();

		if (StoreInterface->CachedPurchaseRestoreObject.IsValid())
		{
			FString ReceiptData;

#ifdef __IPHONE_7_0
			if ([IOSAppDelegate GetDelegate].OSVersion >= 7.0)
			{
				NSURL* nsReceiptUrl = [[NSBundle mainBundle] appStoreReceiptURL];
				NSData* nsReceiptData = [NSData dataWithContentsOfURL : nsReceiptUrl];
				NSString* nsEncodedReceiptData = [nsReceiptData base64EncodedStringWithOptions : NSDataBase64EncodingEndLineWithLineFeed];

				ReceiptData = nsEncodedReceiptData;
			}
			else
#endif
			{
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_7_0
				// If earlier than IOS 7, we will need to use the transactionReceipt
				NSString* nsEncodedReceiptData = [transaction.originalTransaction.transactionReceipt base64EncodedStringWithOptions : NSDataBase64EncodingEndLineWithLineFeed];
				ReceiptData = nsEncodedReceiptData;
#endif
			}

			FInAppPurchaseRestoreInfo RestoreInfo;
			RestoreInfo.Identifier = FString(transaction.originalTransaction.payment.productIdentifier);
			RestoreInfo.ReceiptData = ReceiptData;
			StoreInterface->CachedPurchaseRestoreObject->ProvidedRestoreInformation.Add(RestoreInfo);
		}

		return true;
	}];
	
	[[SKPaymentQueue defaultQueue] finishTransaction:transaction];
}


- (void) failedTransaction: (SKPaymentTransaction *)transaction
{
	UE_LOG(LogOnline, Log, TEXT( "FStoreKitHelper::failedTransaction - %s" ), *FString([transaction.error localizedDescription]));

	EInAppPurchaseState::Type CompletionState = EInAppPurchaseState::Unknown;
	switch (transaction.error.code)
	{
	case SKErrorPaymentCancelled:
		CompletionState = EInAppPurchaseState::Cancelled;
		break;
    case SKErrorClientInvalid:
		CompletionState = EInAppPurchaseState::AlreadyOwned;
		break;
	case SKErrorStoreProductNotAvailable:
	case SKErrorPaymentInvalid:
		CompletionState = EInAppPurchaseState::Invalid;
		break;
	case SKErrorPaymentNotAllowed:
		CompletionState = EInAppPurchaseState::NotAllowed;
		break;
	}
	
    [FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
    {
        IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get(IOS_SUBSYSTEM);
        FOnlineStoreInterfaceIOS* StoreInterface = (FOnlineStoreInterfaceIOS*)OnlineSub->GetStoreInterface().Get();
        if (StoreInterface->CachedPurchaseStateObject.IsValid())
        {
            StoreInterface->CachedPurchaseStateObject->ReadState = EOnlineAsyncTaskState::Done;
        }

        StoreInterface->TriggerOnInAppPurchaseCompleteDelegates(CompletionState);
 		// Ensure these flags are false to allow a subsequent transaction attempt to proceed
		StoreInterface->bIsProductRequestInFlight = false;
		StoreInterface->bIsPurchasing = false;
		StoreInterface->bIsRestoringPurchases = false;
	 
        return true;
     }];

    [[SKPaymentQueue defaultQueue] finishTransaction: transaction];
}


- (void) requestProductData: (NSMutableSet*) productIDs
{
	UE_LOG(LogOnline, Verbose, TEXT( "FStoreKitHelper::requestProductData" ));

	Request = [[SKProductsRequest alloc] initWithProductIdentifiers: productIDs];
	Request.delegate = self;

	[Request start];
}


- (void) makePurchase: (NSMutableSet*) productIDs
{
	UE_LOG(LogOnline, Verbose, TEXT( "FStoreKitHelper::makePurchase" ));

	Request = [[SKProductsRequest alloc] initWithProductIdentifiers: productIDs];
	Request.delegate = self;

	[Request start];
}


- (void)productsRequest: (SKProductsRequest *)request didReceiveResponse:(SKProductsResponse *)response
{
	UE_LOG(LogOnline, Verbose, TEXT( "FStoreKitHelper::didReceiveResponse" ));
	// Direct the response back to the store interface
	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get(IOS_SUBSYSTEM);
		FOnlineStoreInterfaceIOS* StoreInterface = (FOnlineStoreInterfaceIOS*)OnlineSub->GetStoreInterface().Get();
		StoreInterface->ProcessProductsResponse(response);

		return true;
	}];

	[request autorelease];
}

-(void)requestDidFinish:(SKRequest*)request
{
#ifdef __IPHONE_7_0
	if ([Request isKindOfClass : [SKReceiptRefreshRequest class]])
	{
		[[SKPaymentQueue defaultQueue] restoreCompletedTransactions];
	}
	
	else if ([Request isKindOfClass : [SKProductsRequest class]])
	{
		// This is a placeholder to check if this case needs to be handled in the future
	}
	
#endif
}

-(void)request:(SKRequest*)request didFailWithError: (NSError *)error
{
#ifdef __IPHONE_7_0
	// Check if we were refreshing a receipt
	if ([Request isKindOfClass : [SKReceiptRefreshRequest class]])
	{
		UE_LOG(LogOnline, Warning, TEXT( "FStoreKitHelper: SKReceiptRefreshRequest failed" ));
		[self paymentQueue:[SKPaymentQueue defaultQueue] restoreCompletedTransactionsFailedWithError: error];
		[Request release];
	}

	// Check if we are doing a product info request
	else if ([Request isKindOfClass : [SKProductsRequest class]])
	{
		UE_LOG(LogOnline, Warning, TEXT( "FStoreKitHelper: SKProductsRequest failed" ));
		[self paymentQueue:[SKPaymentQueue defaultQueue] requestProductDataFailedWithError: error];
		[Request release];
	}
	else
	{
		UE_LOG(LogOnline, Warning, TEXT( "FStoreKitHelper: didFailWithError went unhandled" ));
	}
	
#endif
}

-(void)restorePurchases
{
#ifdef __IPHONE_7_0
	if ([IOSAppDelegate GetDelegate].OSVersion >= 7.0)
	{
		Request = [[SKReceiptRefreshRequest alloc] init];
		Request.delegate = self;
		[Request start];
    }
	else
#endif
	{
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_7_0
		[[SKPaymentQueue defaultQueue] restoreCompletedTransactions];
#endif
	}
}
@end



////////////////////////////////////////////////////////////////////
/// FOnlineStoreInterfaceIOS implementation


FOnlineStoreInterfaceIOS::FOnlineStoreInterfaceIOS() 
{
	UE_LOG(LogOnline, Verbose, TEXT( "FOnlineStoreInterfaceIOS::FOnlineStoreInterfaceIOS" ));
	StoreHelper = [[FStoreKitHelper alloc] init];
    
    [[SKPaymentQueue defaultQueue] addTransactionObserver:StoreHelper];

	bIsPurchasing = false;
	bIsProductRequestInFlight = false;
	bIsRestoringPurchases = false;
}


FOnlineStoreInterfaceIOS::~FOnlineStoreInterfaceIOS() 
{
	UE_LOG(LogOnline, Verbose, TEXT( "FOnlineStoreInterfaceIOS::~FOnlineStoreInterfaceIOS" ));
	[StoreHelper release];
}


bool FOnlineStoreInterfaceIOS::QueryForAvailablePurchases(const TArray<FString>& ProductIDs, FOnlineProductInformationReadRef& InReadObject)
{
	UE_LOG(LogOnline, Verbose, TEXT( "FOnlineStoreInterfaceIOS::QueryForAvailablePurchases" ));
	bool bSentAQueryRequest = false;

	CachedReadObject = InReadObject;
	
	if( bIsPurchasing || bIsProductRequestInFlight || bIsRestoringPurchases )
	{
		UE_LOG(LogOnline, Verbose, TEXT( "FOnlineStoreInterfaceIOS::BeginPurchase - cannot make a purchase whilst one is in transaction." ));
	}
	else if (ProductIDs.Num() == 0)
	{
		UE_LOG(LogOnline, Verbose, TEXT( "There are no product IDs configured for Microtransactions in the engine.ini" ));
	}
	else
	{
		// autoreleased NSSet to hold IDs
		NSMutableSet* ProductSet = [NSMutableSet setWithCapacity:ProductIDs.Num()];
		for (int32 ProductIdx = 0; ProductIdx < ProductIDs.Num(); ProductIdx++)
		{
			NSString* ID = [NSString stringWithFString:ProductIDs[ ProductIdx ]];
			// convert to NSString for the set objects
			[ProductSet addObject:ID];
		}
        
        dispatch_async(dispatch_get_main_queue(), ^
        {
            [StoreHelper requestProductData:ProductSet];
        });

		bIsProductRequestInFlight = true;

		bSentAQueryRequest = true;
	}

	return bSentAQueryRequest;
}


bool FOnlineStoreInterfaceIOS::IsAllowedToMakePurchases()
{
	UE_LOG(LogOnline, Verbose, TEXT( "FOnlineStoreInterfaceIOS::IsAllowedToMakePurchases" ));
	bool bCanMakePurchases = [SKPaymentQueue canMakePayments];
	return bCanMakePurchases;
}


bool FOnlineStoreInterfaceIOS::BeginPurchase(const FInAppPurchaseProductRequest& ProductRequest, FOnlineInAppPurchaseTransactionRef& InPurchaseStateObject)
{
	UE_LOG(LogOnline, Verbose, TEXT( "FOnlineStoreInterfaceIOS::BeginPurchase" ));
	bool bCreatedNewTransaction = false;
	
	const FString& ProductId = ProductRequest.ProductIdentifier;

	if( bIsPurchasing || bIsProductRequestInFlight || bIsRestoringPurchases)
	{
		UE_LOG(LogOnline, Verbose, TEXT( "FOnlineStoreInterfaceIOS::BeginPurchase - cannot make a purchase whilst one is in transaction." ));
        
		TriggerOnInAppPurchaseCompleteDelegates(EInAppPurchaseState::Failed);
	}
	else if( IsAllowedToMakePurchases() )
	{
		UE_LOG(LogOnline, Verbose, TEXT("FOnlineStoreInterfaceIOS - Making a transaction of product %s"), *ProductId);

		NSString* ID = [NSString stringWithFString : ProductId];
        
        dispatch_async(dispatch_get_main_queue(), ^
        {
            NSMutableSet* ProductSet = [NSMutableSet setWithCapacity:1];
            [ProductSet addObject:ID];
            
            // Purchase the product through the StoreKit framework
            [StoreHelper makePurchase:ProductSet];
        });
		
		// Flag that we are purchasing so we can manage subsequent callbacks and transaction requests
		bIsPurchasing = true;
		
		// We have created a new transaction on this pass.
		bCreatedNewTransaction = true;

		// Cache the readobject so we can add product information to it after the purchase
		CachedPurchaseStateObject = InPurchaseStateObject;
		CachedPurchaseStateObject->ReadState = EOnlineAsyncTaskState::InProgress;
	}
	else
	{
		UE_LOG(LogOnline, Verbose, TEXT("This device is not able to make purchases."));
		InPurchaseStateObject->ReadState = EOnlineAsyncTaskState::Failed;
        
        TriggerOnInAppPurchaseCompleteDelegates(EInAppPurchaseState::NotAllowed);
	}

	return bCreatedNewTransaction;
}


bool FOnlineStoreInterfaceIOS::RestorePurchases(const TArray<FInAppPurchaseProductRequest>& ConsumableProductFlags, FOnlineInAppPurchaseRestoreReadRef& InReadObject)
{
	bool bSentAQueryRequest = false;
	CachedPurchaseRestoreObject = InReadObject;

	if (bIsPurchasing || bIsProductRequestInFlight || bIsRestoringPurchases)
	{
		UE_LOG(LogOnline, Verbose, TEXT("FOnlineStoreInterfaceIOS::BeginPurchase - cannot make a purchase whilst one is in transaction."));
		TriggerOnInAppPurchaseRestoreCompleteDelegates(EInAppPurchaseState::Failed);
	}
	else
	{
		bIsRestoringPurchases = true;
		[StoreHelper restorePurchases];
		bSentAQueryRequest = true;
	}

	return bSentAQueryRequest;
}

void FOnlineStoreInterfaceIOS::ProcessProductsResponse( SKProductsResponse* Response )
{
	if( bIsPurchasing )
	{
		bool bWasSuccessful = [Response.products count] > 0;
		if( [Response.products count] == 1 )
		{
			SKProduct* Product = [Response.products objectAtIndex:0];

			NSNumberFormatter *numberFormatter = [[NSNumberFormatter alloc] init];
			[numberFormatter setFormatterBehavior:NSNumberFormatterBehavior10_4];
			[numberFormatter setNumberStyle:NSNumberFormatterCurrencyStyle];
			[numberFormatter setLocale:Product.priceLocale];

			UE_LOG(LogOnline, Log, TEXT("Making a purchase: Product: %s, Price: %s"), *FString([Product localizedTitle]), *FString([numberFormatter stringFromNumber:Product.price]));

			FInAppPurchaseProductInfo PurchaseProductInfo;
			PurchaseProductInfo.Identifier = [Product productIdentifier];
			PurchaseProductInfo.DisplayName = [Product localizedTitle];
			PurchaseProductInfo.DisplayDescription = [Product localizedDescription];
			PurchaseProductInfo.DisplayPrice = [numberFormatter stringFromNumber : Product.price];
			PurchaseProductInfo.CurrencyCode = [Product.priceLocale objectForKey : NSLocaleCurrencyCode];
			PurchaseProductInfo.CurrencySymbol = [Product.priceLocale objectForKey : NSLocaleCurrencySymbol];
			PurchaseProductInfo.DecimalSeparator = [Product.priceLocale objectForKey : NSLocaleDecimalSeparator];
			PurchaseProductInfo.GroupingSeparator = [Product.priceLocale objectForKey : NSLocaleGroupingSeparator];

			[numberFormatter release];

			CachedPurchaseStateObject->ProvidedProductInformation = PurchaseProductInfo;
            
            dispatch_async(dispatch_get_main_queue(), ^
            {
                // now that we have recently refreshed the info, we can purchase it
                SKPayment* Payment = [SKPayment paymentWithProduct:Product];
                [[SKPaymentQueue defaultQueue] addPayment:Payment];
            });
		}
		else if( [Response.products count] > 1 )
		{
			UE_LOG(LogOnline, Warning, TEXT("Wrong number of products, [%d], in the response when trying to make a single purchase"), [Response.products count]);
		}
		else
		{
			for(NSString *invalidProduct in Response.invalidProductIdentifiers)
			{
				UE_LOG(LogOnline, Error, TEXT("Problem in iTunes connect configuration for product: %s"), *FString(invalidProduct));
			}
		}

		bIsPurchasing = false;
	}
	else if( bIsProductRequestInFlight )
	{
		bool bWasSuccessful = [Response.products count] > 0;
		if( [Response.products count] == 0 && [Response.invalidProductIdentifiers count] == 0 )
		{
			UE_LOG(LogOnline, Warning, TEXT("Wrong number of products [%d] in the response when trying to make a single purchase"), [Response.products count]);
		}

		if( CachedReadObject.IsValid() )
		{
			for (SKProduct* Product in Response.products)
			{
				NSNumberFormatter *numberFormatter = [[NSNumberFormatter alloc] init];
				[numberFormatter setFormatterBehavior : NSNumberFormatterBehavior10_4];
				[numberFormatter setNumberStyle : NSNumberFormatterCurrencyStyle];
				[numberFormatter setLocale : Product.priceLocale];

				FInAppPurchaseProductInfo NewProductInfo;
				NewProductInfo.Identifier = [Product productIdentifier];
				NewProductInfo.DisplayName = [Product localizedTitle];
				NewProductInfo.DisplayDescription = [Product localizedDescription];
				NewProductInfo.DisplayPrice = [numberFormatter stringFromNumber : Product.price];
				NewProductInfo.CurrencyCode = [Product.priceLocale objectForKey : NSLocaleCurrencyCode];
				NewProductInfo.CurrencySymbol = [Product.priceLocale objectForKey : NSLocaleCurrencySymbol];
				NewProductInfo.DecimalSeparator = [Product.priceLocale objectForKey : NSLocaleDecimalSeparator];
				NewProductInfo.GroupingSeparator = [Product.priceLocale objectForKey : NSLocaleGroupingSeparator];

				UE_LOG(LogOnline, Log, TEXT("\nProduct Identifier: %s, Name: %s, Description: %s, Price: %s\n"),
					*NewProductInfo.Identifier,
					*NewProductInfo.DisplayName,
					*NewProductInfo.DisplayDescription,
					*NewProductInfo.DisplayPrice);

				[numberFormatter release];

				CachedReadObject->ProvidedProductInformation.Add( NewProductInfo );
			}
		}
		else
		{
			UE_LOG(LogOnline, Log, TEXT("Read Object is invalid."));
		}

		
		for( NSString *invalidProduct in Response.invalidProductIdentifiers )
		{
			UE_LOG(LogOnline, Warning, TEXT("Problem in iTunes connect configuration for product: %s"), *FString(invalidProduct));
		}

		TriggerOnQueryForAvailablePurchasesCompleteDelegates( bWasSuccessful );

		bIsProductRequestInFlight = false;
	}
}