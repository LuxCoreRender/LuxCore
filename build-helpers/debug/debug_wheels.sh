# SPDX-FileCopyrightText: 2024 Howetuft
#
# SPDX-License-Identifier: Apache-2.0

# Debug linux build locally


# Script to run build locally for Linux.
# You need 'act' to be installed in your system

#export CIBW_DEBUG_KEEP_CONTAINER=TRUE

python_version_minor=$(python -c 'import sys; print(sys.version_info[1])')
python_version_minor=12

act workflow_dispatch \
  --pull \
  --action-offline-mode \
  --workflows ".github/workflows/wheel-builder.yml" \
  -s GITHUB_TOKEN="$(gh auth token)" \
  --matrix os:ubuntu-latest \
  --matrix python-minor:${python_version_minor} \
  --input build-type=Debug \
  --artifact-server-path /tmp/pyluxcore \
  --rm \
  | tee /tmp/pyluxcore.log
  #&& unzip -o ${zipfolder}/cibw-wheels-ubuntu-latest-13.zip -d ${zipfolder}
