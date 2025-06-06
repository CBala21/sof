// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/common.h>
#include <sof/audio/audio_stream.h>
#include <user/fir.h>

#include "tdfb.h"
#include "tdfb_comp.h"

#if TDFB_HIFI3

#include <sof/math/fir_hifi3.h>

#if CONFIG_FORMAT_S16LE
void tdfb_fir_s16(struct tdfb_comp_data *cd, struct input_stream_buffer *bsource,
		  struct output_stream_buffer *bsink, int frames)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	struct sof_tdfb_config *cfg = cd->config;
	struct fir_state_32x16 *f;
	ae_int16x4 d;
	ae_int32 y0;
	ae_int32 y1;
	ae_int16 *x = audio_stream_get_rptr(source);
	ae_int16 *y = audio_stream_get_wptr(sink);
	int shift;
	int is2;
	int is;
	int om;
	int i;
	int j;
	int k;
	int in_nch = audio_stream_get_channels(source);
	int out_nch = audio_stream_get_channels(sink);
	int emp_ch = 0;
	int n, nmax;
	int remaining_frames = frames;
	const int inc = sizeof(ae_int16);

	while (remaining_frames) {
		nmax = audio_stream_frames_without_wrap(source, x);
		n = MIN(remaining_frames, nmax);
		nmax = audio_stream_frames_without_wrap(sink, y);
		n = MIN(n, nmax);

		for (j = 0; j < n; j += 2) {
			/* Clear output mix*/
			memset(cd->out, 0,  2 * out_nch * sizeof(int32_t));

			/* Read two frames from all input channels
			 * there won't be buffer overflow since we
			 * set 2 frames align in tdfb_prepare function.
			 */
			for (i = 0; i < 2 * in_nch; i++) {
				AE_L16_XP(d, x, inc);
				cd->in[i] = (ae_int32)AE_CVT32X2F16_32(d);
				if (cd->direction_updates)
					tdfb_direction_copy_emphasis(cd, in_nch, &emp_ch,
								     cd->in[i]);
			}

			/* Run and mix all filters to their output channel */
			for (i = 0; i < cfg->num_filters; i++) {
				is = cd->input_channel_select[i];
				is2 = is + in_nch;
				om = cd->output_channel_mix[i];

				/* Get filter instance */
				f = &cd->fir[i];
				shift = -f->out_shift;

				/* Compute FIR and mix as Q5.27*/
				fir_core_setup_circular(f);
				fir_32x16_2x(f, cd->in[is], cd->in[is2], &y0, &y1, shift);
				for (k = 0; k < out_nch; k++) {
					if (om & 1) {
						cd->out[k] += (int32_t)y0 >> 4;
						cd->out[k + out_nch] +=
							(int32_t)y1 >> 4;
					}
					om = om >> 1;
				}
			}

			/* Write two frames of output. The values in out[] are shifted
			 * left and saturated to convert to Q1.27. The values
			 * are then rounded to 16 bit and converted to Q1.15 for
			 * sink buffer. TODO: Could saturate four samples with
			 * one AE_ROUND16X4F32SSYM() instruction.
			 */
			for (i = 0; i < 2 * out_nch; i++) {
				d = AE_ROUND16X4F32SSYM(0, AE_SLAI32S(cd->out[i], 4));
				AE_S16_0_XP(d, y, inc);
			}
		}
		remaining_frames -= n;
		x = audio_stream_wrap(source, x);
		y = audio_stream_wrap(sink, y);
	}
}
#endif

