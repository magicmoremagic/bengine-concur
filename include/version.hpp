#pragma once
#ifndef BE_CONCUR_VERSION_HPP_
#define BE_CONCUR_VERSION_HPP_

#include <be/core/macros.hpp>

#define BE_CONCUR_VERSION_MAJOR 0
#define BE_CONCUR_VERSION_MINOR 1
#define BE_CONCUR_VERSION_REV 5

/*!! include('common/version', 'BE_CONCUR', 'ConCur') !! 6 */
/* ################# !! GENERATED CODE -- DO NOT MODIFY !! ################# */
#define BE_CONCUR_VERSION (BE_CONCUR_VERSION_MAJOR * 100000 + BE_CONCUR_VERSION_MINOR * 1000 + BE_CONCUR_VERSION_REV)
#define BE_CONCUR_VERSION_STRING "ConCur " BE_STRINGIFY(BE_CONCUR_VERSION_MAJOR) "." BE_STRINGIFY(BE_CONCUR_VERSION_MINOR) "." BE_STRINGIFY(BE_CONCUR_VERSION_REV)

/* ######################### END OF GENERATED CODE ######################### */

#endif
