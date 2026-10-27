# SPDX-License-Identifier: MIT
# Dependency integration only. Never check out or execute upstream build scripts.
find_package(Git REQUIRED)
set(GRAVASTAR_REFERENCE_GIT_CACHE "${PROJECT_SOURCE_DIR}/build/reference-sources"
    CACHE PATH "Bare, pinned reference dependency object stores")

function(reference_repository name url tag revision)
    set(repository "${GRAVASTAR_REFERENCE_GIT_CACHE}/${name}.git")
    if(NOT EXISTS "${repository}")
        execute_process(COMMAND "${GIT_EXECUTABLE}" clone --bare --depth 1
            --single-branch --branch "${tag}" --template= "${url}" "${repository}"
            COMMAND_ERROR_IS_FATAL ANY)
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" "--git-dir=${repository}"
        rev-parse "${tag}^{commit}" OUTPUT_VARIABLE actual
        OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
    if(NOT actual STREQUAL revision)
        message(FATAL_ERROR "${name} ${tag}: expected ${revision}, found ${actual}")
    endif()
    set(${name}_repository "${repository}" PARENT_SCOPE)
endfunction()

function(reference_export repository revision source destination)
    get_filename_component(parent "${destination}" DIRECTORY)
    file(MAKE_DIRECTORY "${parent}")
    execute_process(COMMAND "${GIT_EXECUTABLE}" "--git-dir=${repository}"
        show "${revision}:${source}" OUTPUT_FILE "${destination}"
        COMMAND_ERROR_IS_FATAL ANY)
endfunction()

function(reference_export_matching repository revision pattern destination)
    execute_process(COMMAND "${GIT_EXECUTABLE}" "--git-dir=${repository}"
        ls-tree -r --name-only "${revision}" OUTPUT_VARIABLE paths
        COMMAND_ERROR_IS_FATAL ANY)
    string(REPLACE "\n" ";" paths "${paths}")
    foreach(path IN LISTS paths)
        if(path MATCHES "${pattern}")
            reference_export("${repository}" "${revision}" "${path}" "${destination}/${path}")
        endif()
    endforeach()
endfunction()
