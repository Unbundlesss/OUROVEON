#!/bin/bash
set -e 

local_appdir=$1.app
lower_app=$(echo $1 | tr [:upper:] [:lower:])

rm -rf ./$local_appdir

mkdir $local_appdir
mkdir $local_appdir/Contents
mkdir $local_appdir/Contents/MacOS
mkdir $local_appdir/Contents/Resources

cp -r ../../bin/shared ./$local_appdir/Contents/Resources/shared

cp -a ../../bin/$lower_app/macosx_release_universal/$1 ./$local_appdir/Contents/MacOS/$1

cp -a $1.Info.plist $local_appdir/Contents/Info.plist

cp -a ../../brand/AppIcon_$1.icns ./$local_appdir/Contents/Resources/$1.icns

codesign -f -s $2 -v --timestamp --deep --options runtime $local_appdir

current_time=$(date "+%Y%m%d-%H%M%S")
submission_file=$current_time.$1.zip

/usr/bin/ditto -c -k --keepParent "$local_appdir" "$submission_file"


# store-credentials 
# https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution/customizing_the_notarization_workflow
xcrun notarytool submit "$submission_file" --keychain-profile "AC_PASS" --wait

xcrun stapler staple -v "$local_appdir"
