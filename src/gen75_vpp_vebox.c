/*
 * Copyright � 2011 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *   Li Xiaowei <xiaowei.a.li@intel.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "intel_batchbuffer.h"
#include "intel_driver.h"
#include "i965_defines.h"
#include "i965_structs.h"
#include "gen75_vpp_vebox.h"
#include "intel_media.h"

#define PI  3.1415926

extern VAStatus
i965_MapBuffer(VADriverContextP ctx, VABufferID buf_id, void **);

extern VAStatus
i965_UnmapBuffer(VADriverContextP ctx, VABufferID buf_id);

extern VAStatus
i965_DeriveImage(VADriverContextP ctx, VABufferID surface, VAImage *out_image);

extern VAStatus
i965_DestroyImage(VADriverContextP ctx, VAImageID image);


VAStatus vpp_surface_copy(VADriverContextP ctx, VASurfaceID dstSurfaceID, VASurfaceID srcSurfaceID)
{
    VAStatus va_status = VA_STATUS_SUCCESS;
    VAImage srcImage, dstImage;
    void *pBufferSrc, *pBufferDst;
    unsigned char *ySrc, *yDst;

    va_status = vpp_surface_convert(ctx, dstSurfaceID, srcSurfaceID);
    if(va_status == VA_STATUS_SUCCESS){
       return va_status;
    }

    va_status = i965_DeriveImage(ctx, srcSurfaceID, &srcImage);
    assert(va_status == VA_STATUS_SUCCESS);

    va_status = i965_DeriveImage(ctx, dstSurfaceID, &dstImage);
    assert(va_status == VA_STATUS_SUCCESS);

    if(srcImage.width  != dstImage.width  ||
       srcImage.height != dstImage.height ||
       srcImage.format.fourcc != dstImage.format.fourcc) {
        return VA_STATUS_ERROR_UNIMPLEMENTED;
    }

    va_status = i965_MapBuffer(ctx, srcImage.buf, &pBufferSrc);
    assert(va_status == VA_STATUS_SUCCESS);

    va_status = i965_MapBuffer(ctx, dstImage.buf, &pBufferDst);
    assert(va_status == VA_STATUS_SUCCESS);

    ySrc = (unsigned char*)(pBufferSrc + srcImage.offsets[0]);
    yDst = (unsigned char*)(pBufferDst + dstImage.offsets[0]);
  
    memcpy(pBufferDst, pBufferSrc, dstImage.data_size);

    i965_UnmapBuffer(ctx, srcImage.buf);
    i965_UnmapBuffer(ctx, dstImage.buf);
    i965_DestroyImage(ctx, srcImage.image_id);
    i965_DestroyImage(ctx, dstImage.image_id);

    return va_status;;
}

VAStatus vpp_surface_convert(VADriverContextP ctx, VASurfaceID dstSurfaceID, VASurfaceID srcSurfaceID)
{
    VAStatus va_status = VA_STATUS_SUCCESS;
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct object_surface* src_obj_surf = SURFACE(srcSurfaceID);
    struct object_surface* dst_obj_surf = SURFACE(dstSurfaceID);

    assert(src_obj_surf->orig_width  == dst_obj_surf->orig_width);
    assert(src_obj_surf->orig_height == dst_obj_surf->orig_height);

    VARectangle src_rect, dst_rect;
    src_rect.x = dst_rect.x = 0;
    src_rect.y = dst_rect.y = 0; 
    src_rect.width  = dst_rect.width  = src_obj_surf->orig_width; 
    src_rect.height = dst_rect.height = dst_obj_surf->orig_height;

    struct i965_surface src_surface, dst_surface;
    src_surface.id    = srcSurfaceID;
    src_surface.type  = I965_SURFACE_TYPE_SURFACE;
    src_surface.flags = I965_SURFACE_FLAG_FRAME;

    dst_surface.id    = dstSurfaceID;
    dst_surface.type  = I965_SURFACE_TYPE_SURFACE;
    dst_surface.flags = I965_SURFACE_FLAG_FRAME;

    va_status = i965_image_processing(ctx,
                                     &src_surface,
                                     &src_rect,
                                     &dst_surface,
                                     &dst_rect);
    return va_status;
}

VAStatus vpp_surface_scaling(VADriverContextP ctx, VASurfaceID dstSurfaceID, VASurfaceID srcSurfaceID)
{
    VAStatus va_status = VA_STATUS_SUCCESS;
    int flags = I965_PP_FLAG_AVS;
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct object_surface* src_obj_surf = SURFACE(srcSurfaceID);
    struct object_surface* dst_obj_surf = SURFACE(dstSurfaceID);

    assert(src_obj_surf->fourcc == VA_FOURCC('N','V','1','2'));
    assert(dst_obj_surf->fourcc == VA_FOURCC('N','V','1','2'));

    VARectangle src_rect, dst_rect;
    src_rect.x = 0;
    src_rect.y = 0; 
    src_rect.width  = src_obj_surf->orig_width; 
    src_rect.height = src_obj_surf->orig_height;

    dst_rect.x = 0;
    dst_rect.y = 0; 
    dst_rect.width  = dst_obj_surf->orig_width; 
    dst_rect.height = dst_obj_surf->orig_height;

    va_status = i965_scaling_processing(ctx,
                                       srcSurfaceID,
                                       &src_rect,
                                       dstSurfaceID,
                                       &dst_rect,
                                       flags);
     
    return va_status;
}

void hsw_veb_dndi_table(VADriverContextP ctx, struct intel_vebox_context *proc_ctx)
{
    unsigned int* p_table ;
    /*
    VAProcFilterParameterBufferDeinterlacing *di_param =
            (VAProcFilterParameterBufferDeinterlacing *) proc_ctx->filter_di;

    VAProcFilterParameterBuffer * dn_param =
            (VAProcFilterParameterBuffer *) proc_ctx->filter_dn;
    */
    p_table = (unsigned int *)proc_ctx->dndi_state_table.ptr;

    *p_table ++ = 0;               // reserved  . w0
    *p_table ++ = ( 0   << 24 |    // denoise STAD threshold . w1
                    128 << 16 |    // dnmh_history_max
                    0   << 12 |    // reserved
                    8   << 8  |    // dnmh_delta[3:0]
                    0 );           // denoise ASD threshold

    *p_table ++ = ( 0  << 30 |    // reserved . w2
                    16 << 24 |    // temporal diff th
                    0  << 22 |    // reserved.
                    8  << 16 |    // low temporal diff th
                    0  << 13 |    // STMM C2
                    0  << 8  |    // denoise moving pixel th
                    64 );         // denoise th for sum of complexity measure

    *p_table ++ = ( 0 << 30  |   // reserved . w3
                    4 << 24  |   // good neighbor th[5:0]
                    9 << 20  |   // CAT slope minus 1
                    5 << 16  |   // SAD Tight in
                    0 << 14  |   // smooth mv th
                    0 << 12  |   // reserved
                    1 << 8   |   // bne_edge_th[3:0]
                    15 );        // block noise estimate noise th

    *p_table ++ = ( 0  << 31  |  // STMM blending constant select. w4
                    64 << 24  |  // STMM trc1
                    0  << 16  |  // STMM trc2
                    0  << 14  |  // reserved
                    2  << 8   |  // VECM_mul
                    128 );       // maximum STMM

    *p_table ++ = ( 0  << 24  |  // minumum STMM  . W5
                    0  << 22  |  // STMM shift down
                    0  << 20  |  // STMM shift up
                    7  << 16  |  // STMM output shift
                    128 << 8  |  // SDI threshold
                    8 );         // SDI delta

    *p_table ++ = ( 0 << 24  |   // SDI fallback mode 1 T1 constant . W6
                    0 << 16  |   // SDI fallback mode 1 T2 constant
                    0 << 8   |   // SDI fallback mode 2 constant(angle2x1)
                    0 );         // FMD temporal difference threshold

    *p_table ++ = ( 32 << 24  |  // FMD #1 vertical difference th . w7
                    32 << 16  |  // FMD #2 vertical difference th
                    1  << 14  |  // CAT th1
                    32 << 8   |  // FMD tear threshold
                    0  << 7   |  // MCDI Enable, use motion compensated deinterlace algorithm
                    0  << 6   |  // progressive DN
                    0  << 4   |  // reserved
                    0  << 3   |  // DN/DI Top First
                    0 );         // reserved

    *p_table ++ = ( 0  << 29  |  // reserved . W8
                    0  << 23  |  // dnmh_history_init[5:0]
                    10 << 19  |  // neighborPixel th
                    0  << 18  |  // reserved
                    0  << 16  |  // FMD for 2nd field of previous frame
                    25 << 10  |  // MC pixel consistency th
                    0  << 8   |  // FMD for 1st field for current frame
                    10 << 4   |  // SAD THB
                    5 );         // SAD THA

    *p_table ++ = ( 0 << 24  |  // reserved
                    0 << 16  |  // chr_dnmh_stad_th
                    0 << 13  |  // reserved
                    0 << 12  |  // chrome denoise enable
                    0 << 6   |  // chr temp diff th
                    0 );        // chr temp diff low

}

