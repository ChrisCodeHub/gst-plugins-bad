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
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "gstleanTsChecker.h"


enum
{
  PROP_PID_TO_TRACK = 1,
  N_PROPERTIES
};
static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };


// need to define the input and the output pads.  Sink is the output
// src is the input.  Since this filter can create its pads when its
// stamped out, they are ALWAYS pads
// For now only support "broadcast" formats of data, ie I420 STATIC CAPs only required
// TODO (later) - add the support for a 4:2:2 format and a 10bit as a nod to the Main10 HEVC standard
// This blob is just a simple 1 in ->-> 1 out pin entity

// setup the GObject basics so that all functions will be called appropriately:
G_DEFINE_TYPE (GstleanTsChecker, gst_leanTsChecker, GST_TYPE_ELEMENT);



// forward declarations
static GstFlowReturn gst_my_filter_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buf);

static gboolean gst_leanTsChecker_sink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);

void gst_cutter_message_new (GstleanTsChecker * me);


// https://developer.gnome.org/gstreamer/stable/GstPad.html
// is a good description of pad templates(below)
static GstStaticPadTemplate gst_leanTsChecker_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY"));

static GstStaticPadTemplate gst_leanTsChecker_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY"));


// add forward declarations for the boiler plate functions
static void gst_leanTsChecker_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_leanTsChecker_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);



//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// The _class_init() function, which is used to initialise the class only once
//(specifying what signals, arguments and virtual functions the class has and
// setting up global state);
static void
gst_leanTsChecker_class_init (GstleanTsCheckerClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_leanTsChecker_set_property;
  gobject_class->get_property = gst_leanTsChecker_get_property;

#if 1
  // GOBject STUFF

  obj_properties[PROP_PID_TO_TRACK] =
      g_param_spec_uint ("pid_to_track",
      "pid to track", "The PID to follow in the stream", 0 /* minimum value */ ,
      8192 /* maximum value */ ,
      0 /* default value */ ,
      G_PARAM_READWRITE);

  g_object_class_install_properties (G_OBJECT_CLASS (klass),
      N_PROPERTIES, obj_properties);
#endif

#if 0
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_ACTIVE,
      g_param_spec_boolean ("active", "active",
          "process stream", TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_PID_TO_TRACK,
      g_param_spec_int ("target_pid", "target_pid", "pid_to_track",
          G_MININT, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#endif
  // lets get the pads connected
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_leanTsChecker_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_leanTsChecker_src_template);


  gst_element_class_set_static_metadata (gstelement_class, "Chris leanTSChecker",       // long english name for functionality
      "parser",                 // type of element
      "basic parser on an Mpeg Transport Stream",       // purpose
      "Chris <ChrisCodeHub@hotmail.com>");      // author email

}

// initialise the variables to sensible defaultws
 // the _init() function, which is used to initialise a specific instance of this type.
static void
gst_leanTsChecker_init (GstleanTsChecker * leanTsChecker)
{
  // initialise the variables of this instance
  leanTsChecker->active = FALSE;
  leanTsChecker->silent = TRUE;
  leanTsChecker->totalCalls = 0;
  leanTsChecker->TotalBytesPassed = 0;
  leanTsChecker->pidToTrack = 0x123;

  /* pad through which data comes in to the element */
  leanTsChecker->sinkpad =
      gst_pad_new_from_static_template (&gst_leanTsChecker_sink_template,
      "sink");
  leanTsChecker->srcpad =
      gst_pad_new_from_static_template (&gst_leanTsChecker_src_template, "src");

  // Pads need to handle "events" such as EOS, configure callback function
  gst_pad_set_event_function (leanTsChecker->sinkpad,
      gst_leanTsChecker_sink_event);
  gst_pad_set_event_function (leanTsChecker->srcpad,
      gst_leanTsChecker_sink_event);

  // configure chain function on the pad before adding the pad to the element
  gst_pad_set_chain_function (leanTsChecker->sinkpad, gst_my_filter_chain);

  // pads are configured here with gst_pad_set_*_function ()
  gst_element_add_pad (GST_ELEMENT (leanTsChecker), leanTsChecker->sinkpad);
  gst_element_add_pad (GST_ELEMENT (leanTsChecker), leanTsChecker->srcpad);
}


//#############################################################################
//##
//##
//##
static gboolean
gst_leanTsChecker_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  gboolean ret;
  GstleanTsChecker *leanTsChecker = GST_leanTsChecker (parent);
  GstPad *outboundPad;

  if (pad == leanTsChecker->srcpad) {
    outboundPad = leanTsChecker->sinkpad;
    g_print (" <<<<<<< src =====> sink >>>>>>>> \n");
  } else {
    outboundPad = leanTsChecker->srcpad;
    g_print (" <<<<<<< sink =====> src >>>>>>>> \n");
  }

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
      /* we should handle the format here */
      // TODO - STUFF NEEDS TO GO HERE
      /* push the event downstream */
      ret = gst_pad_push_event (outboundPad, event);
      break;
    case GST_EVENT_EOS:
      /* end-of-stream, we should close down all stream leftovers here */
      // TODO - Clean up, wash the pots and get ready to head back to the space ship
      ret = gst_pad_event_default (pad, parent, event);
      g_print (" <<<<<<< EOS leanTsCheck >>>>>>>> \n");
      break;
    default:
    {
      /* just call the default handler */
      const gchar *MyNameIs = gst_event_type_get_name (GST_EVENT_TYPE (event));
      g_print (" DEFAULT EVENT  ##################> was a %s event \n",
          MyNameIs);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
  }
  return ret;
}



