/**
 * @file gpg/ios_support.h
 *
 * @brief iOS specific support functions, used to enable iOS-specific features
 * like push notifications.
 */

#ifndef GPG_IOS_SUPPORT_H_
#define GPG_IOS_SUPPORT_H_

#include "gpg/platform_defines.h"

#ifdef __OBJC__

#include "gpg/common.h"

#import <UIKit/UIKit.h>

namespace gpg {

// Push Notification specific functionality.

/**
 * Attempts to handle a Play Games Services push notification payload which was
 * provided to the app via application:didFinishLaunchingWithOptions:.
 * Returns true if the payload is from Play Games Services and handles the
 * payload automatically. The resulting match invites/updates are sent back
 * through their respective delegates. This function should not be used for push
 * notifications provided from application:didReceiveRemoteNotification:.
 * Instead please call TryHandleRemoteNotification.
 *
 * @param launch_options the raw launch options payload from
 *     application:didFinishLaunchingWithOptions:
 */
bool GPG_EXPORT
    TryHandleNotificationFromLaunchOptions(NSDictionary *launch_options);

/**
 * Attempts to handle a Play Games Services push notification payload. Returns
 * true if the payload is from Play Games Services and handles the payload
 * automatically. The resulting match invites/updates are sent back through
 * their respective delegates. This function should not be used for push
 * notifications provided from application:didFinishLaunchingWithOptions:.
 * Instead please call TryHandleNotificationFromLaunchOptions.
 *
 * @param user_info the raw push notification payload from
 *     application:didReceiveRemoteNotification:
 */
bool GPG_EXPORT TryHandleRemoteNotification(NSDictionary *user_info);

/**
 * Registers the device with Play Games Services for future push tokens. Call
 * this after the user has signed in.
 *
 * @param device_token_data the raw 32 byte device token given by Apple in
 *     application:didRegisterForRemoteNotificationsWithDeviceToken:
 * @param is_sandbox_environment Whether to target to sandbox APNS server. Use
 *     true for testing and false for a deployed production environment.
 */
void GPG_EXPORT
    RegisterDeviceToken(NSData *device_token_data, bool is_sandbox_environment);

/**
 * Unregisters the device for future push notifications. Automatically called at
 * user sign out.
 */
void GPG_EXPORT UnregisterCurrentDeviceToken();

}  // namespace gpg

#endif  // __OBJC__

#endif  // GPG_IOS_SUPPORT_H_