void hsw_veb_iecp_std_table(VADriverContextP ctx, struct intel_vebox_context *proc_ctx)
{
    unsigned int *p_table = proc_ctx->iecp_state_table.ptr + 0 ;
    //VAProcFilterParameterBuffer * std_param =
    //        (VAProcFilterParameterBuffer *) proc_ctx->filter_std;

    if(!(proc_ctx->filters_mask & VPP_IECP_STD_STE)){ 
        memset(p_table, 0, 29 * 4);
    }else{
        *p_table ++ = 0x9a6e39f0;
        *p_table ++ = 0x400c0000;
        *p_table ++ = 0x00001180;
        *p_table ++ = 0xfe2f2e00;
        *p_table ++ = 0x000000ff;

        *p_table ++ = 0x00140000;
        *p_table ++ = 0xd82e0000;
        *p_table ++ = 0x8285ecec;
        *p_table ++ = 0x00008282;
        *p_table ++ = 0x00000000;

        *p_table ++ = 0x02117000;
        *p_table ++ = 0xa38fec96;
        *p_table ++ = 0x0000c8c8;
        *p_table ++ = 0x00000000;
        *p_table ++ = 0x01478000;
 
        *p_table ++ = 0x0007c306;
        *p_table ++ = 0x00000000;
        *p_table ++ = 0x00000000;
        *p_table ++ = 0x1c1bd000;
        *p_table ++ = 0x00000000;

        *p_table ++ = 0x00000000;
        *p_table ++ = 0x00000000;
        *p_table ++ = 0x0007cf80;
        *p_table ++ = 0x00000000;
        *p_table ++ = 0x00000000;

        *p_table ++ = 0x1c080000;
        *p_table ++ = 0x00000000;
        *p_table ++ = 0x00000000;
        *p_table ++ = 0x00000000;
    }
}

void hsw_veb_iecp_ace_table(VADriverContextP ctx, struct intel_vebox_context *proc_ctx)
{
   unsigned int *p_table = (unsigned int*)(proc_ctx->iecp_state_table.ptr + 116);

    if(!(proc_ctx->filters_mask & VPP_IECP_ACE)){ 
        memset(p_table, 0, 13 * 4);
    }else{
        *p_table ++ = 0x00000068;
        *p_table ++ = 0x4c382410;
        *p_table ++ = 0x9c887460;
        *p_table ++ = 0xebd8c4b0;
        *p_table ++ = 0x604c3824;

        *p_table ++ = 0xb09c8874;
        *p_table ++ = 0x0000d8c4;
        *p_table ++ = 0x00000000;
        *p_table ++ = 0x00000000;
        *p_table ++ = 0x00000000;

        *p_table ++ = 0x00000000;
        *p_table ++ = 0x00000000;
        *p_table ++ = 0x00000000;
   }
}

void hsw_veb_iecp_tcc_table(VADriverContextP ctx, struct intel_vebox_context *proc_ctx)
{
    unsigned int *p_table = (unsigned int*)(proc_ctx->iecp_state_table.ptr + 168);

//    VAProcFilterParameterBuffer * tcc_param =
//            (VAProcFilterParameterBuffer *) proc_ctx->filter_iecp_tcc;

   if(!(proc_ctx->filters_mask & VPP_IECP_TCC)){ 
        memset(p_table, 0, 11 * 4);
    }else{
        *p_table ++ = 0x00000000;
        *p_table ++ = 0x00000000;
        *p_table ++ = 0x1e34cc91;
        *p_table ++ = 0x3e3cce91;
        *p_table ++ = 0x02e80195;

        *p_table ++ = 0x0197046b;
        *p_table ++ = 0x01790174;
        *p_table ++ = 0x00000000;
        *p_table ++ = 0x00000000;
        *p_table ++ = 0x03030000;

        *p_table ++ = 0x009201c0;
   }
}

