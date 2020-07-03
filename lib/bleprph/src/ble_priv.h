/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef H_BLEPRPH_PRIV_
#define H_BLEPRPH_PRIV_

#include <stdbool.h>
#include "log/log.h"
#include "nimble/ble.h"
#ifdef __cplusplus
extern "C" {
#endif

struct ble_hs_cfg;
struct ble_gatt_register_ctxt;

extern struct log bleprph_log;

/* bleprph uses the first "peruser" log module. */
#define BLEPRPH_LOG_MODULE  (LOG_MODULE_PERUSER + 0)

/* Convenience macro for logging to the bleprph module. */
#if MYNEWT_VAL(BLEPRPH_LOG_ENABLE)
  #define BLEPRPH_LOG(lvl, ...)                                   \
    LOG_ ## lvl(&bleprph_log, BLEPRPH_LOG_MODULE, __VA_ARGS__)
#else
  #define BLEPRPH_LOG(lvl, ...) {}
#endif

#ifdef __cplusplus
}
#endif

#endif
