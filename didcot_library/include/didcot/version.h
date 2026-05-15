/*
 * didcot -- Library flavor and version identification macros.
 *
 * Provides DIDCOT_VERSION string constants that identify the library build at runtime.
 *
 * This file is processed by tooling in the automated build system.
 * It's important to maintain the structure and formatting of the version macros for compatibility with version parsing scripts.
 *
 * cdox v1.0.2.1
 */

#pragma once

// @dtack-version-file DIDCOT

#define DIDCOT_VERSION_MAJOR 1
#define DIDCOT_VERSION_MINOR 0
#define DIDCOT_VERSION_PATCH 0

#define DIDCOT_VERSION_STR_(x) #x
#define DIDCOT_VERSION_STR(x) DIDCOT_VERSION_STR_(x)
#define DIDCOT_VERSION                                                                                                         \
    DIDCOT_VERSION_STR(DIDCOT_VERSION_MAJOR)                                                                                   \
    "." DIDCOT_VERSION_STR(DIDCOT_VERSION_MINOR) "." DIDCOT_VERSION_STR(DIDCOT_VERSION_PATCH)
