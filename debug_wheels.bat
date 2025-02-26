REM @echo off
REM SPDX-FileCopyrightText: 2024 Howetuft
REM
REM SPDX-License-Identifier: Apache-2.0

REM Debug linux build locally


REM Script to run build locally for Linux.
REM You need 'act' to be installed in your system

REM export CIBW_DEBUG_KEEP_CONTAINER=TRUE

REM Get Github token
FOR /F "tokens=*" %%g IN ('gh auth token') do (SET TOKEN=%%g)


act.exe workflow_dispatch^
  --action-offline-mode^
  --platform="windows-latest=-self-hosted"^
  --job=build-wheels^
  --pull=false^
  --secret="GITHUB_TOKEN=%TOKEN%"^
  --matrix=os:windows-latest^
  --matrix=python-minor:11^
  --artifact-server-path=/tmp/pyluxcore^
  --rm
