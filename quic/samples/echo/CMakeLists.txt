# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

add_library(
  z_quic STATIC
  client.cc
  server.cc
)

target_include_directories(
  z_quic PUBLIC
  $<BUILD_INTERFACE:${QUIC_FBCODE_ROOT}>
  $<INSTALL_INTERFACE:include/>
)

target_compile_options(
  z_quic
  PRIVATE
  ${_QUIC_COMMON_COMPILE_OPTIONS}
)

add_dependencies(
  z_quic
  mvfst_constants
  mvfst_exception
  mvfst_state_ack_handler
  mvfst_state_machine
)

target_link_libraries(
  z_quic PUBLIC
  Folly::folly
  mvfst_constants
  mvfst_exception
  mvfst_state_ack_handler
  mvfst_state_machine
  mvfst_test_utils
  ${GFLAGS_LIBRARIES}
  ${LIBGMOCK_LIBRARIES}
  PRIVATE
  ${BOOST_LIBRARIES}
)

file(
  GLOB_RECURSE QUIC_API_HEADERS_TOINSTALL
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  *.h
)
list(FILTER QUIC_API_HEADERS_TOINSTALL EXCLUDE REGEX test/)
foreach(header ${QUIC_API_HEADERS_TOINSTALL})
  get_filename_component(header_dir ${header} DIRECTORY)
  install(FILES ${header} DESTINATION include/quic/observer/${header_dir})
endforeach()

install(
  TARGETS z_quic
  DESTINATION lib
)