#if CONFIG_FORMAT_S24LE
void tdfb_fir_s24(struct tdfb_comp_data *cd, struct input_stream_buffer *bsource,
		  struct output_stream_buffer *bsink, int frames)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	struct sof_tdfb_config *cfg = cd->config;
	struct fir_state_32x16 *f;
	ae_int32x2 d;
	ae_int32 y0;
	ae_int32 y1;
	ae_int32 *x = audio_stream_get_rptr(source);
	ae_int32 *y = audio_stream_get_wptr(sink);
	int shift;
	int is2;
	int is;
	int om;
	int i;
	int j;
	int k;
	int in_nch = audio_stream_get_channels(source);
	int out_nch = audio_stream_get_channels(sink);
	int emp_ch = 0;
	int n, nmax;
	int remaining_frames = frames;
	const int inc = sizeof(ae_int32);

	while (remaining_frames) {
		nmax = audio_stream_frames_without_wrap(source, x);
		n = MIN(remaining_frames, nmax);
		nmax = audio_stream_frames_without_wrap(sink, y);
		n = MIN(n, nmax);

		for (j = 0; j < n; j += 2) {
			/* Clear output mix*/
			memset(cd->out, 0,  2 * out_nch * sizeof(int32_t));

			/* Read two frames from all input channels
			 * there won't be buffer overflow since we
			 * set 2 frames align in tdfb_prepare function.
			 */
			for (i = 0; i < 2 * in_nch; i++) {
				AE_L32_XP(d, x, inc);
				cd->in[i] = AE_SLAI32(d, 8);
				if (cd->direction_updates)
					tdfb_direction_copy_emphasis(cd, in_nch, &emp_ch,
								     cd->in[i]);
			}

			/* Run and mix all filters to their output channel */
			for (i = 0; i < cfg->num_filters; i++) {
				is = cd->input_channel_select[i];
				is2 = is + in_nch;
				om = cd->output_channel_mix[i];

				/* Get filter instance */
				f = &cd->fir[i];
				shift = -f->out_shift;

				/* Compute FIR and mix as Q5.27*/
				fir_core_setup_circular(f);
				fir_32x16_2x(f, cd->in[is], cd->in[is2], &y0, &y1, shift);
				for (k = 0; k < out_nch; k++) {
					if (om & 1) {
						cd->out[k] += (int32_t)y0 >> 4;
						cd->out[k + out_nch] +=
							(int32_t)y1 >> 4;
					}
					om = om >> 1;
				}
			}

			/* Write two frames of output. The values in out[] are shifted
			 * left and saturated to convert to Q1.27. The values
			 * are then rounded to 16 bit and converted to Q1.15 for
			 * sink buffer. TODO: Could saturate four samples with
			 * one AE_ROUND16X4F32SSYM() instruction.
			 */
			for (i = 0; i < 2 * out_nch; i++) {
				d = AE_SRAI32(AE_SLAI32S(AE_SRAI32R(cd->out[i], 4), 8), 8);
				AE_S32_L_XP(d, y, inc);
			}
		}
		remaining_frames -= n;
		x = audio_stream_wrap(source, x);
		y = audio_stream_wrap(sink, y);
	}
}
#endif

#if CONFIG_FORMAT_S32LE
void tdfb_fir_s32(struct tdfb_comp_data *cd, struct input_stream_buffer *bsource,
		  struct output_stream_buffer *bsink, int frames)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	struct sof_tdfb_config *cfg = cd->config;
	struct fir_state_32x16 *f;
	ae_int32x2 d;
	ae_int32 y0;
	ae_int32 y1;
	ae_int32 *x = audio_stream_get_rptr(source);
	ae_int32 *y = audio_stream_get_wptr(sink);
	int shift;
	int is2;
	int is;
	int om;
	int i;
	int j;
	int k;
	int in_nch = audio_stream_get_channels(source);
	int out_nch = audio_stream_get_channels(sink);
	int emp_ch = 0;
	int n, nmax;
	int remaining_frames = frames;
	const int inc = sizeof(ae_int32);

	while (remaining_frames) {
		nmax = audio_stream_frames_without_wrap(source, x);
		n = MIN(remaining_frames, nmax);
		nmax = audio_stream_frames_without_wrap(sink, y);
		n = MIN(n, nmax);

		for (j = 0; j < n; j += 2) {
			/* Clear output mix*/
			memset(cd->out, 0,  2 * out_nch * sizeof(int32_t));

			/* Read two frames from all input channels
			 * there won't be buffer overflow since we
			 * set 2 frames align in tdfb_prepare function.
			 */
			for (i = 0; i < 2 * in_nch; i++) {
				AE_L32_XC(d, x, inc);
				cd->in[i] = d;
				if (cd->direction_updates)
					tdfb_direction_copy_emphasis(cd, in_nch, &emp_ch,
								     cd->in[i]);
			}

			for (i = 0; i < cfg->num_filters; i++) {
				is = cd->input_channel_select[i];
				is2 = is + in_nch;
				om = cd->output_channel_mix[i];

				/* Get filter instance */
				f = &cd->fir[i];
				shift = -f->out_shift;

				/* Compute FIR and mix as Q5.27*/
				fir_core_setup_circular(f);
				fir_32x16_2x(f, cd->in[is], cd->in[is2], &y0, &y1, shift);
				for (k = 0; k < out_nch; k++) {
					if (om & 1) {
						cd->out[k] += (int32_t)y0 >> 4;
						cd->out[k + out_nch] +=
							(int32_t)y1 >> 4;
					}
					om = om >> 1;
				}
			}

			/* Write two frames of output. In Q5.27 to Q1.31 conversion
			 * rounding is not applicable so just shift left by 4 and
			 * saturate. TODO: Could shift two samples with one
			 * instruction.
			 */
			for (i = 0; i < 2 * out_nch; i++) {
				d = AE_SLAI32S(cd->out[i], 4);
				AE_S32_L_XP(d, y, inc);
			}
		}
		remaining_frames -= n;
		x = audio_stream_wrap(source, x);
		y = audio_stream_wrap(sink, y);
	}
}
#endif

#endif /* TDFB_HIFI3 */