void hsw_veb_iecp_pro_amp_table(VADriverContextP ctx, struct intel_vebox_context *proc_ctx)
{
    unsigned int contrast = 0x80;  //default 
    int brightness = 0x00;         //default
    int cos_c_s    = 256 ;         //default
    int sin_c_s    = 0;            //default 
    unsigned int *p_table = (unsigned int*)(proc_ctx->iecp_state_table.ptr + 212);

    if(!(proc_ctx->filters_mask & VPP_IECP_PRO_AMP)){
        memset(p_table, 0, 2 * 4);
    }else {
        float  src_saturation = 1.0;
        float  src_hue = 0.0;
        float  src_contrast = 1.0;
        float  src_brightness = 0.0;
        float  tmp_value = 0.0;

        VAProcFilterParameterBufferColorBalance * amp_param =
        (VAProcFilterParameterBufferColorBalance *) proc_ctx->filter_iecp_amp;
        VAProcColorBalanceType attrib = amp_param->attrib;

        if(attrib == VAProcColorBalanceHue) {
           src_hue = amp_param->value;         //(-180.0, 180.0)
        }else if(attrib == VAProcColorBalanceSaturation) {
           src_saturation = amp_param->value; //(0.0, 10.0)
        }else if(attrib == VAProcColorBalanceBrightness) {
           src_brightness = amp_param->value; // (-100.0, 100.0)
           brightness = intel_format_convert(src_brightness, 7, 4, 1);
        }else if(attrib == VAProcColorBalanceContrast) {
           src_contrast = amp_param->value;  //  (0.0, 10.0)
           contrast = intel_format_convert(src_contrast, 4, 7, 0);
        }

        tmp_value = cos(src_hue/180*PI) * src_contrast * src_saturation;
        cos_c_s = intel_format_convert(tmp_value, 7, 8, 1);
        
        tmp_value = sin(src_hue/180*PI) * src_contrast * src_saturation;
        sin_c_s = intel_format_convert(tmp_value, 7, 8, 1);
     
        *p_table ++ = ( 0 << 28 |         //reserved
                        contrast << 17 |  //contrast value (U4.7 format)
                        0 << 13 |         //reserved
                        brightness << 1|  // S7.4 format
                        1);

        *p_table ++ = ( cos_c_s << 16 |  // cos(h) * contrast * saturation
                        sin_c_s);        // sin(h) * contrast * saturation
                 
    }
}


void hsw_veb_iecp_csc_table(VADriverContextP ctx, struct intel_vebox_context *proc_ctx)
{
    unsigned int *p_table = (unsigned int*)(proc_ctx->iecp_state_table.ptr + 220);
    float tran_coef[9] = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
    float v_coef[3]    = {0.0, 0.0, 0.0};
    float u_coef[3]    = {0.0, 0.0, 0.0};
    int   is_transform_enabled = 0;

    if(!(proc_ctx->filters_mask & VPP_IECP_CSC)){
        memset(p_table, 0, 8 * 4);
        return;
    }

    VAProcColorStandardType   in_color_std  = proc_ctx->pipeline_param->surface_color_standard;
    VAProcColorStandardType   out_color_std = proc_ctx->pipeline_param->output_color_standard;
    assert(in_color_std == out_color_std);  
    
    if(proc_ctx->fourcc_input == VA_FOURCC('R','G','B','A') &&
       (proc_ctx->fourcc_output == VA_FOURCC('N','V','1','2') ||
        proc_ctx->fourcc_output == VA_FOURCC('Y','V','1','2') ||
        proc_ctx->fourcc_output == VA_FOURCC('Y','V','Y','2') ||
        proc_ctx->fourcc_output == VA_FOURCC('A','Y','U','V'))) {

         tran_coef[0] = 0.257;
         tran_coef[1] = 0.504;
         tran_coef[2] = 0.098;
         tran_coef[3] = -0.148;
         tran_coef[4] = -0.291;
         tran_coef[5] = 0.439;
         tran_coef[6] = 0.439;
         tran_coef[7] = -0.368;
         tran_coef[8] = -0.071; 

         u_coef[0] = 16 * 4;
         u_coef[1] = 128 * 4;
         u_coef[2] = 128 * 4;
 
         is_transform_enabled = 1; 
    }else if((proc_ctx->fourcc_input  == VA_FOURCC('N','V','1','2') || 
              proc_ctx->fourcc_input  == VA_FOURCC('Y','V','1','2') || 
              proc_ctx->fourcc_input  == VA_FOURCC('Y','U','Y','2') ||
              proc_ctx->fourcc_input  == VA_FOURCC('A','Y','U','V'))&&
              proc_ctx->fourcc_output == VA_FOURCC('R','G','B','A')) {

         tran_coef[0] = 1.164;
         tran_coef[1] = 0.000;
         tran_coef[2] = 1.569;
         tran_coef[3] = 1.164;
         tran_coef[4] = -0.813;
         tran_coef[5] = -0.392;
         tran_coef[6] = 1.164;
         tran_coef[7] = 2.017;
         tran_coef[8] = 0.000; 

         v_coef[0] = -16 * 4;
         v_coef[1] = -128 * 4;
         v_coef[2] = -128 * 4;

        is_transform_enabled = 1; 
    }else if(proc_ctx->fourcc_input != proc_ctx->fourcc_output){
         //enable when input and output format are different.
         is_transform_enabled = 1;
    }

    if(is_transform_enabled == 0){
        memset(p_table, 0, 8 * 4);
    }else{
        *p_table ++ = ( 0 << 29 | //reserved
                        intel_format_convert(tran_coef[1], 2, 10, 1) << 16 | //c1, s2.10 format
                        intel_format_convert(tran_coef[0], 2, 10, 1) << 3 |  //c0, s2.10 format
                        0 << 2 | //reserved
                        0 << 1 | // yuv_channel swap
                        is_transform_enabled);                

        *p_table ++ = ( 0 << 26 | //reserved
                        intel_format_convert(tran_coef[3], 2, 10, 1) << 13 | 
                        intel_format_convert(tran_coef[2], 2, 10, 1));
    
        *p_table ++ = ( 0 << 26 | //reserved
                        intel_format_convert(tran_coef[5], 2, 10, 1) << 13 | 
                        intel_format_convert(tran_coef[4], 2, 10, 1));

        *p_table ++ = ( 0 << 26 | //reserved
                        intel_format_convert(tran_coef[7], 2, 10, 1) << 13 | 
                        intel_format_convert(tran_coef[6], 2, 10, 1));

        *p_table ++ = ( 0 << 13 | //reserved
                        intel_format_convert(tran_coef[8], 2, 10, 1));

        *p_table ++ = ( 0 << 22 | //reserved
                        intel_format_convert(u_coef[0], 10, 0, 1) << 11 | 
                        intel_format_convert(v_coef[0], 10, 0, 1));

        *p_table ++ = ( 0 << 22 | //reserved
                        intel_format_convert(u_coef[1], 10, 0, 1) << 11 | 
                        intel_format_convert(v_coef[1], 10, 0, 1));

        *p_table ++ = ( 0 << 22 | //reserved
                        intel_format_convert(u_coef[2], 10, 0, 1) << 11 | 
                        intel_format_convert(v_coef[2], 10, 0, 1));
    }
}

void hsw_veb_iecp_aoi_table(VADriverContextP ctx, struct intel_vebox_context *proc_ctx)
{
    unsigned int *p_table = (unsigned int*)(proc_ctx->iecp_state_table.ptr + 252);
   // VAProcFilterParameterBuffer * tcc_param =
   //         (VAProcFilterParameterBuffer *) proc_ctx->filter_iecp_tcc;

    if(!(proc_ctx->filters_mask & VPP_IECP_AOI)){ 
        memset(p_table, 0, 3 * 4);
    }else{
        *p_table ++ = 0x00000000;
        *p_table ++ = 0x00030000;
        *p_table ++ = 0x00030000;
   }
}

