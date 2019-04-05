#!/bin/bash

# Read release tag name from staged file and output it to variable
# Allows "Autorelease" release pipeline to work even if it finds more than one tag

RELEASE_TAG=$(cat $SYSTEM_DEFAULTWORKINGDIRECTORY/_LuxCoreRender.LuxCore_Release/LuxCore/github_release_tag)
echo $RELEASE_TAG
echo "##vso[task.setvariable variable=ReleaseTag;]$RELEASE_TAG"
