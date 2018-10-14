/**
 * Copyright (C) 2017-2018, Decawave Limited, All Rights Reserved
 * 
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


#ifndef _JSON_ENCODE_H_
#define _JSON_ENCODE_H_

#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

#include "json/json.h"
#include <dw1000/dw1000_ftypes.h>
#include <dw1000/dw1000_rng.h>


#define CIR_SIZE (64)

typedef union {
    struct  _cir_complex_t{
        int16_t real;           
        int16_t imag;             
    }__attribute__((__packed__));
    uint8_t array[sizeof(struct _cir_complex_t)];
}cir_complex_t;

typedef struct _cir_t{
    uint8_t dummy;
    cir_complex_t array[CIR_SIZE];
    uint16_t fp_idx;
    uint16_t fp_amp1;
}cir_t;

int json_ftype_encode(twr_frame_t * frame);
void json_rng_encode(twr_frame_t frames[], uint16_t nsize);
void json_cir_encode(cir_t * cir, char * name, uint16_t nsize);
void json_rxdiag_encode(dw1000_dev_rxdiag_t * rxdiag, char * name);

#endif



