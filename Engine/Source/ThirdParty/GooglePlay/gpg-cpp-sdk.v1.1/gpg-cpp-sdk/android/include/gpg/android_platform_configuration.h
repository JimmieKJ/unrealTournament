/**
 * @file gpg/android_platform_configuration.h
 * @copyright Copyright 2014 Google Inc. All Rights Reserved.
 * @brief Platform-specific builder configuration for Android.
 */

#ifndef GPG_ANDROID_PLATFORM_CONFIGURATION_H_
#define GPG_ANDROID_PLATFORM_CONFIGURATION_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <jni.h>
#include <memory>
#include "gpg/common.h"
#include "gpg/quest.h"
#include "gpg/snapshot_metadata.h"

namespace gpg {

class AndroidPlatformConfigurationImpl;

/**
 * The platform configuration used when creating an instance of the GameServices
 * class on Android.
 */
class GPG_EXPORT AndroidPlatformConfiguration {
 public:
  AndroidPlatformConfiguration();
  ~AndroidPlatformConfiguration();

  /**
   * Specifies the activity that will own the GameServices object.
   *
   * The provided activity must implement Activity.onActivityResult() in such a
   * way that it allows the superclass implementation to handle invocations with
   * a requestCode having the special value 0x475047. This special value should
   * not be used by the application as the request code for calls to
   * Activity.startActivityForResult(), Activity.startIntentSenderForResult(),
   * etc. Standard practice for overriding Activity.onActivityResult(), which is
   * to pass invocations with unrecognized requestCodes to the superclass
   * implementation, should be followed.
   */
  AndroidPlatformConfiguration &SetActivity(jobject android_app_activity);
  /**
   * The callback type used with {@link SetOnLaunchedWithSnapshot}.
   * @ingroup callbacks
   */
  typedef std::function<void(SnapshotMetadata)> OnLaunchedWithSnapshotCallback;

  /**
   * The default callback called when the app is launched from the Play Games
   * Destination App by selecting a snapshot. Logs the ID of the quest. This can
   * be overridden by setting a new callback with
   * {@link SetOnLaunchedWithSnapshot}.
   */
  static void DEFAULT_ON_LAUNCHED_WITH_SNAPSHOT(SnapshotMetadata snapshot);

  /**
   * Registers a callback that will be called if the app is launched from the
   * Play Games Destination App by selecting a snapshot.
   */
  AndroidPlatformConfiguration &SetOnLaunchedWithSnapshot(
      OnLaunchedWithSnapshotCallback callback);

  /**
   * The callback type used with {@link SetOnLaunchedWithQuest}.
   * @ingroup callbacks
   */
  typedef std::function<void(Quest)> OnLaunchedWithQuestCallback;

  /**
   * The default callback called when the app is launched from the Play Games
   * Destination App by selecting a quest. Logs the ID of the quest. This can be
   * overridden by setting a new callback with {@link SetOnLaunchedWithQuest}.
   */
  static void DEFAULT_ON_LAUNCHED_WITH_QUEST(Quest quest);

  /**
   * Registers a callback that will be called if the app is launched from the
   * Play Games Destination App by selecting a quest.
   */
  AndroidPlatformConfiguration &SetOnLaunchedWithQuest(
      OnLaunchedWithQuestCallback callback);

  /**
   * Returns true if all required values were provided to the
   * AndroidPlatformConfiguration. In this case, the only required value is the
   * Activity.
   */
  bool Valid() const;

 private:
  AndroidPlatformConfiguration(AndroidPlatformConfiguration const &) = delete;
  AndroidPlatformConfiguration &operator=(
      AndroidPlatformConfiguration const &) = delete;

  friend class GameServicesImpl;
  std::unique_ptr<AndroidPlatformConfigurationImpl> impl_;
};

}  // namespace gpg

#endif  // GPG_ANDROID_PLATFORM_CONFIGURATION_H_