void hsw_veb_state_table_setup(VADriverContextP ctx, struct intel_vebox_context *proc_ctx)
{
    if(proc_ctx->filters_mask & 0x000000ff) {
        dri_bo *dndi_bo = proc_ctx->dndi_state_table.bo;
        dri_bo_map(dndi_bo, 1);
        proc_ctx->dndi_state_table.ptr = dndi_bo->virtual;

        hsw_veb_dndi_table(ctx, proc_ctx);

        dri_bo_unmap(dndi_bo);
    }

    if(proc_ctx->filters_mask & 0x0000ff00 ||
       proc_ctx->fourcc_input != proc_ctx->fourcc_output) {
        dri_bo *iecp_bo = proc_ctx->iecp_state_table.bo;
        dri_bo_map(iecp_bo, 1);
        proc_ctx->iecp_state_table.ptr = iecp_bo->virtual;

        hsw_veb_iecp_std_table(ctx, proc_ctx);
        hsw_veb_iecp_ace_table(ctx, proc_ctx);
        hsw_veb_iecp_tcc_table(ctx, proc_ctx);
        hsw_veb_iecp_pro_amp_table(ctx, proc_ctx);
        hsw_veb_iecp_csc_table(ctx, proc_ctx);
        hsw_veb_iecp_aoi_table(ctx, proc_ctx);
   
        dri_bo_unmap(iecp_bo);
    }
}

void hsw_veb_state_command(VADriverContextP ctx, struct intel_vebox_context *proc_ctx)
{
    struct intel_batchbuffer *batch = proc_ctx->batch;
    unsigned int is_dn_enabled   = (proc_ctx->filters_mask & 0x01)? 1: 0;
    unsigned int is_di_enabled   = (proc_ctx->filters_mask & 0x02)? 1: 0;
    unsigned int is_iecp_enabled = (proc_ctx->filters_mask & 0xff00)?1:0;

    if(proc_ctx->fourcc_input != proc_ctx->fourcc_output ||
       (is_dn_enabled == 0 && is_di_enabled == 0)){
       is_iecp_enabled = 1;
    }

    BEGIN_VEB_BATCH(batch, 6);
    OUT_VEB_BATCH(batch, VEB_STATE | (6 - 2));
    OUT_VEB_BATCH(batch,
                  0 << 26 |       // state surface control bits
                  0 << 11 |       // reserved.
                  0 << 10 |       // pipe sync disable
                  2 << 8  |       // DI output frame
                  0 << 7  |       // 444->422 downsample method
                  0 << 6  |       // 422->420 downsample method
                  !!(proc_ctx->is_first_frame && (is_di_enabled || is_dn_enabled)) << 5  |   // DN/DI first frame
                  is_di_enabled   << 4  |             // DI enable
                  is_dn_enabled   << 3  |             // DN enable
                  is_iecp_enabled << 2  |             // global IECP enabled
                  0 << 1  |       // ColorGamutCompressionEnable
                  0 ) ;           // ColorGamutExpansionEnable.

    OUT_RELOC(batch, 
              proc_ctx->dndi_state_table.bo,
              I915_GEM_DOMAIN_INSTRUCTION, 0, 0);

    OUT_RELOC(batch,
              proc_ctx->iecp_state_table.bo, 
              I915_GEM_DOMAIN_INSTRUCTION, 0, 0);

    OUT_RELOC(batch,
              proc_ctx->gamut_state_table.bo, 
              I915_GEM_DOMAIN_INSTRUCTION, 0, 0);

    OUT_RELOC(batch,
              proc_ctx->vertex_state_table.bo, 
              I915_GEM_DOMAIN_INSTRUCTION, 0, 0);

    ADVANCE_VEB_BATCH(batch);
}

void hsw_veb_surface_state(VADriverContextP ctx, struct intel_vebox_context *proc_ctx, unsigned int is_output)
{
    struct  i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = proc_ctx->batch;
    unsigned int u_offset_y = 0, v_offset_y = 0;
    unsigned int is_uv_interleaved = 0, tiling = 0, swizzle = 0;
    unsigned int surface_format = PLANAR_420_8;
    struct object_surface* obj_surf = NULL;
    unsigned int surface_pitch = 0;
    unsigned int half_pitch_chroma = 0;

    if(is_output){   
         obj_surf = SURFACE(proc_ctx->frame_store[FRAME_OUT_CURRENT].surface_id);
    }else {
         obj_surf = SURFACE(proc_ctx->frame_store[FRAME_IN_CURRENT].surface_id);
    }

    assert(obj_surf->fourcc == VA_FOURCC_NV12 ||
           obj_surf->fourcc == VA_FOURCC_YUY2 ||
           obj_surf->fourcc == VA_FOURCC_AYUV ||
           obj_surf->fourcc == VA_FOURCC_RGBA);

    if (obj_surf->fourcc == VA_FOURCC_NV12) {
        surface_format = PLANAR_420_8;
        surface_pitch = obj_surf->width; 
        is_uv_interleaved = 1;
        half_pitch_chroma = 0;
    } else if (obj_surf->fourcc == VA_FOURCC_YUY2) {
        surface_format = YCRCB_NORMAL;
        surface_pitch = obj_surf->width * 2; 
        is_uv_interleaved = 0;
        half_pitch_chroma = 0;
    } else if (obj_surf->fourcc == VA_FOURCC_AYUV) {
        surface_format = PACKED_444A_8;
        surface_pitch = obj_surf->width * 4; 
        is_uv_interleaved = 0;
        half_pitch_chroma = 0;
    } else if (obj_surf->fourcc == VA_FOURCC_RGBA) {
        surface_format = R8G8B8A8_UNORM_SRGB;
        surface_pitch = obj_surf->width * 4; 
        is_uv_interleaved = 0;
        half_pitch_chroma = 0;
    }

    u_offset_y = obj_surf->y_cb_offset;
    v_offset_y = obj_surf->y_cr_offset;
     
    dri_bo_get_tiling(obj_surf->bo, &tiling, &swizzle);

    BEGIN_VEB_BATCH(batch, 6);
    OUT_VEB_BATCH(batch, VEB_SURFACE_STATE | (6 - 2));
    OUT_VEB_BATCH(batch,
                  0 << 1 |         // reserved
                  is_output);      // surface indentification.

    OUT_VEB_BATCH(batch,
                  (proc_ctx->height_input - 1) << 18 |  // height . w3
                  (proc_ctx->width_input) << 4  |       // width
                  0);                                   // reserve

    OUT_VEB_BATCH(batch,
                  surface_format      << 28  |  // surface format, YCbCr420. w4
                  is_uv_interleaved   << 27  |  // interleave chrome , two seperate palar
                  0                   << 20  |  // reserved
                  (surface_pitch - 1) << 3   |  // surface pitch, 64 align
                  half_pitch_chroma   << 2   |  // half pitch for chrome
                  !!tiling            << 1   |  // tiled surface, linear surface used
                  (tiling == I915_TILING_Y));   // tiled walk, ignored when liner surface

    OUT_VEB_BATCH(batch,
                  0 << 29  |     // reserved . w5
                  0 << 16  |     // X offset for V(Cb)
                  0 << 15  |     // reserved
                  u_offset_y);   // Y offset for V(Cb)

    OUT_VEB_BATCH(batch,
                  0 << 29  |     // reserved . w6
                  0 << 16  |     // X offset for V(Cr)
                  0 << 15  |     // reserved
                  v_offset_y );  // Y offset for V(Cr)

    ADVANCE_VEB_BATCH(batch);
}

