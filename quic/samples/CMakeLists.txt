# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

if(NOT BUILD_TESTS)
  return()
endif()

file(
  GLOB_RECURSE QUIC_API_HEADERS_TOINSTALL
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  *.h
)
list(FILTER QUIC_API_HEADERS_TOINSTALL EXCLUDE REGEX test/)
foreach(header ${QUIC_API_HEADERS_TOINSTALL})
  get_filename_component(header_dir ${header} DIRECTORY)
  install(FILES ${header} DESTINATION include/quic/samples/${header_dir})
endforeach()

add_subdirectory(echo)

add_executable(main echo/main.cpp)

target_compile_options(
  main
  PRIVATE
  ${_QUIC_COMMON_COMPILE_OPTIONS}
)

add_dependencies(
  main
  z_quic
)

target_link_libraries(
  main PUBLIC
  z_quic
  ${GFLAGS_LIBRARIES}
  ${LIBGMOCK_LIBRARIES}
)

install(
  TARGETS main
  DESTINATION bin
)
