#pragma once

#include <showcase/core/Log.h>

#include <cassert>

#ifdef NDEBUG
    #define SE_ASSERT(expr) ((void)0)
#else
    #define SE_ASSERT(expr) \
        do { \
            if (!(expr)) { \
                SE_LOG_CRITICAL("Assertion failed: {} ({}:{})", #expr, __FILE__, __LINE__); \
                assert(false); \
            } \
        } while (0)
#endif