void hsw_veb_dndi_iecp_command(VADriverContextP ctx, struct intel_vebox_context *proc_ctx)
{
    struct intel_batchbuffer *batch = proc_ctx->batch;
    unsigned char frame_ctrl_bits = 0;
    unsigned int startingX = 0;
    unsigned int endingX = proc_ctx->width_input;
    VEBFrameStore tempFrame;

    /* s1:update the previous and current input */
/*    tempFrame = proc_ctx->frame_store[FRAME_IN_PREVIOUS];
    proc_ctx->frame_store[FRAME_IN_PREVIOUS] = proc_ctx->frame_store[FRAME_IN_CURRENT]; ;
    proc_ctx->frame_store[FRAME_IN_CURRENT] = tempFrame;

    if(proc_ctx->surface_input_vebox != -1){
        vpp_surface_copy(ctx, proc_ctx->frame_store[FRAME_IN_CURRENT].surface_id,
                     proc_ctx->surface_input_vebox);
    } else {
        vpp_surface_copy(ctx, proc_ctx->frame_store[FRAME_IN_CURRENT].surface_id,
                     proc_ctx->surface_input);
    }
*/
    /*s2: update the STMM input and output */
/*    tempFrame = proc_ctx->frame_store[FRAME_IN_STMM];
    proc_ctx->frame_store[FRAME_IN_STMM] = proc_ctx->frame_store[FRAME_OUT_STMM]; ;
    proc_ctx->frame_store[FRAME_OUT_STMM] = tempFrame;
*/	
    /*s3:set reloc buffer address */
    BEGIN_VEB_BATCH(batch, 10);
    OUT_VEB_BATCH(batch, VEB_DNDI_IECP_STATE | (10 - 2));
    OUT_VEB_BATCH(batch,
                  startingX << 16 |
                  endingX);
    OUT_RELOC(batch,
              proc_ctx->frame_store[FRAME_IN_CURRENT].bo,
              I915_GEM_DOMAIN_RENDER, 0, frame_ctrl_bits);
    OUT_RELOC(batch,
              proc_ctx->frame_store[FRAME_IN_PREVIOUS].bo,
              I915_GEM_DOMAIN_RENDER, 0, frame_ctrl_bits);
    OUT_RELOC(batch,
              proc_ctx->frame_store[FRAME_IN_STMM].bo,
              I915_GEM_DOMAIN_RENDER, 0, frame_ctrl_bits);
    OUT_RELOC(batch,
              proc_ctx->frame_store[FRAME_OUT_STMM].bo,
              I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER, frame_ctrl_bits);
    OUT_RELOC(batch,
              proc_ctx->frame_store[FRAME_OUT_CURRENT_DN].bo,
              I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER, frame_ctrl_bits);
    OUT_RELOC(batch,
              proc_ctx->frame_store[FRAME_OUT_CURRENT].bo,
              I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER, frame_ctrl_bits);
    OUT_RELOC(batch,
              proc_ctx->frame_store[FRAME_OUT_PREVIOUS].bo,
              I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER, frame_ctrl_bits);
    OUT_RELOC(batch,
              proc_ctx->frame_store[FRAME_OUT_STATISTIC].bo,
              I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER, frame_ctrl_bits);

    ADVANCE_VEB_BATCH(batch);
}

void hsw_veb_resource_prepare(VADriverContextP ctx,
                              struct intel_vebox_context *proc_ctx)
{
    VAStatus va_status;
    dri_bo *bo;
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    unsigned int input_fourcc, output_fourcc;
    unsigned int input_sampling, output_sampling;
    unsigned int input_tiling, output_tiling;
    VAGenericID vebox_in_id, vebox_out_id;
    unsigned int i, swizzle;

    if(proc_ctx->surface_input_vebox != -1){
       vebox_in_id = proc_ctx->surface_input_vebox;
    }else{
       vebox_in_id = proc_ctx->surface_input;
    } 

    if(proc_ctx->surface_output_vebox != -1){
       vebox_out_id = proc_ctx->surface_output_vebox;
    }else{
       vebox_out_id = proc_ctx->surface_output;
    } 

    struct object_surface* obj_surf_in  = SURFACE(vebox_in_id);
    struct object_surface* obj_surf_out = SURFACE(vebox_out_id);
       
    if(obj_surf_in->bo == NULL){
          input_fourcc = VA_FOURCC('N','V','1','2');
          input_sampling = SUBSAMPLE_YUV420;
          input_tiling = 0;
          i965_check_alloc_surface_bo(ctx, obj_surf_in, input_tiling, input_fourcc, input_sampling);
    } else {
        input_fourcc = obj_surf_in->fourcc;
        input_sampling = obj_surf_in->subsampling;
        dri_bo_get_tiling(obj_surf_in->bo, &input_tiling, &swizzle);
        input_tiling = !!input_tiling;
    }

    if(obj_surf_out->bo == NULL){
          output_fourcc = VA_FOURCC('N','V','1','2');
          output_sampling = SUBSAMPLE_YUV420;
          output_tiling = 0;
          i965_check_alloc_surface_bo(ctx, obj_surf_out, output_tiling, output_fourcc, output_sampling);
    }else {
        output_fourcc   = obj_surf_out->fourcc;
        output_sampling = obj_surf_out->subsampling;
        dri_bo_get_tiling(obj_surf_out->bo, &output_tiling, &swizzle);
        output_tiling = !!output_tiling;
    }

    /* vebox pipelien input surface format info */
    proc_ctx->fourcc_input = input_fourcc;
    proc_ctx->fourcc_output = output_fourcc;
   
    /* create pipeline surfaces */
    VASurfaceID surfaces[FRAME_STORE_SUM];
    va_status =   i965_CreateSurfaces(ctx,
                                   proc_ctx ->width_input,
                                   proc_ctx ->height_input,
                                   VA_RT_FORMAT_YUV420,
                                   FRAME_STORE_SUM,
                                   &surfaces);
    assert(va_status == VA_STATUS_SUCCESS);

