/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2018 defog <<psnk42@gmail.com>>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-recsign
 *
 * FIXME: Simple Gstreamer plugin to add a blinking REC sign to a pipeline
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! recsign ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "gstrecsign.h"
#include <malloc.h>
#include <stdlib.h>

GST_DEBUG_CATEGORY_STATIC (gst_rec_sign_debug);
#define GST_CAT_DEFAULT gst_rec_sign_debug

/* Filter signals and args */
enum
{
    /* FILL ME */
    LAST_SIGNAL
};

enum
{
    PROP_0,
    PROP_SILENT,
    PROP_SHOW
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
      GST_STATIC_CAPS( \
        "video/x-raw, " \
        "format = (string) " GST_RECSIGN_VIDEO_FORMATS ", " \
        "width = (int) [ 64, MAX ], " \
        "height = (int) [ 64, MAX ], " \
        "framerate = (fraction) [ 0, MAX ]; " \
      )
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
      GST_STATIC_CAPS( \
        "video/x-raw, " \
        "format = (string) " GST_RECSIGN_VIDEO_FORMATS ", " \
        "width = (int) [ 64, MAX ], " \
        "height = (int) [ 64, MAX ], " \
        "framerate = (fraction) [ 0, MAX ]; " \
      )
    );

#define gst_rec_sign_parent_class parent_class
G_DEFINE_TYPE (GstRecSign, gst_rec_sign, GST_TYPE_ELEMENT);

static void gst_rec_sign_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_rec_sign_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_rec_sign_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static GstFlowReturn gst_rec_sign_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);


static gint width, height;
static gint fr_numerator, fr_denominator;
static gint framerate;
static gint circleDiameter, circleRadius;
static guint8 *circleArray;
static gint circleArraySize;
static guint8 y, u, v;

/*
    Y = Kr*R + Kg*G + Kb*B
    V = (R-Y)/(1-Kr) = R - G * Kg/(1-Kr) - B * Kb/(1-Kr)
    U = (B-Y)/(1-Kb) = - R * Kr/(1-Kb) - G * Kg/(1-Kb) + B

    coefficients					Rec.601	Rec.709	FCC
    Kr : Red channel coefficient	0.299	0.2126	0.3
    Kg : Green channel coefficient	0.587	0.7152	0.59
    Kb : Blue channel coefficient	0.114	0.0722	0.11
*/
/* Choose only one color palette */
#define REC_601
//#define REC_709
//#define FCC

#if (defined REC_601 || defined REC_709) && (defined REC_601 || defined FCC) && (defined REC_709 || defined FCC)
#error Choose only ONE color palette!
#endif
#if !(defined REC_601 || defined REC_709 || defined FCC)
#error Choose at least one color palette!
#endif

#ifdef REC_601
    #define KR	0.299
    #define KG	0.587
    #define KB	0.114
#elif defined REC_709
    #define KR	0.2126
    #define KG	0.7152
    #define KB	0.0722
#else
   #define KR	0.3
    #define KG	0.59
    #define KB	0.11
#endif

// Color conversion from RGB[0,255] to YUV[0,255]
static void __attribute_used__ rgb2i420_0_255 (guint8 r, guint8 g, guint8 b, guint8 *y, guint8 *u, guint8 *v)
{
    gfloat  fr = (gfloat)(r)/255.0,
            fg = (gfloat)(g)/255.0,
            fb = (gfloat)(b)/255.0;

    gfloat  fy = KR*fr + KG*fg + KB*fb,
            fu = (fr-fy)/(1.0-KR),
            fv = (fb-fy)/(1.0-KB);

    *y = (guint8)(fy*255.0);
    *u = (guint8)(fu*127.5 + 128.0);
    *v = (guint8)(fv*127.5 + 128.0);
}

// Color conversion from RGB[0,255] to YUV[16,235]
static void __attribute_used__ rgb2i420_16_235 (guint8 r, guint8 g, guint8 b, guint8 *y, guint8 *u, guint8 *v)
{
    gfloat  fr = (gfloat)(r)/255.0,
            fg = (gfloat)(g)/255.0,
            fb = (gfloat)(b)/255.0;

    gfloat  fy = KR*fr + KG*fg + KB*fb,
            fu = (fr-fy)/(1.0-KR),
            fv = (fb-fy)/(1.0-KB);

    *y = (guint8)(fy*219.0 + 16.0);
    *u = (guint8)(fu*112.0 + 128.0);
    *v = (guint8)(fv*112.0 + 128.0);
}

#undef REC_601
#undef REC_709
#undef FCC

/* GObject vmethod implementations */

/* initialize the recsign's class */
static void
gst_rec_sign_class_init (GstRecSignClass * klass)
{
    GObjectClass *gobject_class;
    GstElementClass *gstelement_class;

    gobject_class = (GObjectClass *) klass;
    gstelement_class = (GstElementClass *) klass;

    gobject_class->set_property = gst_rec_sign_set_property;
    gobject_class->get_property = gst_rec_sign_get_property;

    g_object_class_install_property (gobject_class, PROP_SILENT,
        g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
        TRUE, G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class, PROP_SHOW,
        g_param_spec_boolean ("show", "Show", "Whether REC sign is shown or not",
        TRUE, G_PARAM_READWRITE));

    gst_element_class_set_details_simple(gstelement_class,
        "RecSign",
        "Video/Filter",
        "Rec sign element",
        "defog <<psnk42@gmail.com>>");

    gst_element_class_add_pad_template (gstelement_class,
        gst_static_pad_template_get (&src_factory));
    gst_element_class_add_pad_template (gstelement_class,
        gst_static_pad_template_get (&sink_factory));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_rec_sign_init (GstRecSign * filter)
{
    filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
    gst_pad_set_event_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_rec_sign_sink_event));
    gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_rec_sign_chain));
    GST_PAD_SET_PROXY_CAPS (filter->sinkpad);
    gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

    filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
    GST_PAD_SET_PROXY_CAPS (filter->srcpad);
    gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

    filter->silent = TRUE;
    filter->show = TRUE;
}

