/**
 Copyright 2017 Paul Kettle
 
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
 
 http://www.apache.org/licenses/LICENSE-2.0
 
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */




#include "mex.h"

extern "C" {
    #include "slots.h"
}


/* The gateway routine. */
void mexFunction( int nlhs, mxArray *plhs[],
                 int nrhs, const mxArray *prhs[] )
{
    
    uint16_t n,m;

    /* check for the proper number of arguments */
    if(nrhs != 2)
        mexErrMsgTxt("Usage:[idx] = SlotIndex_mex(masks, bitfield);\n");
    if(nlhs > 1)
        mexErrMsgTxt("Too many output arguments.");
    
    /* get the length input vector */
    n = mxGetN(prhs[0]);
    m = mxGetM(prhs[0]);
    
    /* get pointers to the real and imaginary parts of the inputs */
    uint32_t * mask = ( uint32_t *) mxGetPr(prhs[0]);
    uint32_t bitfield = ( uint32_t) mxGetScalar(prhs[1]);
    
    
    /* create a new arrays and set the output pointer to it */
    
    plhs[0] = mxCreateNumericMatrix(n, m, mxSINGLE_CLASS, mxREAL);
    float * idx = (float *) mxGetPr(plhs[0]);

//    for (uint16_t i = 0; i < n; i++)
//        for (uint16_t j = 0; j < m; j++)
//            mexPrintf("mask = %d, bitfield=%d %d\n", mask[i + j * n], bitfield, mask[i + j * n] & bitfield);

    for (uint16_t i = 0; i < n; i++)
        for (uint16_t j = 0; j < m; j++){
            if (mask[i + j * n] & bitfield)
                idx[i + j * n] = BitIndex(mask[i + j * n], bitfield, SLOT_POSITION) + 1;
            else 
                idx[i + j * n] = 0.0/0.0;
        }
    
    return;
}


