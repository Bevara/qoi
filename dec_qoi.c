/*
 *			GPAC - Multimedia Framework C SDK
 *
 *  This file is part of GPAC / QOI ("Quite OK Image Format") decoder
 *  filter, based on qoi.h (https://github.com/phoboslab/qoi) - a
 *  single-header library, compiled directly into this filter (no
 *  prebuilt static library needed, same as qdbmp/rfpcm in this repo).
 */

#include <gpac/filters.h>
#include <gpac/constants.h>
#include <string.h>
#include <stdlib.h>

#define QOI_IMPLEMENTATION
#define QOI_NO_STDIO
#include "qoi.h"

typedef struct
{
	GF_FilterPid *ipid, *opid;
	Bool is_playing;
} GF_QOIDecCtx;

static GF_Err qoidec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_QOIDecCtx *ctx = (GF_QOIDecCtx *)gf_filter_get_udta(filter);

	if (is_remove)
	{
		if (ctx->opid)
		{
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		ctx->ipid = NULL;
		return GF_OK;
	}
	if (!gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	ctx->ipid = pid;
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);

	if (!ctx->opid)
	{
		ctx->opid = gf_filter_pid_new(filter);
	}

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_VISUAL));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW));
	/* PIXFMT must be set here, not just after decode in process(): GPAC's
	 * output filter resolution needs it on the PID before any data flows,
	 * or connection fails with "No suitable filter to adapt caps" even
	 * though the value gets overwritten (RGB vs RGBA, once desc.channels
	 * is known) once decoding actually starts - same fix as dec_bpg.c. */
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(GF_PIXEL_RGB));

	return GF_OK;
}

static Bool qoidec_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_QOIDecCtx *ctx = (GF_QOIDecCtx *)gf_filter_get_udta(filter);
	switch (evt->base.type)
	{
	case GF_FEVT_PLAY:
		ctx->is_playing = GF_TRUE;
		return GF_FALSE;
	case GF_FEVT_STOP:
		ctx->is_playing = GF_FALSE;
		return GF_FALSE;
	default:
		return GF_FALSE;
	}
}

static GF_Err qoidec_process(GF_Filter *filter)
{
	GF_FilterPacket *pck;
	u8 *data;
	u32 size;
	qoi_desc desc;
	void *pixels;
	GF_QOIDecCtx *ctx = (GF_QOIDecCtx *)gf_filter_get_udta(filter);

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck)
	{
		if (gf_filter_pid_is_eos(ctx->ipid))
		{
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}
	data = (u8 *)gf_filter_pck_get_data(pck, &size);
	if (!data)
	{
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_IO_ERR;
	}

	pixels = qoi_decode(data, (int)size, &desc, 0);
	gf_filter_pid_drop_packet(ctx->ipid);

	if (!pixels)
	{
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[QOIDec] Failed to decode QOI image\n"));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	{
		u32 out_size = desc.width * desc.height * desc.channels;
		u8 *output;
		GF_FilterPacket *dst_pck;

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(desc.width));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(desc.height));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(desc.width * desc.channels));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT((desc.channels == 4) ? GF_PIXEL_RGBA : GF_PIXEL_RGB));

		dst_pck = gf_filter_pck_new_alloc(ctx->opid, out_size, &output);
		if (!dst_pck)
		{
			free(pixels);
			return GF_OUT_OF_MEM;
		}
		memcpy(output, pixels, out_size);
		free(pixels);

		gf_filter_pck_set_cts(dst_pck, 0);
		gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
		gf_filter_pck_send(dst_pck);
	}

	gf_filter_pid_set_eos(ctx->opid);
	return GF_EOS;
}

static void qoidec_finalize(GF_Filter *filter)
{
}

static const GF_FilterCapability QOIDecCaps[] =
	{
		CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
		CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "qoi"),
		CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "image/qoi|image/x-qoi"),
		CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
		CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

GF_FilterRegister QOIDecoderRegister = {
	.name = "qoidec",
	GF_FS_SET_DESCRIPTION("QOI image decoder")
		GF_FS_SET_HELP("This filter decodes QOI (\"Quite OK Image Format\") images using qoi.h.")
			.private_size = sizeof(GF_QOIDecCtx),
	SETCAPS(QOIDecCaps),
	.configure_pid = qoidec_configure_pid,
	.process = qoidec_process,
	.process_event = qoidec_process_event,
	.finalize = qoidec_finalize,
};

const GF_FilterRegister * EMSCRIPTEN_KEEPALIVE qoidec_register(GF_FilterSession *session)
{
	return &QOIDecoderRegister;
}

#include "filter_register.h"
__attribute__((constructor))
void register_qoidec(void) {
    gf_filter_auto_register("qoidec", qoidec_register);
}
