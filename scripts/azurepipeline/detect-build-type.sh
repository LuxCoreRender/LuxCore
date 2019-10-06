#!/bin/bash

# Called by the "LuxCoreRender.LuxCore" build pipeline
# Detect release type (daily, alpha, beta, rc or final) and set version string
# One, and only one, tag in the form "luxcorerender_v*" is needed, 
# otherwise the official release build aborts.

echo "Detecting build type..."
echo "Commit ID: $BUILD_SOURCEVERSION"
TAGS=$(git tag --points-at $BUILD_SOURCEVERSION)
for tag in $TAGS
do
    if [[ $tag != "latest" ]] && [[ $tag == "luxcorerender_v"* ]] ; then
        if [[ -z "$VERSION_STRING" ]] ; then
            VERSION_STRING=$(echo $tag | cut -d'_' -f 2)
        else
            echo "Multiple git tags identify different versions, aborting..."
            exit 1
        fi
    fi
done

if [[ -z "$VERSION_STRING" ]] ; then
    echo "No release git tag found, this is a daily build"
    VERSION_STRING="latest"
fi
echo "Detected build type: $VERSION_STRING"

if [[ $VERSION_STRING == *"alpha"* ]] || \
   [[ $VERSION_STRING == *"beta"* ]] || \
   [[ $VERSION_STRING == *"RC"* ]] || \
   [[ $VERSION_STRING == *"rc"* ]] || \
   [[ $VERSION_STRING == "latest" ]] ; then
    FINAL="FALSE"
else
    FINAL="TRUE"
fi

# Generate/copy files to be published for release pipeline usage
echo luxcorerender_$VERSION_STRING > $BUILD_ARTIFACTSTAGINGDIRECTORY/github_release_tag
cp ./scripts/azurepipeline/read-release-tag.sh $BUILD_ARTIFACTSTAGINGDIRECTORY/read-release-tag.sh
cp ./release-notes.txt $BUILD_ARTIFACTSTAGINGDIRECTORY/release-notes.txt

# make FINAL and VERSION_STRING variables available for other pipeline jobs
echo "##vso[task.setvariable variable=final;isOutput=true]$FINAL"
echo "##vso[task.setvariable variable=version_string;isOutput=true]$VERSION_STRING"
