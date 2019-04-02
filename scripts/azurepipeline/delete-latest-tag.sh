#!/bin/bash

# Called by "LuxCoreRender.LuxCore", the build pipeline for daily builds
# Deletes the "latest" tag from LuxCore git repo, so that it is then
# recreated at the current commit by the following release pipeline
# Runs only if at least one of the build jobs was successful 

git remote set-url origin git@github.com:LuxCoreRender/LuxCore.git
git tag --delete latest
git push --delete origin latest
