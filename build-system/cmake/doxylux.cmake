# SPDX-FileCopyrightText: 2024-2025 Authors (see AUTHORS.txt)
#
# SPDX-License-Identifier: Apache-2.0

# https://stackoverflow.com/questions/8247189/doxygen-is-slow/8247993#8247993
# Submodules are just tagged, and main target merges everything


set(DOC_SOURCE_DIR ${PROJECT_SOURCE_DIR}/doxygen)
set(DOC_BUILD_DIR ${PROJECT_BINARY_DIR}/doc)

find_package(Doxygen)

if (NOT TARGET doc)
  if(DOXYGEN_FOUND)
    configure_file("${DOC_SOURCE_DIR}/all.template.in" "${DOC_BUILD_DIR}/Doxyfile")
    doxygen_add_docs(doc
      ${DOC_SOURCE_DIR}/main.md
      WORKING_DIRECTORY ${DOC_BUILD_DIR}
      COMMENT "Generating API documentation"
      CONFIG_FILE ${DOC_BUILD_DIR}/Doxyfile
    )
    # Config/Release is not relevant for doc (thus ../doc/html)
    install(DIRECTORY ${DOC_BUILD_DIR}/html/ DESTINATION ../doc/html COMPONENT doc)
  else()
    message(AUTHOR_WARNING "Doxygen not found: documentation generation will not be available")
  endif(DOXYGEN_FOUND)
endif()

macro(generate_doc target)
  if(DOXYGEN_FOUND)

    # Set Doxygen input and output
    set(DOXYGEN_INPUT_CONFIG ${DOC_BUILD_DIR}/doxygen-${target}.conf)
    set(DOXYGEN_OUTPUT_TAGFILE ${DOC_BUILD_DIR}/${target}.tag)
    set(DOXYGEN_OUTPUT_HTML ${DOC_BUILD_DIR}/html/${target})

    message(STATUS "Doxygen ${target} tags output: " ${DOXYGEN_OUTPUT_TAGFILE})
    message(STATUS "Doxygen ${target} html output: " ${DOXYGEN_OUTPUT_HTML})

    # Generate doxygen.template
    set(DOXYGEN_TEMPLATE_IN ${DOC_SOURCE_DIR}/${target}.template.in)
    configure_file("${DOXYGEN_TEMPLATE_IN}" "${DOXYGEN_INPUT_CONFIG}")


    if(DOXYGEN_DOT_FOUND)
        message(STATUS "Found dot")
        set(DOXYGEN_DOT_CONF YES)
    endif(DOXYGEN_DOT_FOUND)

    # Generate tag file
    if(NOT WIN32)
        add_custom_command(
          OUTPUT ${DOXYGEN_OUTPUT_TAGFILE} ${DOXYGEN_OUTPUT_HTML}
          # Customizing doxygen-xxx.conf
          COMMAND echo "INPUT = " ${PROJECT_SOURCE_DIR}/include/${target}  ${PROJECT_SOURCE_DIR}/src/${target} >> ${DOXYGEN_INPUT_CONFIG}
          COMMAND echo "OUTPUT_DIRECTORY = " ${DOC_BUILD_DIR} >> ${DOXYGEN_INPUT_CONFIG}
          COMMAND echo "HAVE_DOT = ${DOXYGEN_DOT_CONF}" >> ${DOXYGEN_INPUT_CONFIG}
          # Launch doxygen
          COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_INPUT_CONFIG}
          DEPENDS ${DOXYGEN_INPUT_CONFIG}
          WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        )
    else()
        message(STATUS "DOXYGEN ${DOXYGEN_EXECUTABLE}")
        add_custom_command(
          OUTPUT ${DOXYGEN_OUTPUT_TAGFILE} ${DOXYGEN_OUTPUT_HTML}
          # Customizing doxygen-xxx.conf
          COMMAND @echo INPUT = "${PROJECT_SOURCE_DIR}/include/${target}"  "${PROJECT_SOURCE_DIR}/src/${target}" >> ${DOXYGEN_INPUT_CONFIG}
          COMMAND @echo OUTPUT_DIRECTORY = \"${DOC_BUILD_DIR}\" >> ${DOXYGEN_INPUT_CONFIG}
          COMMAND @echo HAVE_DOT = ${DOXYGEN_DOT_CONF} >> ${DOXYGEN_INPUT_CONFIG}
          COMMAND @echo on
          # Launch doxygen
          COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_INPUT_CONFIG}
          DEPENDS ${DOXYGEN_INPUT_CONFIG}
          WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        )
    endif()

    # Generate html

    add_custom_target(doc-${target}
      DEPENDS ${DOXYGEN_OUTPUT_TAGFILE} ${DOXYGEN_OUTPUT_HTML}
    )
    add_dependencies(doc doc-${target})
    message(STATUS "Target 'doc-${target}' declared for ${target} documentation")
  endif(DOXYGEN_FOUND)

endmacro()
