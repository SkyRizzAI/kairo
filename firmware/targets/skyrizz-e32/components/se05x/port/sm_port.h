/** @file sm_port.h
 *  @brief Platform-specific glue for ESP32 (Palanu / skyrizz-e32).
 *
 * ESP32 port of the NXP Plug & Trust nano-package platform layer. Logging goes to
 * printf (IDF console), allocation to malloc/free, and the APDU mutex is a no-op
 * (PLUGANDTRUST_ENABLE_SM_APDU_MUTEX is OFF — single wallet task drives the SE).
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SM_PORT_H_INC
#define SM_PORT_H_INC

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#define SMLOG_I(...) printf(__VA_ARGS__)
#define SMLOG_E(...) printf(__VA_ARGS__)
#define SMLOG_W(...) printf(__VA_ARGS__)

#ifdef SMLOG_DEBUG_MESSAGES
#define SMLOG_D(...)                                       \
    do { printf(__VA_ARGS__); printf("\n"); } while (0)
#define SMLOG_AU8_D(BUF, LEN)                              \
    do { for (size_t i_ = 0; i_ < (LEN); i_++) printf("%02x ", (BUF)[i_]); printf("\n"); } while (0)
#define SMLOG_MAU8_D(MSG, BUF, LEN)                        \
    do { printf("%s", MSG); for (size_t i_ = 0; i_ < (LEN); i_++) printf("%02x ", (BUF)[i_]); printf("\n"); } while (0)
#else
#define SMLOG_D(...)
#define SMLOG_AU8_D(BUF, LEN)
#define SMLOG_MAU8_D(MSG, BUF, LEN)
#endif

#define sm_malloc malloc
#define sm_free free

/* Single-threaded use (mutex disabled): no-op the APDU-layer mutex macros. */
#define SM_MUTEX_DEFINE(x)
#define SM_MUTEX_EXTERN_DEFINE(x)
#define SM_MUTEX_INIT(x)
#define SM_MUTEX_DEINIT(x)
#define SM_MUTEX_LOCK(x)
#define SM_MUTEX_UNLOCK(x)

#ifndef FALSE
#define FALSE false
#endif
#ifndef TRUE
#define TRUE true
#endif

#endif /* SM_PORT_H_INC */
