function(generate_file_header)
  set(options)
  set(oneValueArgs OUTPUT_HEADER VARIABLE_NAME NAMESPACE)
  set(multiValueArgs INPUT_FILES)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT DEFINED ARG_OUTPUT_HEADER)
    message(FATAL_ERROR "OUTPUT_HEADER is required")
  endif()

  if(NOT DEFINED ARG_INPUT_FILES)
    message(FATAL_ERROR "At least one INPUT_FILE is required")
  endif()

  if(NOT DEFINED ARG_VARIABLE_NAME)
    set(ARG_VARIABLE_NAME "FILE_CONTENT")
  endif()

  set(combined_content "")
  foreach(input_file ${ARG_INPUT_FILES})
    if(NOT EXISTS "${input_file}")
      message(FATAL_ERROR "Input file not found: ${input_file}")
    endif()

    file(READ "${input_file}" file_content)
    string(APPEND combined_content "${file_content}")
  endforeach()

  string(REPLACE "\\" "\\\\" escaped_content "${combined_content}")
  string(REPLACE "\"" "\\\"" escaped_content "${escaped_content}")
  string(REPLACE "\n" "\\n\"\n\"" escaped_content "${escaped_content}")

  get_filename_component(guard_name "${ARG_OUTPUT_HEADER}" NAME)
  string(MAKE_C_IDENTIFIER "${guard_name}" guard_name)
  string(TOUPPER "${guard_name}" guard_name)
  set(guard_name "GENERATED_${guard_name}")

  set(header_content "#ifndef ${guard_name}\n")
  string(APPEND header_content "#define ${guard_name}\n\n")

  if(DEFINED ARG_NAMESPACE)
    string(APPEND header_content "namespace ${ARG_NAMESPACE} {\n\n")
  endif()

  string(APPEND header_content "constexpr const char* ${ARG_VARIABLE_NAME} = \n\"${escaped_content}\";\n\n")

  if(DEFINED ARG_NAMESPACE)
    string(APPEND header_content "} // namespace ${ARG_NAMESPACE}\n\n")
  endif()

  string(APPEND header_content "#endif // ${guard_name}\n")

  file(WRITE "${ARG_OUTPUT_HEADER}" "${header_content}")

  message(STATUS "Generated header: ${ARG_OUTPUT_HEADER}")
endfunction()

function(generate_file_map_header)
  # Parse function arguments
  set(options)
  set(oneValueArgs OUTPUT_HEADER MAP_NAME NAMESPACE)
  set(multiValueArgs INPUT_FILES)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  # Validate required arguments
  if(NOT DEFINED ARG_OUTPUT_HEADER)
    message(FATAL_ERROR "OUTPUT_HEADER is required")
  endif()

  if(NOT DEFINED ARG_INPUT_FILES)
    message(FATAL_ERROR "At least one INPUT_FILE is required")
  endif()

  if(NOT DEFINED ARG_MAP_NAME)
    set(ARG_MAP_NAME "FILE_CONTENT_MAP")
  endif()

  get_filename_component(guard_name "${ARG_OUTPUT_HEADER}" NAME)
  string(MAKE_C_IDENTIFIER "${guard_name}" guard_name)
  string(TOUPPER "${guard_name}" guard_name)
  set(guard_name "GENERATED_${guard_name}")

  set(header_content "#ifndef ${guard_name}\n")
  string(APPEND header_content "#define ${guard_name}\n\n")

  string(APPEND header_content "#include <array>\n\n")

  if(DEFINED ARG_NAMESPACE)
    string(APPEND header_content "namespace ${ARG_NAMESPACE} {\n\n")
  endif()

  string(APPEND header_content "constexpr std::array ${ARG_MAP_NAME} = {\n")

  foreach(input_file ${ARG_INPUT_FILES})
    if(NOT EXISTS "${input_file}")
      message(FATAL_ERROR "Input file not found: ${input_file}")
    endif()

    get_filename_component(file_base_name "${input_file}" NAME_WE)

    file(READ "${input_file}" file_content)

    string(REPLACE "\\" "\\\\" escaped_content "${file_content}")
    string(REPLACE "\"" "\\\"" escaped_content "${escaped_content}")
    string(REPLACE "\n" "\\n\"\n\"" escaped_content "${escaped_content}")

    string(APPEND header_content "    std::pair {\"${file_base_name}\", \"${escaped_content}\"},\n")
  endforeach()

  string(REGEX REPLACE ",\n$" "\n" header_content "${header_content}")
  string(APPEND header_content "};\n\n")

  if(DEFINED ARG_NAMESPACE)
    string(APPEND header_content "} // namespace ${ARG_NAMESPACE}\n\n")
  endif()

  string(APPEND header_content "#endif // ${guard_name}\n")

  file(WRITE "${ARG_OUTPUT_HEADER}" "${header_content}")

  message(STATUS "Generated header with file map: ${ARG_OUTPUT_HEADER}")
endfunction()