    for(i = 0; i < FRAME_STORE_SUM; i ++) {
        if(proc_ctx->frame_store[i].bo){
            continue; //refer external surface for vebox pipeline
        }
    
        VASurfaceID new_surface;
        va_status =   i965_CreateSurfaces(ctx,
                                          proc_ctx ->width_input,
                                          proc_ctx ->height_input,
                                          VA_RT_FORMAT_YUV420,
                                          1,
                                          &new_surface);
        assert(va_status == VA_STATUS_SUCCESS);

        proc_ctx->frame_store[i].surface_id = new_surface;
        struct object_surface* obj_surf = SURFACE(new_surface);

        if( i <= FRAME_IN_PREVIOUS || i == FRAME_OUT_CURRENT_DN) {
           i965_check_alloc_surface_bo(ctx, obj_surf, input_tiling, input_fourcc, input_sampling);
        } else if( i == FRAME_IN_STMM || i == FRAME_OUT_STMM){
            i965_check_alloc_surface_bo(ctx, obj_surf, 1, input_fourcc, input_sampling);
        } else if( i >= FRAME_OUT_CURRENT){
            i965_check_alloc_surface_bo(ctx, obj_surf, output_tiling, output_fourcc, output_sampling);
        }
        proc_ctx->frame_store[i].bo = obj_surf->bo;
        dri_bo_reference(proc_ctx->frame_store[i].bo);
        proc_ctx->frame_store[i].is_internal_surface = 1;
    }

    /* alloc dndi state table  */
    dri_bo_unreference(proc_ctx->dndi_state_table.bo);
    bo = dri_bo_alloc(i965->intel.bufmgr,
                      "vebox: dndi state Buffer",
                      0x1000, 0x1000);
    proc_ctx->dndi_state_table.bo = bo;
    dri_bo_reference(proc_ctx->dndi_state_table.bo);
 
    /* alloc iecp state table  */
    dri_bo_unreference(proc_ctx->iecp_state_table.bo);
    bo = dri_bo_alloc(i965->intel.bufmgr,
                      "vebox: iecp state Buffer",
                      0x1000, 0x1000);
    proc_ctx->iecp_state_table.bo = bo;
    dri_bo_reference(proc_ctx->iecp_state_table.bo);

    /* alloc gamut state table  */
    dri_bo_unreference(proc_ctx->gamut_state_table.bo);
    bo = dri_bo_alloc(i965->intel.bufmgr,
                      "vebox: gamut state Buffer",
                      0x1000, 0x1000);
    proc_ctx->gamut_state_table.bo = bo;
    dri_bo_reference(proc_ctx->gamut_state_table.bo);

    /* alloc vertex state table  */
    dri_bo_unreference(proc_ctx->vertex_state_table.bo);
    bo = dri_bo_alloc(i965->intel.bufmgr,
                      "vertex: iecp state Buffer",
                      0x1000, 0x1000);
    proc_ctx->vertex_state_table.bo = bo;
    dri_bo_reference(proc_ctx->vertex_state_table.bo);

}

void hsw_veb_surface_reference(VADriverContextP ctx,
                              struct intel_vebox_context *proc_ctx)
{
    struct object_surface * obj_surf; 
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    VAGenericID vebox_in_id, vebox_out_id;

    if(proc_ctx->surface_input_vebox != -1){
       vebox_in_id = proc_ctx->surface_input_vebox;
    }else{
       vebox_in_id = proc_ctx->surface_input;
    } 

    if(proc_ctx->surface_output_vebox != -1){
       vebox_out_id = proc_ctx->surface_output_vebox;
    }else{
       vebox_out_id = proc_ctx->surface_output;
    } 

    /* update the input surface */ 
     obj_surf = SURFACE(vebox_in_id);
     dri_bo_unreference(proc_ctx->frame_store[FRAME_IN_CURRENT].bo);
     proc_ctx->frame_store[FRAME_IN_CURRENT].surface_id = vebox_in_id;
     proc_ctx->frame_store[FRAME_IN_CURRENT].bo = obj_surf->bo;
     proc_ctx->frame_store[FRAME_IN_CURRENT].is_internal_surface = 0;
     dri_bo_reference(proc_ctx->frame_store[FRAME_IN_CURRENT].bo);

     /* update the output surface */ 
     obj_surf = SURFACE(vebox_out_id);
     if(proc_ctx->filters_mask == VPP_DNDI_DN){
         dri_bo_unreference(proc_ctx->frame_store[FRAME_OUT_CURRENT_DN].bo);
         proc_ctx->frame_store[FRAME_OUT_CURRENT_DN].surface_id = vebox_out_id;
         proc_ctx->frame_store[FRAME_OUT_CURRENT_DN].bo = obj_surf->bo;
         proc_ctx->frame_store[FRAME_OUT_CURRENT_DN].is_internal_surface = 0;
         dri_bo_reference(proc_ctx->frame_store[FRAME_OUT_CURRENT_DN].bo);
     }else {
         dri_bo_unreference(proc_ctx->frame_store[FRAME_OUT_CURRENT].bo);
         proc_ctx->frame_store[FRAME_OUT_CURRENT].surface_id = vebox_out_id;
         proc_ctx->frame_store[FRAME_OUT_CURRENT].bo = obj_surf->bo;
         proc_ctx->frame_store[FRAME_OUT_CURRENT].is_internal_surface = 0;
         dri_bo_reference(proc_ctx->frame_store[FRAME_OUT_CURRENT].bo);
     } 
}

void hsw_veb_surface_unreference(VADriverContextP ctx,
                                 struct intel_vebox_context *proc_ctx)
{
    /* unreference the input surface */ 
    dri_bo_unreference(proc_ctx->frame_store[FRAME_IN_CURRENT].bo);
    proc_ctx->frame_store[FRAME_IN_CURRENT].surface_id = -1;
    proc_ctx->frame_store[FRAME_IN_CURRENT].bo = NULL;
    proc_ctx->frame_store[FRAME_IN_CURRENT].is_internal_surface = 0;

    /* unreference the shared output surface */ 
    if(proc_ctx->filters_mask == VPP_DNDI_DN){
       dri_bo_unreference(proc_ctx->frame_store[FRAME_OUT_CURRENT_DN].bo);
       proc_ctx->frame_store[FRAME_OUT_CURRENT_DN].surface_id = -1;
       proc_ctx->frame_store[FRAME_OUT_CURRENT_DN].bo = NULL;
       proc_ctx->frame_store[FRAME_OUT_CURRENT_DN].is_internal_surface = 0;
    }else{
       dri_bo_unreference(proc_ctx->frame_store[FRAME_OUT_CURRENT].bo);
       proc_ctx->frame_store[FRAME_OUT_CURRENT].surface_id = -1;
       proc_ctx->frame_store[FRAME_OUT_CURRENT].bo = NULL;
       proc_ctx->frame_store[FRAME_OUT_CURRENT].is_internal_surface = 0;
    }
}

int hsw_veb_pre_format_convert(VADriverContextP ctx,
                           struct intel_vebox_context *proc_ctx)
{
    VAStatus va_status;
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct object_surface* obj_surf_input = SURFACE(proc_ctx->surface_input);
    struct object_surface* obj_surf_output = SURFACE(proc_ctx->surface_output);
    struct object_surface* obj_surf_input_vebox;
    struct object_surface* obj_surf_output_vebox;
    struct object_surface* obj_surf_output_scaled;

