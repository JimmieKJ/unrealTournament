#!/bin/bash

SCRIPT_DIR=$(cd "$(dirname "$BASH_SOURCE")" ; pwd)

# args: wrong filename, correct filename
# expects to be run in Engine folder
CreateLinkIfNoneExists()
{
    WrongName=$1
    CorrectName=$2

    pushd `dirname $CorrectName` > /dev/null
    if [ ! -f `basename $CorrectName` ] && [ -f $WrongName ]; then
      echo "$WrongName -> $CorrectName"
      ln -sf $WrongName `basename $CorrectName`
    fi
    popd > /dev/null
}


# main
set -e

TOP_DIR=$(cd $SCRIPT_DIR/../../.. ; pwd)
cd ${TOP_DIR}

IS_GITHUB_BUILD=true
if [ -e Build/PerforceBuild.txt ]; then
  IS_GITHUB_BUILD=false
fi

if [ -e /etc/os-release ]; then
  source /etc/os-release
  # Ubuntu/Debian/Mint
  if [[ "$ID" == "ubuntu" ]] || [[ "$ID_LIKE" == "ubuntu" ]] || [[ "$ID" == "debian" ]] || [[ "$ID_LIKE" == "debian" ]] || [[ "$ID" == "tanglu" ]] || [[ "$ID_LIKE" == "tanglu" ]]; then
    # Install the necessary dependencies (require clang-3.8 on 16.04, although 3.3 and 3.5 through 3.7 should work too for this release)
    if [[ "$VERSION_ID" < 16.04 ]]; then
     DEPS="mono-xbuild \
       mono-dmcs \
       libmono-microsoft-build-tasks-v4.0-4.0-cil \
       libmono-system-data-datasetextensions4.0-cil
       libmono-system-web-extensions4.0-cil
       libmono-system-management4.0-cil
       libmono-system-xml-linq4.0-cil
       libmono-corlib4.0-cil
       libmono-windowsbase4.0-cil
       libmono-system-io-compression4.0-cil
       libmono-system-io-compression-filesystem4.0-cil
       clang-3.5
       "
    elif [[ "$VERSION_ID" == 16.04 ]]; then
     DEPS="mono-xbuild \
       mono-dmcs \
       libmono-microsoft-build-tasks-v4.0-4.0-cil \
       libmono-system-data-datasetextensions4.0-cil
       libmono-system-web-extensions4.0-cil
       libmono-system-management4.0-cil
       libmono-system-xml-linq4.0-cil
       libmono-corlib4.5-cil
       libmono-windowsbase4.0-cil
       libmono-system-io-compression4.0-cil
       libmono-system-io-compression-filesystem4.0-cil
       clang-3.8
       "
    fi

    # these tools are only needed to build third-party software which is prebuilt for Ubuntu.
    if [[ "$ID" != "ubuntu" ]]; then
      DEPS+="libqt4-dev \
             cmake
            "
    fi

    for DEP in $DEPS; do
      if ! dpkg -s $DEP > /dev/null 2>&1; then
        echo "Attempting installation of missing package: $DEP"
        set -x
        sudo apt-get install -y $DEP
        set +x
      fi
    done
  fi
  
  # openSUSE/SLED/SLES
  if [[ "$ID" == "opensuse" ]] || [[ "$ID_LIKE" == "suse" ]]; then
    # Install all necessary dependencies
    DEPS="mono-core
      mono-devel
      mono-mvc
      libqt4-devel
      dos2unix
      cmake
      "

    for DEP in $DEPS; do
      if ! rpm -q $DEP > /dev/null 2>&1; then
        echo "Attempting installation of missing package: $DEP"
        set -x
        sudo zypper -n install $DEP
        set +x
      fi
    done
  fi

  # Fedora
  if [[ "$ID" == "fedora" ]]; then
    # Install all necessary dependencies
    DEPS="mono-core
      mono-devel
      qt-devel
      dos2unix
      cmake
      "

    for DEP in $DEPS; do
      if ! rpm -q $DEP > /dev/null 2>&1; then
        echo "Attempting installation of missing package: $DEP"
        set -x
        sudo dnf -y install $DEP
        set +x
      fi
    done
  fi


  # Arch Linux
  if [[ "$ID" == "arch" ]] || [[ "$ID_LIKE" == "arch" ]]; then
    DEPS="clang35 mono python sdl2 qt4 dos2unix cmake"
    MISSING=false
    for DEP in $DEPS; do
      if ! pacman -Qs $DEP > /dev/null 2>&1; then
        MISSING=true
        break
      fi
    done
    if [ "$MISSING" = true ]; then
      echo "Attempting to install missing packages: $DEPS"
      set -x
      sudo pacman -S --needed --noconfirm $DEPS
      set +x
    fi
  fi
fi

MONO_MINIMUM_VERSION=3
MONO_VERSION=$(mono -V | sed -n 's/.* version \([0-9]\+\).*/\1/p')
if [[ "$MONO_VERSION" -lt "$MONO_MINIMUM_VERSION" ]]; then
  echo "Minimum required Mono version is $MONO_MINIMUM_VERSION. Installed is:"
  mono -V | sed -n '/version/p'
  exit 1
fi

echo 
if [ "$IS_GITHUB_BUILD" = true ]; then
	echo Github build
	echo Checking / downloading the latest archives
	Build/BatchFiles/Linux/GitDependencies.sh --prompt "$@"
else
	echo Perforce build
	echo Assuming availability of up to date third-party libraries
fi

# Fixes for case sensitive filesystem.
echo Fixing inconsistent case in filenames.
for BASE in Content/Editor/Slate Content/Slate Documentation/Source/Shared/Icons; do
  find $BASE -name "*.PNG" | while read PNG_UPPER; do
    png_lower="$(echo "$PNG_UPPER" | sed 's/.PNG$/.png/')"
    if [ ! -f $png_lower ]; then
      PNG_UPPER=$(basename $PNG_UPPER)
      echo "$png_lower -> $PNG_UPPER"
      # link, and not move, to make it usable with Perforce workspaces
      ln -sf `basename "$PNG_UPPER"` "$png_lower"
    fi
  done
done

CreateLinkIfNoneExists ../../engine/shaders/Fxaa3_11.usf  ../Engine/Shaders/Fxaa3_11.usf
CreateLinkIfNoneExists ../../Engine/shaders/Fxaa3_11.usf  ../Engine/Shaders/Fxaa3_11.usf

echo Removing a stable libLND.so binary that was relocated in 4.8
rm -f ../Engine/Binaries/Linux/libLND.so

# We have to build libhlslcc locally due to apparent mismatch between system STL and cross-toolchain one
echo
pushd Build/BatchFiles/Linux > /dev/null
./BuildThirdParty.sh
popd > /dev/null

touch Build/OneTimeSetupPerformed
