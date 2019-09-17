#!/bin/bash

# Read release tag name and build type from staged file and output it to variable
# Allows release pipeline to work even if it finds more than one tag
# (e.g. "latest" + "luxcorerender_v*")
# Build type (FinalBuild) allows to choose release pipeline stage with/without pre-release label

RELEASE_TAG=$(cat $SYSTEM_DEFAULTWORKINGDIRECTORY/_LuxCoreRender.LuxCore/LuxCore/github_release_tag)
echo $RELEASE_TAG

if [[ $RELEASE_TAG == *"alpha"* ]] || \
   [[ $RELEASE_TAG == *"beta"* ]] || \
   [[ $RELEASE_TAG == *"RC"* ]] ; then
    FINAL="FALSE"
elif [[ $RELEASE_TAG == *"latest"* ]] ; then
    FINAL="FALSE"
    RELEASE_TAG="latest"
else
    FINAL="TRUE"
fi

echo "##vso[task.setvariable variable=ReleaseTag;]$RELEASE_TAG"
echo "##vso[task.setvariable variable=FinalBuild;]$FINAL"