    proc_ctx->width_input   = obj_surf_input->orig_width;
    proc_ctx->height_input  = obj_surf_input->orig_height;
    proc_ctx->width_output  = obj_surf_output->orig_width;
    proc_ctx->height_output = obj_surf_output->orig_height;
   
    /* only partial frame is not supported to be processed */
    /*
    assert(proc_ctx->width_input   == proc_ctx->pipeline_param->surface_region->width);
    assert(proc_ctx->height_input  == proc_ctx->pipeline_param->surface_region->height);
    assert(proc_ctx->width_output  == proc_ctx->pipeline_param->output_region->width);
    assert(proc_ctx->height_output == proc_ctx->pipeline_param->output_region->height);
    */

    if(proc_ctx->width_output  != proc_ctx->width_input ||
       proc_ctx->height_output != proc_ctx->height_input){
        proc_ctx->format_convert_flags |= POST_SCALING_CONVERT;
    }

     /* convert the following format to NV12 format */
     if(obj_surf_input->fourcc ==  VA_FOURCC('Y','V','1','2') ||
           obj_surf_input->fourcc ==  VA_FOURCC('I','4','2','0') ||
           obj_surf_input->fourcc ==  VA_FOURCC('I','M','C','1') ||
           obj_surf_input->fourcc ==  VA_FOURCC('I','M','C','2')){

         proc_ctx->format_convert_flags |= PRE_FORMAT_CONVERT;

      } else if(obj_surf_input->fourcc ==  VA_FOURCC('R','G','B','A') ||
               obj_surf_input->fourcc ==  VA_FOURCC('A','Y','U','V') ||
               obj_surf_input->fourcc ==  VA_FOURCC('Y','U','Y','2') ||
               obj_surf_input->fourcc ==  VA_FOURCC('N','V','1','2')){
                // nothing to do here
     } else {
           /* not support other format as input */ 
           assert(0);
     }
    
     if(proc_ctx->format_convert_flags & PRE_FORMAT_CONVERT){
        if(proc_ctx->surface_input_vebox == -1){
             va_status = i965_CreateSurfaces(ctx,
                                            proc_ctx->width_input,
                                            proc_ctx->height_input,
                                            VA_RT_FORMAT_YUV420,
                                            1,
                                            &(proc_ctx->surface_input_vebox));
             assert(va_status == VA_STATUS_SUCCESS);
             obj_surf_input_vebox = SURFACE(proc_ctx->surface_input_vebox);
             i965_check_alloc_surface_bo(ctx, obj_surf_input_vebox, 1, VA_FOURCC('N','V','1','2'), SUBSAMPLE_YUV420);
         }
       
         vpp_surface_convert(ctx, proc_ctx->surface_input_vebox, proc_ctx->surface_input);
      }

      /* create one temporary NV12 surfaces for conversion*/
     if(obj_surf_output->fourcc ==  VA_FOURCC('Y','V','1','2') ||
        obj_surf_output->fourcc ==  VA_FOURCC('I','4','2','0') ||
        obj_surf_output->fourcc ==  VA_FOURCC('I','M','C','1') ||
        obj_surf_output->fourcc ==  VA_FOURCC('I','M','C','2')) {  

        proc_ctx->format_convert_flags |= POST_FORMAT_CONVERT;
    } else if(obj_surf_output->fourcc ==  VA_FOURCC('R','G','B','A') ||
              obj_surf_output->fourcc ==  VA_FOURCC('A','Y','U','V') ||
              obj_surf_output->fourcc ==  VA_FOURCC('Y','U','Y','2') ||
              obj_surf_output->fourcc ==  VA_FOURCC('N','V','1','2')){
              /* Nothing to do here */
     } else {
           /* not support other format as input */ 
           assert(0);
     }
  
     if(proc_ctx->format_convert_flags & POST_FORMAT_CONVERT ||
        proc_ctx->format_convert_flags & POST_SCALING_CONVERT){
       if(proc_ctx->surface_output_vebox == -1){
             va_status = i965_CreateSurfaces(ctx,
                                            proc_ctx->width_input,
                                            proc_ctx->height_input,
                                            VA_RT_FORMAT_YUV420,
                                            1,
                                            &(proc_ctx->surface_output_vebox));
             assert(va_status == VA_STATUS_SUCCESS);
             obj_surf_output_vebox = SURFACE(proc_ctx->surface_output_vebox);
             i965_check_alloc_surface_bo(ctx, obj_surf_output_vebox, 1, VA_FOURCC('N','V','1','2'), SUBSAMPLE_YUV420);
       }
     }   

     if(proc_ctx->format_convert_flags & POST_SCALING_CONVERT){
       if(proc_ctx->surface_output_scaled == -1){
             va_status = i965_CreateSurfaces(ctx,
                                            proc_ctx->width_output,
                                            proc_ctx->height_output,
                                            VA_RT_FORMAT_YUV420,
                                            1,
                                            &(proc_ctx->surface_output_scaled));
             assert(va_status == VA_STATUS_SUCCESS);
             obj_surf_output_vebox = SURFACE(proc_ctx->surface_output_scaled);
             i965_check_alloc_surface_bo(ctx, obj_surf_output_vebox, 1, VA_FOURCC('N','V','1','2'), SUBSAMPLE_YUV420);
       }
     } 
    
     return 0;
}

int hsw_veb_post_format_convert(VADriverContextP ctx,
                           struct intel_vebox_context *proc_ctx)
{
     struct i965_driver_data *i965 = i965_driver_data(ctx);
     VASurfaceID surf_id_pipe_out = 0;

     if(proc_ctx->filters_mask == VPP_DNDI_DN){
         surf_id_pipe_out = proc_ctx->frame_store[FRAME_OUT_CURRENT_DN].surface_id;
     } else {
         surf_id_pipe_out = proc_ctx->frame_store[FRAME_OUT_CURRENT].surface_id;
     }
 
    if(!(proc_ctx->format_convert_flags & POST_FORMAT_CONVERT) &&
        !(proc_ctx->format_convert_flags & POST_SCALING_CONVERT)){
        /* Output surface format is covered by vebox pipeline and 
         * processed picture is already store in output surface 
         * so nothing will be done here */
    } else if ((proc_ctx->format_convert_flags & POST_FORMAT_CONVERT) &&
               !(proc_ctx->format_convert_flags & POST_SCALING_CONVERT)){
       /* convert and copy NV12 to YV12/IMC3/IMC2 output*/
        vpp_surface_convert(ctx,proc_ctx->surface_output, surf_id_pipe_out);

    } else if(proc_ctx->format_convert_flags & POST_SCALING_CONVERT) {
       /* scaling, convert and copy NV12 to YV12/IMC3/IMC2/ output*/
        assert((SURFACE(surf_id_pipe_out))->fourcc == VA_FOURCC('N','V','1','2'));
     
        /* first step :surface scaling */
        vpp_surface_scaling(ctx,proc_ctx->surface_output_scaled, surf_id_pipe_out);

        /* second step: color format convert and copy to output */
        struct object_surface *obj_surf = SURFACE(proc_ctx->surface_output);
        if(obj_surf->fourcc ==  VA_FOURCC('Y','V','1','2') ||
           obj_surf->fourcc ==  VA_FOURCC('I','4','2','0') ||
           obj_surf->fourcc ==  VA_FOURCC('Y','U','Y','2') ||
           obj_surf->fourcc ==  VA_FOURCC('I','M','C','2')) {  
           vpp_surface_convert(ctx,proc_ctx->surface_output, proc_ctx->surface_output_scaled);
       }else {
           assert(0); 
       }
   }