//#############################################################################
//##
//##

static GstFlowReturn
gst_my_filter_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstleanTsChecker *leanTsChecker = GST_leanTsChecker (parent);

  if (!leanTsChecker->silent)
    g_print ("Have data of size %" G_GSIZE_FORMAT " bytes!\n",
        gst_buffer_get_size (buf));

  leanTsChecker->TotalBytesPassed += gst_buffer_get_size (buf);
  leanTsChecker->totalCalls++;
  if (leanTsChecker->totalCalls == 1) {
    g_print ("######## ######Have data of size %" G_GSIZE_FORMAT " bytes!\n",
        gst_buffer_get_size (buf));

    gst_cutter_message_new (leanTsChecker);

  }

  return gst_pad_push (leanTsChecker->srcpad, buf);
}

//static GstMessage *gst_cutter_message_new (GstleanTsChecker * c, gboolean above,
//    GstClockTime timestamp);
//###########################################################################################
// to send a message on the message bus to main application - gst_element_post_message() is what the TSparsers are doing
// lifted this code from gst-good/cutter/cutter.c
void
gst_cutter_message_new (GstleanTsChecker * me)
{
  GstStructure *s;

//  s = gst_structure_new ("leanTsChecker",
//      "above", G_TYPE_BOOLEAN, above,
//      "timestamp", GST_TYPE_CLOCK_TIME, timestamp, NULL);

  s = gst_structure_new ("leanTsChecker", "iterations ", G_TYPE_INT,
      me->totalCalls, NULL);

  GstMessage *messageInABottle = gst_message_new_element (GST_OBJECT (me), s);
  gst_element_post_message (GST_ELEMENT (me), messageInABottle);
  return;
}


//###############################################################################
//      allow the element to be configured and checked with getter/setter duo
//
static void
gst_leanTsChecker_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstleanTsChecker *leanTsChecker;

  g_return_if_fail (GST_IS_leanTsChecker (object));
  leanTsChecker = GST_leanTsChecker (object);

  switch (prop_id) {
    case PROP_PID_TO_TRACK:
      leanTsChecker->pidToTrack = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_leanTsChecker_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstleanTsChecker *leanTsChecker;

  g_return_if_fail (GST_IS_leanTsChecker (object));
  leanTsChecker = GST_leanTsChecker (object);

  switch (prop_id) {

    case PROP_PID_TO_TRACK:
      g_value_set_uint (value, leanTsChecker->pidToTrack);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

//##############################################################################
// Once we have written code defining all the parts of the plugin, we need to
// write the plugin_init() function. This is a special function, which is
// called as soon as the plugin is loaded, and should return TRUE or FALSE
// depending on whether it loaded initialized any dependencies correctly. Also,
// in this function, any supported element type in the plugin should be
// registered.
static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "leanTsChecker", GST_RANK_NONE,
      GST_TYPE_leanTsChecker);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    leanTsChecker,
    "Apply a blurring filter to an image",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
//###############################################################################