static void
gst_rec_sign_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRecSign *filter = GST_RECSIGN (object);

    switch (prop_id) {
        case PROP_SILENT:
          filter->silent = g_value_get_boolean (value);
          break;
        case PROP_SHOW:
          filter->show = g_value_get_boolean (value);
          break;
        default:
          G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          break;
    }
}

static void
gst_rec_sign_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
    GstRecSign *filter = GST_RECSIGN (object);

    switch (prop_id) {
        case PROP_SILENT:
          g_value_set_boolean (value, filter->silent);
          break;
        case PROP_SHOW:
          g_value_set_boolean (value, filter->show);
          break;
        default:
          G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          break;
    }
}

/* GstElement vmethod implementations */

/* this function handles sink events */
static gboolean
gst_rec_sign_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
    GstRecSign *filter;
    gboolean ret;

    filter = GST_RECSIGN (parent);

    GST_LOG_OBJECT (filter, "Received %s event: %" GST_PTR_FORMAT,
    GST_EVENT_TYPE_NAME (event), event);

    switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
        {
            GstCaps * caps;

            gst_event_parse_caps (event, &caps);
            /* do something with the caps */

            GstStructure *str = gst_caps_get_structure(caps, 0);
            gst_structure_get_int(str, "height", &height);
            gst_structure_get_int(str, "width", &width);
            gst_structure_get_fraction(str, "framerate", &fr_numerator, &fr_denominator);

            framerate = fr_numerator/fr_denominator;
            circleDiameter = width/32;
            circleRadius = circleDiameter/2;
            circleArraySize = circleDiameter*circleDiameter;

            circleArray = (guint8*)malloc(circleArraySize+1);
            if (circleArray == NULL) {
                g_print("Unable allocate %d bytes. Exit...\n", circleArraySize);
                exit(-1);
            } else {
                g_print("Allocated %d bytes\n", circleArraySize);
            }

            gint circleRadiusSquared = (circleRadius)*(circleRadius);
            gint cx=-circleRadius, cy=-circleRadius;

            // Calculate circle pixels
            for (guint cnt=0; cnt<circleArraySize; cnt++) {
                // x^2 + y^2 = r^2
                if ((cx*cx + cy*cy) >= circleRadiusSquared) {
                    circleArray[cnt] = 0;
                } else {
                    circleArray[cnt] = 1;
                }

                if (++cx == circleRadius) {
                    cx = -circleRadius;
                    if (++cy == circleRadius) {

                    }
                }
            }

            // Calculate YUV values for red circle
            rgb2i420_16_235(255,0,0,&y,&u,&v);

            // Print CAPs if not in silent mode
            if (filter->silent == FALSE) {
                g_print("Caps: %s\n", gst_caps_to_string(caps));
                g_print("Height=%d\n", height);
                g_print("Width=%d\n", width);
                g_print("Framerate=%d\n", framerate);
            }

            /* and forward */
            ret = gst_pad_event_default (pad, parent, event);
            break;
        }
    case GST_EVENT_EOS:
        /* end-of-stream, we should close down all stream leftovers here */
        if (circleArray != NULL) {
          free(circleArray);
        }
        ret = gst_pad_event_default (pad, parent, event);
        break;
    default:
        ret = gst_pad_event_default (pad, parent, event);
        break;
    }
    return ret;
}
/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_rec_sign_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
    GstRecSign *filter;
    GstMapInfo map;
    static glong framecount = 0;

    filter = GST_RECSIGN (parent);

    if (++framecount % (framerate) < (framerate/2)) {
        buf = gst_buffer_make_writable(buf);
        if (buf == NULL) {
            return gst_pad_push (filter->srcpad, buf);
        }
        if (gst_buffer_map(buf, &map, GST_MAP_WRITE)) {
            guint h, w;
            guint8 *ptrChroma, *ptrLumaU, *ptrLumaV;

            // Magic with colorspace addresses
            ptrChroma = (guint8 *)map.data + width*circleDiameter + circleDiameter;
            ptrLumaU  = (guint8 *)map.data + width*height + width*circleRadius/2 + width*height/4 + circleRadius;
            ptrLumaV  = (guint8 *)map.data + width*height + width*circleRadius/2 + circleRadius;

            // Draw actual circle
            for (h=0; h<circleDiameter; h++) {
                for (w=0; w<circleDiameter; w++) {
                    if (circleArray[h*circleDiameter + w]) {
                        ptrChroma[h*width + w] = y;
                        ptrLumaU[h/2*width/2 + w/2 +  0] = u;
                        ptrLumaV[h/2*width/2 + w/2 +  0] = v;
                    }
                }
            }
        gst_buffer_unmap(buf, &map);
        }
    }
    
    return gst_pad_push (filter->srcpad, buf);
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
recsign_init (GstPlugin * recsign)
{
    /* debug category for fltering log messages
    *
    * exchange the string 'Template recsign' with your description
    */
    GST_DEBUG_CATEGORY_INIT (gst_rec_sign_debug, "recsign",
        0, "Template recsign");

    return gst_element_register (recsign, "recsign", GST_RANK_NONE,
        GST_TYPE_RECSIGN);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "recsign"
#endif

/* gstreamer looks for this structure to register recsigns
 *
 * exchange the string 'Template recsign' with your recsign description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    recsign,
    "Shows a blinking REC sign on a video feed",
    recsign_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
