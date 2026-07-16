# Read repo-root VERSION (call once from the top CMakeLists).
# Before project(): sets AEQUITAS_VERSION (+ MAJOR/MINOR/PATCH).
# After project(): call aequitas_write_version_header() to emit aequitas_version.h.

get_filename_component(_AEQ_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_AEQ_VERSION_FILE "${_AEQ_REPO_ROOT}/VERSION")

if(NOT EXISTS "${_AEQ_VERSION_FILE}")
    message(FATAL_ERROR "VERSION file missing at ${_AEQ_VERSION_FILE}")
endif()

file(READ "${_AEQ_VERSION_FILE}" _AEQ_VERSION_RAW)
string(STRIP "${_AEQ_VERSION_RAW}" AEQUITAS_VERSION)

if(NOT AEQUITAS_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must be SemVer MAJOR.MINOR.PATCH (got '${AEQUITAS_VERSION}')")
endif()

string(REPLACE "." ";" _AEQ_VER_PARTS "${AEQUITAS_VERSION}")
list(GET _AEQ_VER_PARTS 0 AEQUITAS_VERSION_MAJOR)
list(GET _AEQ_VER_PARTS 1 AEQUITAS_VERSION_MINOR)
list(GET _AEQ_VER_PARTS 2 AEQUITAS_VERSION_PATCH)

function(aequitas_write_version_header)
    set(AEQUITAS_VERSION_HEADER_DIR "${CMAKE_BINARY_DIR}/generated" PARENT_SCOPE)
    set(_hdr_dir "${CMAKE_BINARY_DIR}/generated")
    file(MAKE_DIRECTORY "${_hdr_dir}")
    configure_file(
        "${_AEQ_REPO_ROOT}/cmake/aequitas_version.h.in"
        "${_hdr_dir}/aequitas_version.h"
        @ONLY
    )
endfunction()
