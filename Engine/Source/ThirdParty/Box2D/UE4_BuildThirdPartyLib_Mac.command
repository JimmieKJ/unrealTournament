#!/bin/sh

pushd Box2D_v2.3.1
echo Compiling Box2D (Mac command file)
echo //@TODO: Paper2D: Test compiling Box2D for Mac and iOS (not tested yet!)


# compile Mac
p4 edit $THIRD_PARTY_CHANGELIST Build/Mac/lib...
pushd Build/xcode5
xcodebuild -sdk macosx -configuration Debug clean
xcodebuild -sdk macosx -configuration Debug
cp build/Debug-macosx/libBox2D.a ../Mac/lib/libBox2Dd.a
xcodebuild -sdk macosx -configuration Release clean
xcodebuild -sdk macosx -configuration Release
cp build/Release-macosx/libBox2D.a ../Mac/lib/libBox2D.a
popd

# compile IOS
p4 edit $THIRD_PARTY_CHANGELIST Build/IOS/lib...
pushd Build/xcode5
xcodebuild -sdk iphoneos -configuration Debug clean
xcodebuild -sdk iphoneos -configuration Debug
cp build/Debug-iphone/libBox2D.a ../IOS/lib/Device/libBox2Dd.a
xcodebuild -sdk iphoneos -configuration Release clean
xcodebuild -sdk iphoneos -configuration Release
cp build/Release-iphone/libBox2D.a ../IOS/lib/Device/libBox2D.a

xcodebuild -sdk iphonesimulator -configuration Debug clean
xcodebuild -sdk iphonesimulator -configuration Debug
cp build/Debug-iphonesimulator/libBox2D.a ../IOS/lib/Simulator/libBox2Dd.a
xcodebuild -sdk iphonesimulator -configuration Release clean
xcodebuild -sdk iphonesimulator -configuration Release
cp build/Release-iphonesimulator/libBox2D.a ../IOS/lib/Simulator/libBox2D.a
popd

popd
