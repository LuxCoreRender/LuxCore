
set(template "${CMAKE_CURRENT_LIST_DIR}/PreprocessedKernel.template")

file(READ ${SRC} CL_SOURCE_CODE)

# Escape quotes:
string(REGEX REPLACE "\\\\" "\\\\\\\\" CL_SOURCE_CODE "${CL_SOURCE_CODE}")
string(REGEX REPLACE "\"" "\\\\\"" CL_SOURCE_CODE "${CL_SOURCE_CODE}")

# Wrap lines in string literals:
string(REGEX REPLACE "\r?\n$" "" CL_SOURCE_CODE "${CL_SOURCE_CODE}")
string(REGEX REPLACE "\r?\n" "\\\\n\"\n\"" CL_SOURCE_CODE "\"${CL_SOURCE_CODE}\\n\"")

configure_file(${template} ${DST} @ONLY)
