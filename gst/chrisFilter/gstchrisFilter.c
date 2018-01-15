/* GStreamer
 * Copyright (C) <2018> Chris <ChrisCodeHub@hotmail.com>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "gstchrisFilter.h"


enum
{
  PROP_0,
  PROP_ACTIVE,
  PROP_TOLERANCE,
  PROP_FILTER_SIZE,
  PROP_LUMA_ONLY
};


// need to define the input and the output pads.  Sink is the output
// src is the input.  Since this filter can create its pads when its
// stamped out, they are ALWAYS pads
// For now only support "broadcast" formats of data, ie I420 STATIC CAPs only required
// TODO (later) - add the support for a 4:2:2 format and a 10bit as a nod to the Main10 HEVC standard
// This blob is just a simple 1 in ->-> 1 out pin entity 

static GstStaticPadTemplate gst_chrisFilter_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("I420")));

static GstStaticPadTemplate gst_chrisFilter_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("I420")));


// add forward declarations for the boiler plate functions
static gboolean gst_chrisFilter_set_info (GstVideoFilter * filter,
    GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info);
static GstFlowReturn gst_chrisFilter_transform_frame (GstVideoFilter *
    vfilter, GstVideoFrame * in_frame, GstVideoFrame * out_frame);
static void gst_chrisFilter_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_chrisFilter_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

// not sure what this does TODO - find out!
G_DEFINE_TYPE (GstchrisFilter, gst_chrisFilter, GST_TYPE_VIDEO_FILTER);


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void
gst_chrisFilter_class_init (GstchrisFilterClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstVideoFilterClass *vfilter_class = (GstVideoFilterClass *) klass;

  gobject_class->set_property = gst_chrisFilter_set_property;
  gobject_class->get_property = gst_chrisFilter_get_property;

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_ACTIVE,
      g_param_spec_boolean ("active", "active",
          "process video", TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_TOLERANCE,
      g_param_spec_int ("tolerance",
          "tolerance",
          "contrast tolerance for smoothing",
          G_MININT, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_FILTER_SIZE,
      g_param_spec_int ("filter-size",
          "filter-size",
          "size of media filter",
          G_MININT, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_LUMA_ONLY,
      g_param_spec_boolean ("luma-only",
          "luma-only",
          "only filter luma part",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  // lets get the pads connected
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_chrisFilter_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_chrisFilter_src_template);
  gst_element_class_set_static_metadata (gstelement_class,
      "Chris Filter effect",
      "Filter/Effect/Video",
      "do stuff to a I420 image", "Chris <ChrisCodeHub@hotmail.com>");

  // No idea what binding these are carrrying out either - TODO understand these bindings
  vfilter_class->transform_frame =
      GST_DEBUG_FUNCPTR (gst_chrisFilter_transform_frame);
  vfilter_class->set_info = GST_DEBUG_FUNCPTR (gst_chrisFilter_set_info);
}



// grabbing paramater from the input pin to setup this block accordingly
static gboolean
gst_chrisFilter_set_info (GstVideoFilter * filter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstchrisFilter *chrisFilter;

  chrisFilter = GST_CHRISFILTER (filter);

  chrisFilter->width = GST_VIDEO_INFO_WIDTH (in_info);
  chrisFilter->height = GST_VIDEO_INFO_HEIGHT (in_info);

  return TRUE;
}

// initialise the variables to sensible defaultws
static void
gst_chrisFilter_init (GstchrisFilter * chrisFilter)
{
  chrisFilter->active = FALSE;
  chrisFilter->tolerance = 8;
  chrisFilter->filtersize = 3;
  chrisFilter->luma_only = TRUE;
}

//##########################################################################################
// Actually do some manipulaion of the video pixels we have been given
// this is take from the gstsmooth filter code as a starting point
// TODO - make this a 3x3 gaussian with programable sigma

static void
smooth_filter (guchar * dest, guchar * src, gint width, gint height,
    gint stride, gint dstride, gint tolerance, gint filtersize)
{
  gint refval, aktval, upperval, lowerval, numvalues, sum;
  gint x, y, fx, fy, fy1, fy2, fx1, fx2;
  guchar *srcp = src, *destp = dest;

  fy1 = 0;
  fy2 = MIN (filtersize + 1, height) * stride;

  for (y = 0; y < height; y++) {
    if (y > (filtersize + 1))
      fy1 += stride;
    if (y < height - (filtersize + 1))
      fy2 += stride;

    for (x = 0; x < width; x++) {
      refval = *src;
      upperval = refval + tolerance;
      lowerval = refval - tolerance;

      numvalues = 1;
      sum = refval;

      fx1 = MAX (x - filtersize, 0) + fy1;
      fx2 = MIN (x + filtersize + 1, width) + fy1;

      for (fy = fy1; fy < fy2; fy += stride) {
        for (fx = fx1; fx < fx2; fx++) {
          aktval = srcp[fx];
          if ((lowerval - aktval) * (upperval - aktval) < 0) {
            numvalues++;
            sum += aktval;
          }
        }                       /*for fx */
        fx1 += stride;
        fx2 += stride;
      }                         /*for fy */

      src++;
      *dest++ = sum / numvalues;
    }

    src = srcp + stride * y;
    dest = destp + dstride * y;
  }
}

