# CMake common helper functions module

include_guard(GLOBAL)

# check_uuid: Helper function to check for valid UUID
function(check_uuid uuid_string return_value)
  set(valid_uuid TRUE)
  # gersemi: off
  set(uuid_token_lengths 8 4 4 4 12)
  # gersemi: on
  set(token_num 0)

  string(REPLACE "-" ";" uuid_tokens ${uuid_string})
  list(LENGTH uuid_tokens uuid_num_tokens)

  if(uuid_num_tokens EQUAL 5)
    message(DEBUG "UUID ${uuid_string} is valid with 5 tokens.")
    foreach(uuid_token IN LISTS uuid_tokens)
      list(GET uuid_token_lengths ${token_num} uuid_target_length)
      string(LENGTH "${uuid_token}" uuid_actual_length)
      if(uuid_actual_length EQUAL uuid_target_length)
        string(REGEX MATCH "[0-9a-fA-F]+" uuid_hex_match ${uuid_token})
        if(NOT uuid_hex_match STREQUAL uuid_token)
          set(valid_uuid FALSE)
          break()
        endif()
      else()
        set(valid_uuid FALSE)
        break()
      endif()
      math(EXPR token_num "${token_num}+1")
    endforeach()
  else()
    set(valid_uuid FALSE)
  endif()
  message(DEBUG "UUID ${uuid_string} valid: ${valid_uuid}")
  set(${return_value} ${valid_uuid} PARENT_SCOPE)
endfunction()

function(target_install_manifest target)
  set(options "")
  set(oneValueArgs DESTINATION)
  set(multiValueArgs "")
  cmake_parse_arguments(PARSE_ARGV 1 _TIM "${options}" "${oneValueArgs}" "${multiValueArgs}")

  if(OS_WINDOWS)
    set(PLUGIN_OS_ARCH "windows-x64")
  elseif(OS_MACOS)
    set(PLUGIN_OS_ARCH "macos-universal")
  else()
    set(PLUGIN_OS_ARCH "linux-${CMAKE_SYSTEM_PROCESSOR}")
  endif()
  set(PLUGIN_DESCRIPTION "Cut out a region of another source with an optional mask.")

  set(manifest_file "${CMAKE_CURRENT_BINARY_DIR}/${target}-manifest.json")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/resources/manifest.json.in" "${manifest_file}" @ONLY)

  if(OS_MACOS)
    target_sources(${target} PRIVATE "${manifest_file}")
    set_source_files_properties("${manifest_file}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    source_group("Resources" FILES "${manifest_file}")
    return()
  endif()

  if(_TIM_DESTINATION)
    install(FILES "${manifest_file}" DESTINATION "${_TIM_DESTINATION}" RENAME manifest.json)
  endif()

  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/rundir/$<CONFIG>/${target}"
    COMMAND
      "${CMAKE_COMMAND}" -E copy_if_different "${manifest_file}"
      "${CMAKE_CURRENT_BINARY_DIR}/rundir/$<CONFIG>/${target}/manifest.json"
    COMMENT "Copy ${target} manifest to rundir"
    VERBATIM
  )
endfunction()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/plugin-support.c.in")
  configure_file(src/plugin-support.c.in plugin-support.c @ONLY)
  add_library(plugin-support STATIC)
  target_sources(plugin-support PRIVATE plugin-support.c PUBLIC src/plugin-support.h)
  target_include_directories(plugin-support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
  if(OS_LINUX OR OS_FREEBSD OR OS_OPENBSD)
    # add fPIC on Linux to prevent shared object errors
    set_property(TARGET plugin-support PROPERTY POSITION_INDEPENDENT_CODE ON)
  endif()
endif()
