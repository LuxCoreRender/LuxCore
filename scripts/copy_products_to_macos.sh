#!/bin/bash

# copy built (Release) luxrays results to macos directory
# kroko

###########################################

LUXSRCDIRNAME=$(dirname "$(dirname "$(dirname "$0")")")

###########################################

HEADERSFROM=$LUXSRCDIRNAME
HEADERSFROM+="/luxrays/include/"

HEADERSTO=$LUXSRCDIRNAME
HEADERSTO+="/macos/include/LuxRays/"

cp -Rfp "${HEADERSFROM}" "${HEADERSTO}"

###########################################

LIBSFROM=$LUXSRCDIRNAME
LIBSFROM+="/luxrays/build/lib/Release/"

LIBSTO=$LUXSRCDIRNAME
LIBSTO+="/macos/lib/LuxRays/"

cp -Rfp "${LIBSFROM}" "${LIBSTO}"

###########################################

EMBREE2FROM=$LUXSRCDIRNAME
EMBREE2FROM+="/luxrays/build/lib/Release/libembree.2.4.0.dylib"

EMBREE2TO=$LUXSRCDIRNAME
EMBREE2TO+="/macos/lib/embree2/"

cp -Rfp "${EMBREE2FROM}" "${EMBREE2TO}"

