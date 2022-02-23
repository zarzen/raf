# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0

if (${MNM_USE_GTEST} STREQUAL "ON")
  include(CTest)
  include(${PROJECT_SOURCE_DIR}/cmake/utils/MNMTest.cmake)
  add_subdirectory(${PROJECT_SOURCE_DIR}/3rdparty/googletest/ EXCLUDE_FROM_ALL)
elseif (${MNM_USE_GTEST} STREQUAL "OFF")
  message(STATUS "Build without googletest")
else()
  message(FATAL_ERROR "Cannot recognize MNM_USE_GTEST = ${MNM_USE_GTEST}")
endif()