static GstMessage *gst_cutter_message_new (GstchrisFilter * c, gboolean above,
    GstClockTime timestamp);
//###########################################################################################
// to send a message on the message bus to main application - gst_element_post_message() is what the TSparsers are doing
// lifted this code from gst-good/cutter/cutter.c
static GstMessage *
gst_cutter_message_new (GstchrisFilter * c, gboolean above,
    GstClockTime timestamp)
{
  GstStructure *s;

  s = gst_structure_new ("chrisFilter",
      "above", G_TYPE_BOOLEAN, above,
      "timestamp", GST_TYPE_CLOCK_TIME, timestamp, NULL);

  return gst_message_new_element (GST_OBJECT (c), s);
}


//###########################################################################################
// The function that gets called to do "something"
//
//
static GstFlowReturn
gst_chrisFilter_transform_frame (GstVideoFilter * filter,
    GstVideoFrame * in_frame, GstVideoFrame * out_frame)
{
  GstchrisFilter *chrisFilter;
  static guint timesCalled = 0;

  chrisFilter = GST_CHRISFILTER (filter);

  timesCalled++;
  if (timesCalled > 10) {
    g_print ("~");
    timesCalled = 0;
    GstMessage *message = gst_cutter_message_new (chrisFilter, FALSE,
        GST_BUFFER_TIMESTAMP (in_frame));
    gst_element_post_message (GST_ELEMENT (chrisFilter), message);
  }
  // is all of this is not active, just copy data from the input pin to the
  // output pin
  if (!chrisFilter->active) {
    gst_video_frame_copy (out_frame, in_frame);
    return GST_FLOW_OK;
  }

  smooth_filter (GST_VIDEO_FRAME_COMP_DATA (out_frame, 0),
      GST_VIDEO_FRAME_COMP_DATA (in_frame, 0),
      GST_VIDEO_FRAME_COMP_WIDTH (in_frame, 0),
      GST_VIDEO_FRAME_COMP_HEIGHT (in_frame, 0),
      GST_VIDEO_FRAME_COMP_STRIDE (in_frame, 0),
      GST_VIDEO_FRAME_COMP_STRIDE (out_frame, 0),
      chrisFilter->tolerance, chrisFilter->filtersize);

  if (!chrisFilter->luma_only) {
    smooth_filter (GST_VIDEO_FRAME_COMP_DATA (out_frame, 1),
        GST_VIDEO_FRAME_COMP_DATA (in_frame, 1),
        GST_VIDEO_FRAME_COMP_WIDTH (in_frame, 1),
        GST_VIDEO_FRAME_COMP_HEIGHT (in_frame, 1),
        GST_VIDEO_FRAME_COMP_STRIDE (in_frame, 1),
        GST_VIDEO_FRAME_COMP_STRIDE (out_frame, 1),
        chrisFilter->tolerance, chrisFilter->filtersize);

    smooth_filter (GST_VIDEO_FRAME_COMP_DATA (out_frame, 2),
        GST_VIDEO_FRAME_COMP_DATA (in_frame, 2),
        GST_VIDEO_FRAME_COMP_WIDTH (in_frame, 2),
        GST_VIDEO_FRAME_COMP_HEIGHT (in_frame, 2),
        GST_VIDEO_FRAME_COMP_STRIDE (in_frame, 2),
        GST_VIDEO_FRAME_COMP_STRIDE (out_frame, 2),
        chrisFilter->tolerance, chrisFilter->filtersize);
  } else {
    gst_video_frame_copy_plane (out_frame, in_frame, 1);
    gst_video_frame_copy_plane (out_frame, in_frame, 2);
  }

  return GST_FLOW_OK;
}


//###############################################################################
//      allow the element to be configured and checked with getter/setter duo
//
static void
gst_chrisFilter_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstchrisFilter *chrisFilter;

  g_return_if_fail (GST_IS_CHRISFILTER (object));
  chrisFilter = GST_CHRISFILTER (object);

  switch (prop_id) {
    case PROP_ACTIVE:
      chrisFilter->active = g_value_get_boolean (value);
      break;
    case PROP_TOLERANCE:
      chrisFilter->tolerance = g_value_get_int (value);
      break;
    case PROP_FILTER_SIZE:
      chrisFilter->filtersize = g_value_get_int (value);
      break;
    case PROP_LUMA_ONLY:
      chrisFilter->luma_only = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_chrisFilter_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstchrisFilter *chrisFilter;

  g_return_if_fail (GST_IS_CHRISFILTER (object));
  chrisFilter = GST_CHRISFILTER (object);

  switch (prop_id) {
    case PROP_ACTIVE:
      g_value_set_boolean (value, chrisFilter->active);
      break;
    case PROP_TOLERANCE:
      g_value_set_int (value, chrisFilter->tolerance);
      break;
    case PROP_FILTER_SIZE:
      g_value_set_int (value, chrisFilter->filtersize);
      break;
    case PROP_LUMA_ONLY:
      g_value_set_boolean (value, chrisFilter->luma_only);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "chrisFilter", GST_RANK_NONE,
      GST_TYPE_CHRISFILTER);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    chrisFilter,
    "Apply a blurring filter to an image",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
