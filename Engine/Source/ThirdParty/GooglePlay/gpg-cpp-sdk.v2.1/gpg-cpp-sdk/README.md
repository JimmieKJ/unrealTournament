#Google Play Games C++ SDK Version 2.1 #
This library contains the Google Play Games C++ SDK for iOS and Android.
It also includes an ObjectiveC SDK for iOS.

## C++ Documentation ##

A getting started guide is available at:
https://developers.google.com/games/services/cpp/GettingStartedNativeClient

API documentation is available at:
https://developers.google.com/games/services/cpp/api/annotated

Samples are available at:
https://developers.google.com/games/services/downloads

## ObjectiveC Documentation ##
To get started building games on iOS with the SDK, please visit:
https://developers.google.com/games/services/ios/quickstart

API reference documentation is available at:
https://developers.google.com/games/services/ios/api/annotated

## File Structure ##

README.md  -- This file.

android/ - Files needed to use this SDK with an Android app.
    include/ - SDK headers. Should be added to an app's include path.
    lib/ - Compiled libraries for Android. Includes armeabi, armeabi-7a and x86.
    license.txt - License text for dependencies included by the Android SDK.

ios/ - Files needed to use this SDK with an iOS app.
    gpg.bundle - A resource bundle that must be included with iOS apps built on
                 this SDK.
    gpg.framework - A framework containing headers and libraries for iOS.
                    Supports both simulator and device.
    license.txt - License text for dependencies included by the iOS SDK.
