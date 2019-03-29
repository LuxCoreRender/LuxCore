#!/bin/bash

# Detect release type (official or daily) and set version tag
echo "$RELEASE_BUILD"
if [[ $RELEASE_BUILD == "TRUE" ]] ; then
    echo "This is an official release build"
    echo $BUILD_SOURCEVERSION
    VERSION_STRING=$(git tag --points-at $BUILD_SOURCEVERSION | cut -d'_' -f 2)
else
    VERSION_STRING="latest"
fi

echo "$VERSION_STRING"
if [[ -z "$VERSION_STRING" ]] ; then
    echo "No git tag found, one is needed for an official release"
    exit 1
fi

if [[ $VERSION_STRING == *"alpha"* ]] || \
   [[ $VERSION_STRING == *"beta"* ]] || \
   [[ $VERSION_STRING == "latest" ]] ; then
    FINAL="FALSE"
else
    FINAL="TRUE"
fi

echo "##vso[task.setvariable variable=final;isOutput=true]$FINAL"
echo "##vso[task.setvariable variable=version_string;isOutput=true]$VERSION_STRING"
