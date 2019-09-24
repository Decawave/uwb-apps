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

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <dw1000/dw1000_ftypes.h>
#include "json_encode.h"


#define JSON_BUF_SIZE (1024*8)
static char _buf[JSON_BUF_SIZE];
static uint16_t idx=0;

static void
json_fflush(){
    _buf[idx] = '\0';
    printf("%s\n", _buf);
    idx=0;
}

static void
_json_fflush(){
    _buf[idx] = '\0';
    printf("%s", _buf);
    idx=0;
}

static int
json_write(void *buf, char* data, int len) {
    // write(STDOUT_FILENO, data, len);  TODOs: This is the prefered approach

    if (idx + len > JSON_BUF_SIZE) 
        _json_fflush();

    for (uint16_t i=0; i< len; i++)
        _buf[i+idx] = data[i];
    idx+=len;

    return len;
}

int json_ftype_encode(twr_frame_t * frame){

    struct json_encoder encoder;
    struct json_value value;
    int rc;

    /* reset the state of the internal test */
    memset(&encoder, 0, sizeof(encoder));
    encoder.je_write = json_write;
    encoder.je_arg = NULL;

    rc = json_encode_object_start(&encoder);
    JSON_VALUE_UINT(&value, frame->fctrl);
    rc |= json_encode_object_entry(&encoder, "fctrl", &value);
    JSON_VALUE_UINT(&value, frame->seq_num);
    rc |= json_encode_object_entry(&encoder, "seq_num", &value);
    JSON_VALUE_UINT(&value, frame->PANID);
    rc |= json_encode_object_entry(&encoder, "PANID", &value);
    JSON_VALUE_UINT(&value, frame->dst_address);
    rc |= json_encode_object_entry(&encoder, "dst_address", &value);
    JSON_VALUE_UINT(&value, frame->src_address);
    rc |= json_encode_object_entry(&encoder, "src_address", &value);
    JSON_VALUE_UINT(&value, frame->code);
    rc |= json_encode_object_entry(&encoder, "code", &value);
    JSON_VALUE_UINT(&value, frame->reception_timestamp);
    rc |= json_encode_object_entry(&encoder, "reception_timestamp", &value);
    JSON_VALUE_UINT(&value, frame->transmission_timestamp);
    rc |= json_encode_object_entry(&encoder, "transmission_timestamp", &value);
    JSON_VALUE_UINT(&value, frame->request_timestamp);
    rc |= json_encode_object_entry(&encoder, "request_timestamp", &value);
    JSON_VALUE_UINT(&value, frame->response_timestamp);
    rc |= json_encode_object_entry(&encoder, "response_timestamp", &value);

    rc |= json_encode_object_finish(&encoder);
    assert(rc == 0);
     _json_fflush();

    return rc;
}


void json_rng_encode(twr_frame_t frames[], uint16_t len){

    struct json_encoder encoder;
    struct json_value value;
    int rc;

    /* reset the state of the internal test */
    memset(&encoder, 0, sizeof(encoder));
    encoder.je_write = json_write;
    encoder.je_arg= NULL;

    rc = json_encode_object_start(&encoder);
    JSON_VALUE_INT(&value, os_cputime_ticks_to_usecs(os_cputime_get32()));
    rc |= json_encode_object_entry(&encoder, "utime", &value);

    rc |= json_encode_array_name(&encoder, "twr");
    rc |= json_encode_array_start(&encoder);

    for (uint16_t i=0; i< len; i++){
        rc |= json_ftype_encode(&frames[i]);
        if (i < len-1)
            encoder.je_write(encoder.je_arg, ",", sizeof(",")-1);
    }
    rc |= json_encode_array_finish(&encoder);    
    rc |= json_encode_object_finish(&encoder);

    assert(rc == 0);
    json_fflush();
}

void json_cir_encode(cir_t * cir, char * name, uint16_t nsize){

    struct json_encoder encoder;
    struct json_value value;
    int rc;

    /* reset the state of the internal test */
    memset(&encoder, 0, sizeof(encoder));
    encoder.je_write = json_write;
    encoder.je_arg= NULL;

    rc = json_encode_object_start(&encoder);    
    JSON_VALUE_INT(&value, os_cputime_ticks_to_usecs(os_cputime_get32()));
    rc |= json_encode_object_entry(&encoder, "utime", &value);
    
    rc |= json_encode_object_key(&encoder, name);
    rc |= json_encode_object_start(&encoder);    

    JSON_VALUE_INT(&value, cir->fp_idx);
    rc |= json_encode_object_entry(&encoder, "fp_idx", &value);

    rc |= json_encode_array_name(&encoder, "real");
    rc |= json_encode_array_start(&encoder);
    for (uint16_t i=0; i< nsize; i++){
        JSON_VALUE_INT(&value, cir->array[i].real);
        rc |= json_encode_array_value(&encoder, &value); 
        if (i%32==0) _json_fflush();
    }
    rc |= json_encode_array_finish(&encoder);  


    rc |= json_encode_array_name(&encoder, "imag");
    rc |= json_encode_array_start(&encoder);
    for (uint16_t i=0; i< nsize; i++){
        JSON_VALUE_INT(&value, cir->array[i].imag);
        rc |= json_encode_array_value(&encoder, &value);
        if (i%32==0) _json_fflush();
    }
    rc |= json_encode_array_finish(&encoder);    
    rc |= json_encode_object_finish(&encoder);
    rc |= json_encode_object_finish(&encoder);
    assert(rc == 0);
    json_fflush();
}


void json_rxdiag_encode(dw1000_dev_rxdiag_t * rxdiag, char * name){

    struct json_encoder encoder;
    struct json_value value;
    int rc;

    /* reset the state of the internal test */
    memset(&encoder, 0, sizeof(encoder));
    encoder.je_write = json_write;
    encoder.je_arg= NULL;

    rc = json_encode_object_start(&encoder);
    JSON_VALUE_INT(&value, os_cputime_ticks_to_usecs(os_cputime_get32()));
    rc |= json_encode_object_entry(&encoder, "utime", &value);

    rc |= json_encode_object_key(&encoder, name);
    rc |= json_encode_object_start(&encoder);    

    JSON_VALUE_UINT(&value, rxdiag->fp_idx);
    rc |= json_encode_object_entry(&encoder, "fp_idx", &value);
    JSON_VALUE_UINT(&value, rxdiag->fp_amp);
    rc |= json_encode_object_entry(&encoder, "fp_amp", &value);
    JSON_VALUE_UINT(&value, rxdiag->rx_std);
    rc |= json_encode_object_entry(&encoder, "rx_std", &value);
    JSON_VALUE_UINT(&value, rxdiag->pacc_cnt);
    rc |= json_encode_object_entry(&encoder, "pacc_cnt", &value);

    rc |= json_encode_object_finish(&encoder);
    rc |= json_encode_object_finish(&encoder);
    assert(rc == 0);
    json_fflush();
}




