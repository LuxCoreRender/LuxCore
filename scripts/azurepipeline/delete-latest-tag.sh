#!/bin/bash

# Called by "LuxCoreRender.LuxCore", the build pipeline for daily builds
# Deletes the "latest" tag (if existing) from LuxCore git repo, so that it is
# then recreated at the current commit by the following release pipeline.
# Runs only if at least one of the build jobs was successful 
# and pipeline has not been triggered by a pull request.

TAG=$(git tag -l latest)
if [[ $TAG == "latest" ]] ; then
    git remote set-url origin git@github.com:LuxCoreRender/LuxCore.git
    git tag --delete latest
    git push --delete origin latest
fi
