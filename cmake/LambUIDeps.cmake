# Third-party sources are checked out by the repo tool (lambui.xml manifest)
# rather than downloaded at configure time.
set(LAMBUI_DEPS_DIR "${PROJECT_SOURCE_DIR}/../third_party" CACHE PATH
    "Directory holding LambUI's third-party source checkouts")

# lambui_add_dependency(<dir> [TARGET <target>] [NO_ADD_SUBDIRECTORY])
# Sets <lowercased dir>_SOURCE_DIR like FetchContent does and add_subdirectory()s
# the checkout, unless TARGET already exists (e.g. provided by a parent project).
function(lambui_add_dependency name)
    cmake_parse_arguments(arg "NO_ADD_SUBDIRECTORY" "TARGET" "" ${ARGN})
    if (arg_TARGET AND TARGET ${arg_TARGET})
        return()
    endif()

    set(src "${LAMBUI_DEPS_DIR}/${name}")
    if (NOT IS_DIRECTORY "${src}")
        message(FATAL_ERROR
            "LambUI: dependency '${name}' not found at ${src}. "
            "Run 'repo sync' with the lambui.xml manifest, or set LAMBUI_DEPS_DIR.")
    endif()

    string(TOLOWER "${name}" lname)
    set(${lname}_SOURCE_DIR "${src}" PARENT_SCOPE)
    if (NOT arg_NO_ADD_SUBDIRECTORY)
        add_subdirectory("${src}" "${CMAKE_BINARY_DIR}/_deps/${lname}-build")
    endif()
endfunction()
