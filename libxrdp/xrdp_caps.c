/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
 * Copyright (C) Idan Freiberg 2004-2014
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * RDP Capability Sets
 */

#include "libxrdp.h"
#include "parse.h"
#include "../common/rdp_bcgr.h"

#define XRDP_DEBUG 1

/*****************************************************************************/
static int APP_CC
xrdp_caps_send_monitorlayout(struct xrdp_rdp *self) {
	struct stream *s;
	int i;

	make_stream(s);
	init_stream(s, 8192);

	if (xrdp_rdp_init_data(self, s) != 0) {
		free_stream(s);
		return 1;
	}

	out_uint32_le(s, self->client_info.monitorCount); /* monitorCount (4 bytes) */

	/* TODO: validate for allowed monitors in terminal server (maybe by config?) */
	for (i = 0; i < self->client_info.monitorCount; i++) {
		out_uint32_le(s, self->client_info.minfo[i].left);
		out_uint32_le(s, self->client_info.minfo[i].top);
		out_uint32_le(s, self->client_info.minfo[i].right);
		out_uint32_le(s, self->client_info.minfo[i].bottom);
		out_uint32_le(s, self->client_info.minfo[i].is_primary);
	}

	s_mark_end(s);

	if (xrdp_rdp_send_data(self, s, 0x37) != 0) {
		free_stream(s);
		return 1;
	}

	free_stream(s);
	return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_caps_process_general(struct xrdp_rdp *self, struct stream *s, int len) {
	int extraFlags;
	int client_does_fastpath_output;

	if (len < 10 + 2) {
		log_error("not enough data received (%d bytes)",len);
		return 1;
	}
	in_uint8s(s, 10);
	in_uint16_le(s, extraFlags);
	/* use_compact_packets is pretty much 'use rdp5' */
	self->client_info.use_compact_packets = (extraFlags != 0);
	/* op2 is a boolean to use compact bitmap headers in bitmap cache */
	/* set it to same as 'use rdp5' boolean */
	self->client_info.op2 = self->client_info.use_compact_packets;
	/* FASTPATH_OUTPUT_SUPPORTED 0x0001 */
	client_does_fastpath_output = extraFlags & FASTPATH_OUTPUT_SUPPORTED;
	if ((self->client_info.use_fast_path & 1) && !client_does_fastpath_output) {
		/* server supports fast path output and client does not, turn it off */
		self->client_info.use_fast_path &= ~1;
	}
	return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_caps_process_bitmap(struct xrdp_rdp *self, struct stream *s, int len) {

	int preferredBitsPerPixel, receive1BitPerPixel, receive4BitsPerPixel,
			receive8BitsPerPixel, desktopWidth, desktopHeight,

			desktopResizeFlag, bitmapCompressionFlag, highColorFlags,
			drawingFlags, multipleRectangleSupport;

	if (len < 24) {
		g_writeln("xrdp_caps_process_bitmap: error %d", len);
		return 1;
	}

	// preferredBitsPerPixel
	in_uint16_le(s, preferredBitsPerPixel);
	g_writeln("xrdp_caps_process_bitmap : preferredBitsPerPixel :%d",
			preferredBitsPerPixel);
	// receive1BitPerPixel
	in_uint16_le(s, receive1BitPerPixel);
	g_writeln("xrdp_caps_process_bitmap : receive1BitPerPixel :%d",
			receive1BitPerPixel);
	// receive4BitsPerPixel
	in_uint16_le(s, receive4BitsPerPixel);
	g_writeln("xrdp_caps_process_bitmap : receive4BitsPerPixel :%d",
			receive4BitsPerPixel);
	// receive8BitsPerPixel
	in_uint16_le(s, receive8BitsPerPixel);
	g_writeln("xrdp_caps_process_bitmap : receive8BitsPerPixel :%d",
			receive8BitsPerPixel);
	// desktopWidth
	in_uint16_le(s, desktopWidth);
	g_writeln("xrdp_caps_process_bitmap : desktopWidth :%d", desktopWidth);
	// desktopHeight
	in_uint16_le(s, desktopHeight);
	g_writeln("xrdp_caps_process_bitmap : desktopHeight :%d", desktopHeight);
	// pad
	in_uint8s(s, 2);
	// desktopResizeFlag
	in_uint16_le(s, desktopResizeFlag);
	g_writeln("xrdp_caps_process_bitmap : desktopResizeFlag :%d",
			desktopResizeFlag);
	// bitmapCompressionFlag
	in_uint16_le(s, bitmapCompressionFlag);
	g_writeln("xrdp_caps_process_bitmap : bitmapCompressionFlag :%d",
			bitmapCompressionFlag);
	// highColorFlags
	in_uint8(s, highColorFlags);
	g_writeln("xrdp_caps_process_bitmap : highColorFlags :%d", highColorFlags);
	// drawingFlags
	in_uint8(s, drawingFlags);
	g_writeln("xrdp_caps_process_bitmap : drawingFlags :%d", drawingFlags);
	// multipleRectangleSupport
	in_uint16_le(s, multipleRectangleSupport);
	g_writeln("xrdp_caps_process_bitmap : multipleRectangleSupport :%d",
			multipleRectangleSupport);
	// pad
	in_uint8s(s, 2);
	return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_caps_process_order(struct xrdp_rdp *self, struct stream *s, int len) {
	int i;
	char orderSupport[32];

	int ex_flags;
	int orderFlags;

	DEBUG("\nRDP Client order capabilities");
	if (len < 20 + 2 + 2 + 2 + 2 + 2 + 2 + 32 + 2 + 2 + 4 + 4 + 4 + 4) {
		g_writeln("xrdp_caps_process_order: error");
		return 1;
	}
	in_uint8s(s, 16); /* Terminal desc, pad */
	in_uint8s(s, 4);
	in_uint8s(s, 2); /* Cache X granularity */
	in_uint8s(s, 2); /* Cache Y granularity */
	in_uint8s(s, 2); /* Pad */
	in_uint8s(s, 2); // maximumOrderLevel : ignored
	in_uint8s(s, 2); /* Number of fonts */
	in_uint16_le(s, orderFlags); /* Capability flags */

	if (orderFlags & NEGOTIATEORDERSUPPORT) {
		DEBUG("specifies drawing orders specified");
	}
	if (orderFlags & ZEROBOUNDSDELTASSUPPORT) {
		DEBUG("supports for Zero bounds deltas");
	}
	if (orderFlags & COLORINDEXSUPPORT) {
		DEBUG("supports for sending color indices (not RGB values) in orders");
	}
	if (orderFlags & SOLIDPATTERNBRUSHONLY) {
		DEBUG("can receive only solid and pattern brushes");
	}
	if (orderFlags & ORDERFLAGS_EXTRA_FLAGS) {
		DEBUG("orderSupportExFlags contains valid flags");
	}
	// Primary drawing orders
	DEBUG("Supported drawing orders : ");
	in_uint8a(s, orderSupport, 32); /* Orders supported */
	g_memcpy(self->client_info.orders, orderSupport, 32);
	if (orderSupport[TS_NEG_DSTBLT_INDEX] == 1)
		DEBUG("- DstBlt Primary Drawing Order");
	if (orderSupport[TS_NEG_PATBLT_INDEX] == 1)
		DEBUG(
				"- PatBlt Primary Drawing Order and OpaqueRect Primary Drawing Order");
	if (orderSupport[TS_NEG_SCRBLT_INDEX] == 1)
		DEBUG("- ScrBlt Primary Drawing Order");
	if (orderSupport[TS_NEG_MEMBLT_INDEX] == 1)
		DEBUG("- MemBlt Primary Drawing Order");
	if (orderSupport[TS_NEG_MEM3BLT_INDEX] == 1)
		DEBUG("- Mem3Blt Primary Drawing Order");

	if (orderSupport[TS_NEG_DRAWNINEGRID_INDEX] == 1)
		DEBUG("- DrawNineGrid Primary Drawing Order");
	if (orderSupport[TS_NEG_LINETO_INDEX] == 1)
		DEBUG("- LineTo Primary Drawing Order");

	if (orderSupport[TS_NEG_MULTI_DRAWNINEGRID_INDEX] == 1)
		DEBUG("- MultiDrawNineGrid Primary Drawing Order");
	if (orderSupport[TS_NEG_SAVEBITMAP_INDEX] == 1)
		DEBUG("- SaveBitmap Primary Drawing Order");
	if (orderSupport[TS_NEG_MULTIDSTBLT_INDEX] == 1)
		DEBUG("- MultiDstBlt Primary Drawing Order");
	if (orderSupport[TS_NEG_MULTIPATBLT_INDEX] == 1)
		DEBUG("- MultiPatBlt Primary Drawing Order");
	if (orderSupport[TS_NEG_MULTISCRBLT_INDEX] == 1)
		DEBUG("- MultiScrBlt Primary Drawing Order");
	if (orderSupport[TS_NEG_MULTIOPAQUERECT_INDEX] == 1)
		DEBUG("- MultiOpaqueRect Primary Drawing Order");
	if (orderSupport[TS_NEG_FAST_INDEX_INDEX] == 1)
		DEBUG("- FastIndex Primary Drawing Order");
	if (orderSupport[TS_NEG_POLYGON_SC_INDEX] == 1)
		DEBUG("- PolygonSC Primary Drawing Order");
	if (orderSupport[TS_NEG_POLYGON_CB_INDEX] == 1)
		DEBUG("- PolygonCB Primary Drawing Order");
	if (orderSupport[TS_NEG_POLYLINE_INDEX] == 1)
		DEBUG("- Polyline Primary Drawing Order");
	if (orderSupport[TS_NEG_FAST_GLYPH_INDEX] == 1)
		DEBUG("- FastGlyph Primary Drawing Order");
	if (orderSupport[TS_NEG_ELLIPSE_SC_INDEX] == 1)
		DEBUG(
				"- EllipseSC Primary Drawing Order and EllipseCB Primary Drawing Order");
	if (orderSupport[TS_NEG_ELLIPSE_CB_INDEX] == 1)
		DEBUG(
				"- EllipseCB Primary Drawing Order and EllipseSC Primary Drawing Order");
	if (orderSupport[TS_NEG_INDEX_INDEX] == 1)
		DEBUG("- GlyphIndex Primary Drawing Order");

//	DEBUG(("order_caps dump"));
//#if defined(XRDP_DEBUG)
//	g_hexdump(orderSupport, 32);
//#endif
	in_uint8s(s, 2); /* Text capability flags */
	/* read extended order support flags */
	in_uint16_le(s, ex_flags); /* Ex flags */

//	if (orderFlags & ORDERFLAGS_EXTRA_FLAGS) /* ORDER_FLAGS_EXTRA_SUPPORT */
	{
		self->client_info.order_flags_ex = ex_flags;
		if (ex_flags & ORDERFLAGS_EX_CACHE_BITMAP_REV3_SUPPORT) {
			DEBUG("- Cache Bitmap (Revision 3) Secondary Drawing Order");
			self->client_info.bitmap_cache_version |= 4;
		}
		if (ex_flags & ORDERFLAGS_EX_ALTSEC_FRAME_MARKER_SUPPORT) {
			DEBUG("- Frame Marker Alternate Secondary Drawing Order");

		}
	}
	in_uint8s(s, 4); /* Pad */

	in_uint32_le(s, i); /* desktop cache size, usually 0x38400 */
	self->client_info.desktop_cache = i;
	DEBUG("Desktop cache size %d", i);
	in_uint8s(s, 4); /* Unknown */
	in_uint8s(s, 4); /* Unknown */
	return 0;
}

static int APP_CC
xrdp_caps_process_surfacecommand(struct xrdp_rdp *self, struct stream *s,
		int len) {
	return 0;
}
/*****************************************************************************/
/* get the bitmap cache size */
static int APP_CC
xrdp_caps_process_bmpcache(struct xrdp_rdp *self, struct stream *s, int len) {
	int i;

	if (len < 24 + 2 + 2 + 2 + 2 + 2 + 2) {
		g_writeln("xrdp_caps_process_bmpcache: error");
		return 1;
	}
	self->client_info.bitmap_cache_version |= 1;
	in_uint8s(s, 24);
	/* cache 1 */
	in_uint16_le(s, i);
	i = MIN(i, XRDP_MAX_BITMAP_CACHE_IDX);
	i = MAX(i, 0);
	self->client_info.cache1_entries = i;
	in_uint16_le(s, self->client_info.cache1_size);
	/* cache 2 */
	in_uint16_le(s, i);
	i = MIN(i, XRDP_MAX_BITMAP_CACHE_IDX);
	i = MAX(i, 0);
	self->client_info.cache2_entries = i;
	in_uint16_le(s, self->client_info.cache2_size);
	/* caceh 3 */
	in_uint16_le(s, i);
	i = MIN(i, XRDP_MAX_BITMAP_CACHE_IDX);
	i = MAX(i, 0);
	self->client_info.cache3_entries = i;
	in_uint16_le(s, self->client_info.cache3_size);
	DEBUG(
			"cache1 entries %d size %d", self->client_info.cache1_entries, self->client_info.cache1_size);
	DEBUG(
			"cache2 entries %d size %d", self->client_info.cache2_entries, self->client_info.cache2_size);
	DEBUG(
			"cache3 entries %d size %d", self->client_info.cache3_entries, self->client_info.cache3_size);
	return 0;
}

/*****************************************************************************/
/* get the bitmap cache size */
static int APP_CC
xrdp_caps_process_bmpcache2(struct xrdp_rdp *self, struct stream *s, int len) {
	int Bpp = 0;
	int i = 0;

	if (len < 2 + 2 + 4 + 4 + 4) {
		g_writeln("xrdp_caps_process_bmpcache2: error");
		return 1;
	}
	self->client_info.bitmap_cache_version |= 2;
	Bpp = (self->client_info.bpp + 7) / 8;
	in_uint16_le(s, i); /* cache flags */
	self->client_info.bitmap_cache_persist_enable = i;
	in_uint8s(s, 2); /* number of caches in set, 3 */
	in_uint32_le(s, i);
	i = MIN(i, XRDP_MAX_BITMAP_CACHE_IDX);
	i = MAX(i, 0);
	self->client_info.cache1_entries = i;
	self->client_info.cache1_size = 256 * Bpp;
	in_uint32_le(s, i);
	i = MIN(i, XRDP_MAX_BITMAP_CACHE_IDX);
	i = MAX(i, 0);
	self->client_info.cache2_entries = i;
	self->client_info.cache2_size = 1024 * Bpp;
	in_uint32_le(s, i);
	i = i & 0x7fffffff;
	i = MIN(i, XRDP_MAX_BITMAP_CACHE_IDX);
	i = MAX(i, 0);
	self->client_info.cache3_entries = i;
	self->client_info.cache3_size = 4096 * Bpp;
	DEBUG(
			"cache1 entries %d size %d", self->client_info.cache1_entries, self->client_info.cache1_size);
	DEBUG(
			"cache2 entries %d size %d", self->client_info.cache2_entries, self->client_info.cache2_size);
	DEBUG(	"cache3 entries %d size %d", self->client_info.cache3_entries, self->client_info.cache3_size);
	return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_caps_process_cache_v3_codec_id(struct xrdp_rdp *self, struct stream *s,
		int len) {
	int codec_id;

	if (len < 1) {
		g_writeln("xrdp_caps_process_cache_v3_codec_id: error");
		return 1;
	}
	in_uint8(s, codec_id);
	g_writeln("xrdp_caps_process_cache_v3_codec_id: cache_v3_codec_id %d",
			codec_id);
	self->client_info.v3_codec_id = codec_id;
	return 0;
}

/*****************************************************************************/
/* get the number of client cursor cache */
static int APP_CC
xrdp_caps_process_pointer(struct xrdp_rdp *self, struct stream *s, int len) {
	int i;
	int colorPointerFlag;
	int no_new_cursor;

	if (len < 2 + 2 + 2) {
		g_writeln("xrdp_caps_process_pointer: error");
		return 1;
	}
	no_new_cursor = self->client_info.pointer_flags & 2;
	in_uint16_le(s, colorPointerFlag);
	self->client_info.pointer_flags = colorPointerFlag;
	in_uint16_le(s, i);
	i = MIN(i, 32);
	self->client_info.pointer_cache_entries = i;
	if (colorPointerFlag & 1) {
		g_writeln("xrdp_caps_process_pointer: client supports "
				"new(color) cursor");
		in_uint16_le(s, i);
		i = MIN(i, 32);
		self->client_info.pointer_cache_entries = i;
	} else {
		g_writeln("xrdp_caps_process_pointer: client does not support "
				"new(color) cursor");
	}
	if (no_new_cursor) {
		g_writeln("xrdp_caps_process_pointer: new(color) cursor is "
				"disabled by config");
		self->client_info.pointer_flags = 0;
	}
	return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_caps_process_input(struct xrdp_rdp *self, struct stream *s, int len) {
	int inputFlags;
	int client_does_fastpath_input;

	in_uint16_le(s, inputFlags);
	client_does_fastpath_input = (inputFlags & INPUT_FLAG_FASTPATH_INPUT)
			|| (inputFlags & INPUT_FLAG_FASTPATH_INPUT2);
	if ((self->client_info.use_fast_path & 2) && !client_does_fastpath_input) {
		self->client_info.use_fast_path &= ~2;
	}
	return 0;
}

/*****************************************************************************/
/* get the type of client brush cache */
int APP_CC
xrdp_caps_process_brushcache(struct xrdp_rdp *self, struct stream *s, int len) {
	int code;

	if (len < 4) {
		g_writeln("xrdp_caps_process_brushcache: error");
		return 1;
	}
	in_uint32_le(s, code);
	self->client_info.brush_cache_code = code;
	return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_caps_process_offscreen_bmpcache(struct xrdp_rdp *self, struct stream *s,
		int len) {
	int i32;

	if (len < 4 + 2 + 2) {
		g_writeln("xrdp_caps_process_offscreen_bmpcache: error");
		return 1;
	}
	in_uint32_le(s, i32);
	self->client_info.offscreen_support_level = i32;
	in_uint16_le(s, i32);
	self->client_info.offscreen_cache_size = i32 * 1024;
	in_uint16_le(s, i32);
	self->client_info.offscreen_cache_entries = i32;
	g_writeln("xrdp_process_offscreen_bmpcache: support level %d "
			"cache size %d MB cache entries %d",
			self->client_info.offscreen_support_level,
			self->client_info.offscreen_cache_size,
			self->client_info.offscreen_cache_entries);
	return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_caps_process_rail(struct xrdp_rdp *self, struct stream *s, int len) {
	int i32;

	if (len < 4) {
		g_writeln("xrdp_caps_process_rail: error");
		return 1;
	}
	in_uint32_le(s, i32);
	self->client_info.rail_support_level = i32;
	log_info("xrdp_process_capset_rail: rail_support_level %d",
			self->client_info.rail_support_level);
	return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_caps_process_window(struct xrdp_rdp *self, struct stream *s, int len) {
	int i32;

	if (len < 4 + 1 + 2) {
		g_writeln("xrdp_caps_process_window: error");
		return 1;
	}
	in_uint32_le(s, i32);
	self->client_info.wnd_support_level = i32;
	in_uint8(s, i32);
	self->client_info.wnd_num_icon_caches = i32;
	in_uint16_le(s, i32);
	self->client_info.wnd_num_icon_cache_entries = i32;
	log_info("xrdp_process_capset_window wnd_support_level %d "
			"wnd_num_icon_caches %d wnd_num_icon_cache_entries %d",
			self->client_info.wnd_support_level,
			self->client_info.wnd_num_icon_caches,
			self->client_info.wnd_num_icon_cache_entries);
	return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_caps_process_codecs(struct xrdp_rdp *self, struct stream *s, int len) {
	int codec_id;
	int codec_count;
	int index;
	int codec_properties_length;
	int i1;
	char *codec_guid;
	char *next_guid;

	if (len < 1) {
		g_writeln("xrdp_caps_process_codecs: error");
		return 1;
	}
	in_uint8(s, codec_count);
	len--;

	for (index = 0; index < codec_count; index++) {
		codec_guid = s->p;
		if (len < 16 + 1 + 2) {
			g_writeln("xrdp_caps_process_codecs: error");
			return 1;
		}
		in_uint8s(s, 16);
		in_uint8(s, codec_id);
		in_uint16_le(s, codec_properties_length);
		len -= 16 + 1 + 2;
		if (len < codec_properties_length) {
			g_writeln("xrdp_caps_process_codecs: error");
			return 1;
		}
		len -= codec_properties_length;
		next_guid = s->p + codec_properties_length;

		if (g_memcmp(codec_guid, XR_CODEC_GUID_NSCODEC, 16) == 0) {
			g_writeln(
					"xrdp_caps_process_codecs: nscodec codec id %d prop len %d",
					codec_id, codec_properties_length);
			self->client_info.ns_codec_id = codec_id;
			i1 = MIN(64, codec_properties_length);
			g_memcpy(self->client_info.ns_prop, s->p, i1);
			self->client_info.ns_prop_len = i1;
		} else if (g_memcmp(codec_guid, XR_CODEC_GUID_REMOTEFX, 16) == 0) {
			g_writeln("xrdp_caps_process_codecs: rfx codec id %d prop len %d",
					codec_id, codec_properties_length);
			self->client_info.rfx_codec_id = codec_id;
			i1 = MIN(64, codec_properties_length);
			g_memcpy(self->client_info.rfx_prop, s->p, i1);
			self->client_info.rfx_prop_len = i1;
		} else if (g_memcmp(codec_guid, XR_CODEC_GUID_JPEG, 16) == 0) {
			g_writeln("xrdp_caps_process_codecs: jpeg codec id %d prop len %d",
					codec_id, codec_properties_length);
			self->client_info.jpeg_codec_id = codec_id;
			i1 = MIN(64, codec_properties_length);
			g_memcpy(self->client_info.jpeg_prop, s->p, i1);
			self->client_info.jpeg_prop_len = i1;
			/* make sure that requested quality is  between 0 to 100 */
			if (self->client_info.jpeg_prop[0] < 0
					|| self->client_info.jpeg_prop[0] > 100) {
				g_writeln(
						"  Warning: the requested jpeg quality (%d) is invalid,"
								" falling back to default",
						self->client_info.jpeg_prop[0]);
				self->client_info.jpeg_prop[0] = 75; /* use default */
			}
			g_writeln("  jpeg quality set to %d",
					self->client_info.jpeg_prop[0]);
		} else if (g_memcmp(codec_guid, XR_CODEC_GUID_H264, 16) == 0) {
			g_writeln("xrdp_caps_process_codecs: h264 codec id %d prop len %d",
					codec_id, codec_properties_length);
			self->client_info.h264_codec_id = codec_id;
			i1 = MIN(64, codec_properties_length);
			g_memcpy(self->client_info.h264_prop, s->p, i1);
			self->client_info.h264_prop_len = i1;
		} else {
			g_writeln("xrdp_caps_process_codecs: unknown codec id %d",
					codec_id);
		}

		s->p = next_guid;
	}

	return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_caps_process_multifragmetupdate(struct xrdp_rdp *self, struct stream *s,
		int len) {
	int MaxRequestSize;

	in_uint32_le(s, MaxRequestSize);
	self->client_info.max_fastpath_frag_bytes = MaxRequestSize;
	return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_caps_process_frame_ack(struct xrdp_rdp *self, struct stream *s, int len) {
	g_writeln("xrdp_caps_process_frame_ack:");
	self->client_info.use_frame_acks = 1;
	in_uint32_le(s, self->client_info.max_unacknowledged_frame_count);
	g_writeln("  max_unacknowledged_frame_count %d",
			self->client_info.max_unacknowledged_frame_count);
	return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_caps_process_confirm_active(struct xrdp_rdp *self, struct stream *s) {
	int cap_len;
	int source_len;
	int num_caps;
	int index;
	int type;
	int len;
	char *p;

	DEBUG("in xrdp_caps_process_confirm_active");
	in_uint8s(s, 4); /* rdp_shareid */
	in_uint8s(s, 2); /* userid */
	in_uint16_le(s, source_len); /* sizeof RDP_SOURCE */
	in_uint16_le(s, cap_len);
	in_uint8s(s, source_len);
	in_uint16_le(s, num_caps);
	in_uint8s(s, 2); /* pad */

	for (index = 0; index < num_caps; index++) {
		p = s->p;
		if (!s_check_rem(s, 4)) {
			g_writeln("xrdp_caps_process_confirm_active: error 1");
			return 1;
		}
		in_uint16_le(s, type);
		in_uint16_le(s, len);
		if ((len < 4) || !s_check_rem(s, len - 4)) {
			g_writeln("xrdp_caps_process_confirm_active: error len %d", len,
					s->end - s->p);
			return 1;
		}
		len -= 4;
		switch (type) {
		case RDP_CAPSET_GENERAL: /* 1 */
			DEBUG("RDP_CAPSET_GENERAL");
			xrdp_caps_process_general(self, s, len);
			break;
		case RDP_CAPSET_BITMAP: /* 2 */
			DEBUG("RDP_CAPSET_BITMAP");
			xrdp_caps_process_bitmap(self, s, len);
			break;
		case RDP_CAPSET_ORDER: /* 3 */
			DEBUG("RDP_CAPSET_ORDER");
			xrdp_caps_process_order(self, s, len);
			break;
		case RDP_CAPSET_BMPCACHE: /* 4 */
			DEBUG("RDP_CAPSET_BMPCACHE");
			xrdp_caps_process_bmpcache(self, s, len);
			break;
		case RDP_CAPSET_CONTROL: /* 5 */
			DEBUG("RDP_CAPSET_CONTROL");
			break;
		case 6:
			xrdp_caps_process_cache_v3_codec_id(self, s, len);
			break;
		case RDP_CAPSET_ACTIVATE: /* 7 */
			DEBUG("RDP_CAPSET_ACTIVATE");
			;
			break;
		case RDP_CAPSET_POINTER: /* 8 */
			DEBUG("RDP_CAPSET_POINTER");
			xrdp_caps_process_pointer(self, s, len);
			break;
		case RDP_CAPSET_SHARE: /* 9 */
			DEBUG("RDP_CAPSET_SHARE")
			;
			break;
		case RDP_CAPSET_COLCACHE: /* 10 */
			DEBUG("RDP_CAPSET_COLCACHE")
			;
			break;
		case 12: /* 12 */
			DEBUG("--12")
			;
			break;
		case 13: /* 13 */
			xrdp_caps_process_input(self, s, len);
			break;
		case CAPSTYPE_FONT:
			DEBUG("CAPSTYPE_FONT")
			;
			break;
		case RDP_CAPSET_BRUSHCACHE: /* 15 */
			xrdp_caps_process_brushcache(self, s, len);
			break;
		case 16: /* 16 */
			DEBUG("--16")
			;
			break;
		case 17: /* 17 */
			DEBUG("CAPSET_TYPE_OFFSCREEN_CACHE")
			;
			xrdp_caps_process_offscreen_bmpcache(self, s, len);
			break;
		case RDP_CAPSET_BMPCACHE2:
			DEBUG("RDP_CAPSET_BMPCACHE2")
			;
			xrdp_caps_process_bmpcache2(self, s, len);
			break;
		case CAPSTYPE_VIRTUALCHANNEL:
			DEBUG("CAPSTYPE_VIRTUALCHANNEL");

			break;
		case 21: /* 21 */
			DEBUG("--21")
			;
			break;
		case 22: /* 22 */
			DEBUG("--22")
			;
			break;
		case 0x0017: /* 23 CAPSETTYPE_RAIL */
			xrdp_caps_process_rail(self, s, len);
			break;
		case 0x0018: /* 24 CAPSETTYPE_WINDOW */
			xrdp_caps_process_window(self, s, len);
			break;
		case 0x001A: /* 26 CAPSETTYPE_MULTIFRAGMENTUPDATE */
			xrdp_caps_process_multifragmetupdate(self, s, len);
			break;
		case CAPSETTYPE_SURFACE_COMMANDS:
			xrdp_caps_process_surfacecommand(self, s, len);
			break;
		case RDP_CAPSET_BMPCODECS: /* 0x1d(29) */
			xrdp_caps_process_codecs(self, s, len);
			break;
		case 0x001E: /* CAPSSETTYPE_FRAME_ACKNOWLEDGE */
			xrdp_caps_process_frame_ack(self, s, len);
			break;
		default:
			g_writeln("unknown in xrdp_caps_process_confirm_active %d", type);
			break;
		}

		s->p = p + len + 4;
	}

	DEBUG("out xrdp_caps_process_confirm_active");
	return 0;
}
/*****************************************************************************/
int APP_CC
xrdp_caps_send_demand_active(struct xrdp_rdp *self) {
	struct stream *s;
	int caps_count;
	int caps_size;
	int codec_caps_count;
	int codec_caps_size;
	int flags;
	char *caps_count_ptr;
	char *lengthSourceDescriptor_ptr;
	char *caps_ptr;
	char *codec_caps_count_ptr;
	char *codec_caps_size_ptr;

	make_stream(s);
	init_stream(s, 8192);

	DEBUG("------------ in xrdp_caps_send_demand_active\n\n");

	if (xrdp_rdp_init(self, s) != 0) {
		free_stream(s);
		return 1;
	}

	caps_count = 0;
	// shareId
	out_uint32_le(s, self->share_id);
	// lengthSourceDescriptor
	out_uint16_le(s, 4); /* 4 chars for RDP\0 */
	/* 2 bytes size after num caps, set later */
	lengthSourceDescriptor_ptr = s->p;

	out_uint8s(s, 2);
	out_uint8a(s, "RDP", 4);
	/* 4 byte num caps, set later */
	caps_count_ptr = s->p;
	out_uint8s(s, 4);
	caps_ptr = s->p;

	//-- start of capability sets

	// Capability Set (TS_CAPS_SET)
	caps_count++;
	// capabilitySetType
	out_uint16_le(s, CAPSTYPE_SHARE);
	// lengthCapability : size of the TS_CAPS_SET
	out_uint16_le(s, 8);
	// nodeId
	out_uint16_le(s, self->mcs_channel);
	// pad2octets
	out_uint16_le(s, 0x0000);

	// General Capability Set (TS_GENERAL_CAPABILITYSET)
	caps_count++;
	// capabilitySetType
	out_uint16_le(s, CAPSTYPE_GENERAL);
	// lengthCapability
	out_uint16_le(s, 24);
	// osMajorType
	out_uint16_le(s, OSMAJORTYPE_WINDOWS);
	// osMinorType
	out_uint16_le(s, OSMINORTYPE_WINDOWS_NT);
	// protocolVersion
	out_uint16_le(s, TS_CAPS_PROTOCOLVERSION);
	// pad2octetsA
	out_uint16_le(s, 0);
	// generalCompressionTypes
	out_uint16_le(s, 0);
	// extraFlags
	if (self->client_info.use_fast_path & 1) {
		out_uint16_le(s, FASTPATH_OUTPUT_SUPPORTED + NO_BITMAP_COMPRESSION_HDR);
	} else {
		out_uint16_le(s, NO_BITMAP_COMPRESSION_HDR);
	}
	// updateCapabilityFlag
	out_uint16_le(s, 0);
	// remoteUnshareFlag
	out_uint16_le(s, 0);
	// generalCompressionLevel
	out_uint16_le(s, 0);
	// refreshRectSupport
	out_uint8(s, FALSE);
	// suppressOutputSupport
	out_uint8(s, FALSE);

	// FIXME : doit être rensigné pour ChromeRDP
	caps_count++;
	out_uint16_le(s, CAPSTYPE_VIRTUALCHANNEL);
	out_uint16_le(s, 12);
	out_uint32_le(s, 2); //!!
	out_uint32_le(s, 1600);

	if (1) {
		// TODO : voir pourquoi
		caps_count++;
		out_uint16_le(s, CAPSTYPE_DRAWGDIPLUS);
		out_uint16_le(s, 40);
		out_uint32_le(s, 1); //!!
		out_uint16_le(s, 0xFFFF);
		out_uint16_le(s, 0xFFFF);
		out_uint32_le(s, 1);
		out_uint32_le(s, 0);
		out_uint32_le(s, 0);
		out_uint32_le(s, 0);
		out_uint32_le(s, 0);
		out_uint32_le(s, 0);
		out_uint32_le(s, 0);
	}
	// Capability Set (TS_FONT_CAPABILITYSET)
	caps_count++;
	out_uint16_le(s, CAPSTYPE_FONT);
	out_uint16_le(s, 8);
	out_uint16_le(s, FONTSUPPORT_FONTLIST);
	out_uint16_le(s, 0);
	//}

	// Bitmap Capability Set (TS_BITMAP_CAPABILITYSET)
	caps_count++;
	// capabilitySetType
	out_uint16_le(s, CAPSTYPE_BITMAP);
	// lengthCapability
	out_uint16_le(s, 28);
	// preferredBitsPerPixel
	out_uint16_le(s, self->client_info.bpp);
	// receive1BitPerPixel
	out_uint16_le(s, TRUE);
	// receive4BitsPerPixel
	out_uint16_le(s, TRUE);
	// receive8BitsPerPixel
	out_uint16_le(s, TRUE);
	// desktopWidth
	out_uint16_le(s, self->client_info.width);
	// desktopHeight
	out_uint16_le(s, self->client_info.height);
	// pad2octets
	out_uint16_le(s, 0);
	// desktopResizeFlag
	out_uint16_le(s, TRUE);
	// bitmapCompressionFlag
	out_uint16_le(s, TRUE);
	// highColorFlags
	out_uint8(s, 0);
	// drawingFlags
	unsigned char drawingFlags = 0; // TODO : supports 32 bits...
	out_uint8(s, drawingFlags);
	// multipleRectangleSupport
	out_uint16_le(s, TRUE);
	// pad2octetsB
	out_uint16_le(s, 0);

	// Order Capability Set (TS_ORDER_CAPABILITYSET)
	caps_count++;
	// capabilitySetType
	out_uint16_le(s, CAPSTYPE_ORDER);
	// lengthCapability
	out_uint16_le(s, 88);
	// terminalDescriptor
	out_uint8s(s, 16);
	// pad4octetsA
	out_uint32_be(s, 0x000000);
	// desktopSaveXGranularity
	out_uint16_le(s, 1);
	// desktopSaveYGranularity
	out_uint16_le(s, 20);
	// pad2octetsA
	out_uint16_le(s, 0);
	// maximumOrderLevel
	out_uint16_le(s, ORD_LEVEL_1_ORDERS);
	// numberFonts
	out_uint16_le(s, 0);
	// orderFlags
	// TODO : look at other flags
	out_uint16_le(s, NEGOTIATEORDERSUPPORT + COLORINDEXSUPPORT);
	/* caps */
	// TODO : add support to orther things
	out_uint8(s, 1); /* NEG_DSTBLT_INDEX                0x00 0 */
	out_uint8(s, 1); /* NEG_PATBLT_INDEX                0x01 1 */
	out_uint8(s, 1); /* NEG_SCRBLT_INDEX                0x02 2 */
	out_uint8(s, 1); /* NEG_MEMBLT_INDEX                0x03 3 */
	int hack = 0;
	out_uint8(s, hack); /* TS_NEG_MEM3BLT_INDEX            0x04 4 */
	out_uint8(s, 0); /* UnusedIndex1	                0x05 5 */
	out_uint8(s, 0); /* UnusedIndex2		            0x06 6 */
	out_uint8(s, hack); /* NEG_DRAWNINEGRID_INDEX          0x07 7 */

	out_uint8(s, hack); /* NEG_LINETO_INDEX                0x08 8 */
	out_uint8(s, hack); /* NEG_MULTI_DRAWNINEGRID_INDEX    0x09 9 */
	out_uint8(s, 0); /* UnusedIndex3	                0x0A 10 */
	out_uint8(s, hack); /* NEG_SAVEBITMAP_INDEX            0x0B 11 */
	out_uint8(s, 0); /* UnusedIndex4      	            0x0C 12 */
	out_uint8(s, 0); /* UnusedIndex5      			    0x0D 13 */

	out_uint8(s, 0); /* UnusedIndex6            		0x0E 14 */
	out_uint8(s, hack); /* NEG_MULTIDSTBLT_INDEX           0x0F 15 */
	out_uint8(s, hack); /* NEG_MULTIPATBLT_INDEX           0x10 16 */
	out_uint8(s, hack); /* NEG_MULTISCRBLT_INDEX           0x11 17 */
	out_uint8(s, 1); /* NEG_MULTIOPAQUERECT_INDEX       0x12 18 */
	out_uint8(s, hack); /* NEG_FAST_INDEX_INDEX            0x13 19 */
	out_uint8(s, hack); /* NEG_POLYGON_SC_INDEX            0x14 20 */
	out_uint8(s, hack); /* NEG_POLYGON_CB_INDEX            0x15 21 */
	out_uint8(s, hack); /* NEG_POLYLINE_INDEX              0x16 22 */
	out_uint8(s, 0); /* UnusedIndex7                    0x17 23 */

	out_uint8(s, hack); /* NEG_FAST_GLYPH_INDEX            0x18 24 */
	out_uint8(s, hack); /* NEG_ELLIPSE_SC_INDEX            0x19 25 */
	out_uint8(s, hack); /* NEG_ELLIPSE_CB_INDEX            0x1A 26 */
	out_uint8(s, 1); /* NEG_GLYPH_INDEX_INDEX           0x1B 27 */
	out_uint8(s, 0); /* UnusedIndex8    				0x1C 28 */
	out_uint8(s, 0); /* UnusedIndex9    				0x1D 29 */
	out_uint8(s, 0); /* UnusedIndex10 					0x1E 30 */
	out_uint8(s, 0); /* UnusedIndex11                   0x1F 31 */
	// textFlags
	out_uint16_le(s, 0x06A1);				// FIXMEs set to 0
	// orderSupportExFlags
	// TODO : support ORDERFLAGS_EX_ALTSEC_FRAME_MARKER_SUPPORT
	out_uint16_le(s, ORDERFLAGS_EX_CACHE_BITMAP_REV3_SUPPORT);
	// pad4octetsB
	out_uint32_le(s, 1000 * 1000);
	// desktopSaveSize
	out_uint32_le(s, 1000 * 1000);
	// pad2octetsC
	out_uint16_le(s, 1);
	// pad2octetsD
	out_uint16_le(s, 0);
	// textANSICodePage
	out_uint16_le(s, 0);
	// pad2octetsE
	out_uint16_le(s, 0);

	// Color Table Cache Capability Set (TS_COLORTABLE_CAPABILITYSET)
	caps_count++;
	// capabilitySetType
	out_uint16_le(s, CAPSTYPE_COLORCACHE);
	// lengthCapability
	out_uint16_le(s, 8);
	// colorTableCacheSize
	out_uint16_le(s, 0x0006);
	// pad2octets
	out_uint16_le(s, 0);

	// FIXME:
	caps_count++;
	// capabilitySetType
	out_uint16_le(s, CAPSTYPE_BITMAPCACHE_HOSTSUPPORT);
	// lengthCapability
	out_uint16_le(s, 8);
	out_uint16_le(s, 1);
	out_uint16_le(s, 0);

	if (0) {
		// Bitmap Codecs Capability Set (TS_BITMAPCODECS_CAPABILITYSET)
		caps_count++;
		// capabilitySetType
		out_uint16_le(s, CAPSETTYPE_BITMAP_CODECS);
		// lengthCapability
		codec_caps_size_ptr = s->p;
		out_uint8s(s, 2); /* 2 bytes : cap len set later */

		// bitmapCodecCount
		codec_caps_count = 0;
		codec_caps_count_ptr = s->p;
		out_uint8s(s, 1); /* bitmapCodecCount set later */

		// NSCODEC
		codec_caps_count++;
		// codecGUID
		out_uint8a(s, XR_CODEC_GUID_NSCODEC, 16);
		// codecID
		out_uint8(s, 1);
		// codecPropertiesLength
		out_uint16_le(s, 3);
		// properties : NSCodec Capability Set (TS_NSCODEC_CAPABILITYSET)
		out_uint8(s, TRUE);				// fAllowDynamicFidelity
		out_uint8(s, TRUE);				// fAllowSubsampling
		out_uint8(s, 3);				// colorLossLevel
		/* REMOTEFX */
		codec_caps_count++;
		// codecGUID
		out_uint8a(s, XR_CODEC_GUID_REMOTEFX, 16);
		// codecID
		out_uint8(s, 0); /* codec id, client sets */
		// TS_RFX_SRVR_CAPS_CONTAINER (256 bytes set to O)
		out_uint16_le(s, 256);
		out_uint8s(s, 256);

		// TODO : CODEC_GUID_IMAGE_REMOTEFX
		/* jpeg */
		// FIXME : Jpeg removed because not in MS specification
		// codec_caps_count++;
		// out_uint8a(s, XR_CODEC_GUID_JPEG, 16);
		// out_uint8(s, 0); /* codec id, client sets */
		// out_uint16_le(s, 1); /* ext length */
		// out_uint8(s, 75);
		/* calculate and set size and count */
		codec_caps_size = (int) (s->p - codec_caps_size_ptr);
		codec_caps_size += 2; /* 2 bytes for RDP_CAPSET_BMPCODECS above */
		codec_caps_size_ptr[0] = codec_caps_size;
		codec_caps_size_ptr[1] = codec_caps_size >> 8;
		codec_caps_count_ptr[0] = codec_caps_count;

	}

	// Pointer Capability Set (TS_POINTER_CAPABILITYSET)
	caps_count++;
	out_uint16_le(s, CAPSTYPE_POINTER);
	out_uint16_le(s, 10);
	// colorPointerFlag
	out_uint16_le(s, TRUE);
	// colorPointerCacheSize
	out_uint16_le(s, 25);
	//	pointerCacheSize
	out_uint16_le(s, 25); /* Cache size */

	// FIXME
	caps_count++;
	// capabilitySetType
	out_uint16_le(s, CAPSETTYPE_LARGE_POINTER);
	out_uint16_le(s, 6);
	out_uint16_le(s, 1);

	// Input Capability Set (TS_INPUT_CAPABILITYSET)
	caps_count++;
	// capabilitySetType
	out_uint16_le(s, CAPSTYPE_INPUT);
	// lengthCapability
	out_uint16_le(s, 88);
	// inputsFlags
	flags = INPUT_FLAG_SCANCODES | INPUT_FLAG_MOUSEX;
	// TODO : add other imputs support
	if (self->client_info.use_fast_path & 2) {
		flags |= INPUT_FLAG_FASTPATH_INPUT | INPUT_FLAG_FASTPATH_INPUT2;
	}
	out_uint16_le(s, flags);
	// FIXME : but W7 report 0 to : keyboardLayout, keyboardType , keyboardSubType, keyboardFunctionKey, imeFileName
	out_uint8s(s, 82);

	// Remote Programs Capability Set
	caps_count++;
	// capabilitySetType
	out_uint16_le(s, CAPSTYPE_RAIL);
	// lengthCapability
	out_uint16_le(s, 8);
	// railSupportLevel : TS_RAIL_LEVEL_SUPPORTED and TS_RAIL_LEVEL_DOCKED_LANGBAR_SUPPORTED
	// TODO : TS_RAIL_LEVEL_LANGUAGE_IME_SYNC_SUPPORTED
	out_uint32_le(s, 3);

	/* Window List Capability Set */
	caps_count++;
	// capabilitySetType
	out_uint16_le(s, CAPSTYPE_WINDOW);
	// lengthCapability
	out_uint16_le(s, 11);
	// wndSupportLevel
	out_uint32_le(s, TS_WINDOW_LEVEL_SUPPORTED_EX);
	// numIconCaches
	out_uint8(s, 3);
	// NumIconCacheEntries
	out_uint16_le(s, 12);

	// FIXME
	caps_count++;
	// capabilitySetType
	out_uint16_le(s, CAPSETTYPE_COMPDESK);
	// lengthCapability
	out_uint16_le(s, 6);
	out_uint16_le(s, 1);

	/* 6 - bitmap cache v3 codecid */
	// Unsupported by Microsoft
//	caps_count++;
//	out_uint16_le(s, 0x0006);
//	out_uint16_le(s, 5);
//	out_uint8(s, 0); /* client sets */
	if (self->client_info.use_fast_path & 1) /* fastpath output on */
	{
		caps_count++;
		// capabilitySetType
		out_uint16_le(s, CAPSETTYPE_MULTIFRAGMENTUPDATE);
		// lengthCapability
		out_uint16_le(s, 8);
		// MaxRequestSize 38063//(2 MBs)
		out_uint32_le(s, 38063 /*2 * 1024 * 1024*/);
	}

	// FIXME :
	caps_count++;
	// capabilitySetType
	out_uint16_le(s, CAPSETTYPE_SURFACE_COMMANDS);
	// lengthCapability
	out_uint16_le(s, 0x0C);
	out_uint16_le(s, 0x52);
	out_uint16_le(s, 0);
	out_uint16_le(s, 0);
	out_uint16_le(s, 0);

	// TS_FRAME_ACKNOWLEDGE_CAPABILITYSET
	caps_count++;
	// capabilitySetType
	out_uint16_le(s, CAPSETTYPE_FRAME_ACKNOWLEDGE);
	// lengthCapability
	out_uint16_le(s, 8);
	// maxUnacknowledgedFrameCount
	out_uint32_le(s, 2);

	//-- end of capability sets

	// sessionId
	out_uint32_le(s, 0);

	s_mark_end(s);

	caps_size = (int) (s->end - caps_ptr);

	lengthSourceDescriptor_ptr[0] = caps_size;
	lengthSourceDescriptor_ptr[1] = caps_size >> 8;

	caps_count_ptr[0] = caps_count;
	caps_count_ptr[1] = caps_count >> 8;
	caps_count_ptr[2] = caps_count >> 16;
	caps_count_ptr[3] = caps_count >> 24;

	if (xrdp_rdp_send(self, s, RDP_PDU_DEMAND_ACTIVE) != 0) {

		free_stream(s);
		return 1;
	}
	DEBUG("out (1) xrdp_caps_send_demand_active");

	/* send Monitor Layout PDU for dual monitor */
	if (self->client_info.monitorCount > 0 && self->client_info.multimon == 1) {
		DEBUG("xrdp_caps_send_demand_active: sending monitor layout pdu");
		if (xrdp_caps_send_monitorlayout(self) != 0) {
			g_writeln(
					"xrdp_caps_send_demand_active: error sending monitor layout pdu");
		}
	}

	free_stream(s);
	DEBUG("out (2) xrdp_caps_send_demand_active");
	return 0;
}