    return 0;
}

VAStatus gen75_vebox_process_picture(VADriverContextP ctx,
                         struct intel_vebox_context *proc_ctx)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
 
    if(proc_ctx->is_first_frame) {
	/* prepare the basic pipeline */	
        VAProcPipelineParameterBuffer *pipe = proc_ctx->pipeline_param;
        VABufferID *filter_ids = (VABufferID*)proc_ctx->pipeline_param->filters ;
        VAProcFilterParameterBuffer* filter = NULL;
        struct object_buffer *obj_buf = NULL;
        unsigned int i;

        for(i = 0; i < pipe->num_filters; i ++) {
            obj_buf = BUFFER((*(filter_ids + i)));
            filter = (VAProcFilterParameterBuffer*)obj_buf-> buffer_store->buffer;
            
            if(filter->type == VAProcFilterNoiseReduction) {
                proc_ctx->filters_mask |= VPP_DNDI_DN;
                proc_ctx->filter_dn = filter;
            } else if (filter->type == VAProcFilterDeinterlacing) {
                proc_ctx->filters_mask |= VPP_DNDI_DI;
                proc_ctx->filter_di = filter;
            } else if (filter->type == VAProcFilterColorBalance) {
                 proc_ctx->filters_mask |= VPP_IECP_PRO_AMP;
                 proc_ctx->filter_iecp_amp = filter;
            } else if (filter->type == VAProcFilterColorStandard){
                 proc_ctx->filters_mask |= VPP_IECP_CSC;
                 proc_ctx->filter_iecp_csc = filter;
          /*} else if (filter->type == VAProcFilterSkinToneDetectAndEnhance){
                 proc_ctx->filters_mask |= VPP_IECP_STD_STE;
                 proc_ctx->filter_iecp_std = filter;
            } else if (filter->type == VAProcFilterTotalColorControl){
                 proc_ctx->filters_mask |= VPP_IECP_TCC;
                 proc_ctx->filter_iecp_tcc = filter;
          */} else {
                 //not supported filter type
                 return VA_STATUS_ERROR_ATTR_NOT_SUPPORTED;
            }
        }
   }

    hsw_veb_pre_format_convert(ctx, proc_ctx);
    hsw_veb_surface_reference(ctx, proc_ctx);

    if(proc_ctx->is_first_frame){
        hsw_veb_resource_prepare(ctx, proc_ctx);
    }

    intel_batchbuffer_start_atomic_veb(proc_ctx->batch, 0x1000);
    intel_batchbuffer_emit_mi_flush(proc_ctx->batch);
    hsw_veb_surface_state(ctx, proc_ctx, INPUT_SURFACE); 
    hsw_veb_surface_state(ctx, proc_ctx, OUTPUT_SURFACE); 
    hsw_veb_state_table_setup(ctx, proc_ctx);

    hsw_veb_state_command(ctx, proc_ctx);		
    hsw_veb_dndi_iecp_command(ctx, proc_ctx);
    intel_batchbuffer_end_atomic(proc_ctx->batch);
    intel_batchbuffer_flush(proc_ctx->batch);

    hsw_veb_post_format_convert(ctx, proc_ctx);
    hsw_veb_surface_unreference(ctx, proc_ctx);

 
    if(proc_ctx->is_first_frame)
       proc_ctx->is_first_frame = 0; 
     
    return VA_STATUS_SUCCESS;
}

void gen75_vebox_context_destroy(VADriverContextP ctx, 
                          struct intel_vebox_context *proc_ctx)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct object_surface * obj_surf = NULL;
    int i;

    if(proc_ctx->surface_input_vebox != -1){
       obj_surf = SURFACE(proc_ctx->surface_input_vebox);
       i965_DestroySurfaces(ctx, &proc_ctx->surface_input_vebox, 1);
       proc_ctx->surface_input_vebox = -1;
     }

    if(proc_ctx->surface_output_vebox != -1){
       obj_surf = SURFACE(proc_ctx->surface_output_vebox);
       i965_DestroySurfaces(ctx, &proc_ctx->surface_output_vebox, 1);
       proc_ctx->surface_output_vebox = -1;
     }

    if(proc_ctx->surface_output_scaled != -1){
       obj_surf = SURFACE(proc_ctx->surface_output_scaled);
       i965_DestroySurfaces(ctx, &proc_ctx->surface_output_scaled, 1);
       proc_ctx->surface_output_scaled = -1;
     }

    for(i = 0; i < FRAME_STORE_SUM; i ++) {
        if(proc_ctx->frame_store[i].bo){
           dri_bo_unreference(proc_ctx->frame_store[i].bo);
           i965_DestroySurfaces(ctx, &proc_ctx->frame_store[i].surface_id, 1);
        }

       proc_ctx->frame_store[i].surface_id = -1;
       proc_ctx->frame_store[i].bo = NULL;
       proc_ctx->frame_store[i].valid = 0;
    }

    /* dndi state table  */
    dri_bo_unreference(proc_ctx->dndi_state_table.bo);
    proc_ctx->dndi_state_table.bo = NULL;

    /* iecp state table  */
    dri_bo_unreference(proc_ctx->iecp_state_table.bo);
    proc_ctx->dndi_state_table.bo = NULL;
 
    /* gamut statu table */
    dri_bo_unreference(proc_ctx->gamut_state_table.bo);
    proc_ctx->gamut_state_table.bo = NULL;

    /* vertex state table  */
    dri_bo_unreference(proc_ctx->vertex_state_table.bo);
    proc_ctx->vertex_state_table.bo = NULL;

    intel_batchbuffer_free(proc_ctx->batch);

    free(proc_ctx);
}

struct intel_vebox_context * gen75_vebox_context_init(VADriverContextP ctx)
{
    struct intel_driver_data *intel = intel_driver_data(ctx);
    struct intel_vebox_context *proc_context = calloc(1, sizeof(struct intel_vebox_context));

    proc_context->batch = intel_batchbuffer_new(intel, I915_EXEC_VEBOX, 0);
    memset(proc_context->frame_store, 0, sizeof(VEBFrameStore)*FRAME_STORE_SUM);
  
    proc_context->filters_mask          = 0;
    proc_context->is_first_frame        = 1;
    proc_context->surface_input_vebox   = -1;
    proc_context->surface_output_vebox  = -1;
    proc_context->surface_output_scaled = -1;
    proc_context->filters_mask          = 0;
    proc_context->format_convert_flags  = 0;

    return proc_context;
}
