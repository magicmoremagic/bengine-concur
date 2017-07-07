#pragma once
#ifndef BE_ATEX_VERSION_HPP_
#define BE_ATEX_VERSION_HPP_

#include <be/core/macros.hpp>

#define BE_ATEX_VERSION_MAJOR 0
#define BE_ATEX_VERSION_MINOR 1
#define BE_ATEX_VERSION_REV 0

/*!! include('common/version', 'BE_ATEX', 'atex') !! 6 */
/* ################# !! GENERATED CODE -- DO NOT MODIFY !! ################# */
#define BE_ATEX_VERSION (BE_ATEX_VERSION_MAJOR * 100000 + BE_ATEX_VERSION_MINOR * 1000 + BE_ATEX_VERSION_REV)
#define BE_ATEX_VERSION_STRING "atex " BE_STRINGIFY(BE_ATEX_VERSION_MAJOR) "." BE_STRINGIFY(BE_ATEX_VERSION_MINOR) "." BE_STRINGIFY(BE_ATEX_VERSION_REV)

/* ######################### END OF GENERATED CODE ######################### */

#endif
