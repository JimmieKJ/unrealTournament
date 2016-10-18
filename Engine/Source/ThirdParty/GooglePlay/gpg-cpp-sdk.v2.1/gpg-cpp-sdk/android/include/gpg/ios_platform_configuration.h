/**
 * @file gpg/ios_platform_configuration.h
 *
 * @brief Platform-specific builder configuration for iOS.
 */

#ifndef GPG_IOS_PLATFORM_CONFIGURATION_H_
#define GPG_IOS_PLATFORM_CONFIGURATION_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <memory>

#include "gpg/common.h"

#ifdef __OBJC__
#import <UIKit/UIKit.h>
#endif

namespace gpg {

class IosPlatformConfigurationImpl;

/**
 * The platform configuration used when creating an instance of the GameServices
 * class on iOS.
 */
class GPG_EXPORT IosPlatformConfiguration {
 public:
  IosPlatformConfiguration();
  ~IosPlatformConfiguration();

  /**
   * Sets the Client ID, using a value obtained beforehand from the Google Play
   * Developer Console.
   */
  IosPlatformConfiguration &SetClientID(std::string const &client_id);

#ifdef __OBJC__
  IosPlatformConfiguration &SetOptionalViewControllerForPopups(
      UIViewController *ui_view_controller);
#endif  // __OBJC__

  /**
   * Returns true if all required values were provided to the
   * IosPlatformConfiguration. In this case, the only required value is the
   * Client ID.
   */
  bool Valid() const;

 private:
  IosPlatformConfiguration(IosPlatformConfiguration const &) = delete;
  IosPlatformConfiguration &operator=(IosPlatformConfiguration const &) =
      delete;

  friend class GameServicesImpl;
  std::unique_ptr<IosPlatformConfigurationImpl> impl_;
};

}  // namespace gpg

#endif  // GPG_IOS_PLATFORM_CONFIGURATION_H_
