#!/bin/sh

# You may run in trouble on localized Linux installations
# because of ',' parsed as decimal separator instead of '.' char

LC_ALL=C ./luxmark-linux64 $@
