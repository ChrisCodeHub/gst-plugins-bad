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
#include "headerAndPayloads.h"




// setup the GObject basics

enum
{
  PROP_PID_TO_TRACK = 1,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (GstleanTsChecker, gst_leanTsChecker, GST_TYPE_ELEMENT);

// add forward declarations for the boiler plate functions
static void gst_leanTsChecker_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static void gst_leanTsChecker_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_leanTsChecker_dispose (GObject * object);
static void gst_leanTsChecker_finalize (GObject * object);



// forward declarations
static GstFlowReturn gst_my_filter_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buf);

static gboolean gst_leanTsChecker_sink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);

void gst_SendAStatusMessage (GstleanTsChecker * me);


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

  // Add the objects desctructors to the definition.
  gobject_class->dispose = gst_leanTsChecker_dispose;
  gobject_class->finalize = gst_leanTsChecker_finalize;
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

//##############################################################################
// ## initialise the variables to sensible defaults the _init() function,
//## which is used to initialise a specific instance of this type.
static void
gst_leanTsChecker_init (GstleanTsChecker * leanTsChecker)
{
  // initialise the variables of this instance
  leanTsChecker->active = FALSE;
  leanTsChecker->silent = TRUE;
  leanTsChecker->totalCalls = 0;
  leanTsChecker->TotalBytesPassed = 0;
  leanTsChecker->pidToTrack = 0x123;

  leanTsChecker->pheaderAndPayloadStore = TS_levelParserInit ();

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
//##
static void
gst_leanTsChecker_dispose (GObject * object)
{
  GstleanTsChecker *leanTsChecker = GST_leanTsChecker (object);
  gst_SendAStatusMessage (leanTsChecker);
  TS_levelParserDispose (leanTsChecker->pheaderAndPayloadStore);

// Chain up the parent class dispose function to have a clean exit
  if (G_OBJECT_CLASS (gst_leanTsChecker_parent_class)->dispose) {
    G_OBJECT_CLASS (gst_leanTsChecker_parent_class)->dispose (object);
  }
}

static void
gst_leanTsChecker_finalize (GObject * object)
{
  // Chain up the parent class finalize function to have a clean exit
  if (G_OBJECT_CLASS (gst_leanTsChecker_parent_class)->finalize) {
    G_OBJECT_CLASS (gst_leanTsChecker_parent_class)->finalize (object);
  }
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
      TS_showPacketStats (leanTsChecker->pheaderAndPayloadStore);
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
//## This is what gets called to "do stuff" as data flows through the blob
//##
static GstFlowReturn
gst_my_filter_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstBuffer *bufferTosendOn;
  GstleanTsChecker *leanTsChecker = GST_leanTsChecker (parent);

  // if this is the first buffer, latch the time NOTE - could be on state change?
  // if (leanTsChecker->totalCalls == 0) {
  //   leanTsChecker->startTime = g_get_monotonic_time ();
  // } else {
  //   int64_t nowTime = g_get_monotonic_time ();
  //   uint32_t elapsedTime =
  //       (uint32_t) ((nowTime - leanTsChecker->startTime) / 1000);
  //   if (leanTsChecker->totalCalls == 1000) {
  //     g_print (" @@@@@@ elapsed %u \n", elapsedTime);
  //   }
  // }

  bufferTosendOn = buf;

  if (!leanTsChecker->silent)
    g_print ("Have data of size %" G_GSIZE_FORMAT " bytes!\n",
        gst_buffer_get_size (buf));

  leanTsChecker->TotalBytesPassed += gst_buffer_get_size (buf);
  leanTsChecker->totalCalls++;

  // useful - can get GstClock for this element once its running,  Gives us
  // pipeine time - NOTE do need to unref this after use
  //if (leanTsChecker->totalCalls == 10)
  //{
  //  GstClock *clock = gst_element_get_clock(GST_ELEMENT(leanTsChecker));
  //  if (clock) {
  //    g_print("@@@@@@@@@@@@@@@@@@@@@@ GOT CLOCK @@@@@@@@@@@@@@@@@@@@@@@@@@");
  //  }
  //  gst_object_unref (clock);
  //}

  {
    bufferTosendOn = gst_buffer_ref (buf);

    // NOTE : ownership of "buf" is passed to TS_xxx by the below
    // need to get a new handle to pass downstream modules in pipeline
    TS_pushDataIn (leanTsChecker->pheaderAndPayloadStore, buf, TRUE);

    //gst_SendAStatusMessage (leanTsChecker);
  }

  return gst_pad_push (leanTsChecker->srcpad, bufferTosendOn);
}

//static GstMessage *gst_SendAStatusMessage (GstleanTsChecker * c, gboolean above,
//    GstClockTime timestamp);
//###########################################################################################
// to send a message on the message bus to main application - gst_element_post_message() is what the TSparsers are doing
// lifted this code from gst-good/cutter/cutter.c
void
gst_SendAStatusMessage (GstleanTsChecker * me)
{
  GstStructure *s;
  GstMessage *messageInABottle;

//  s = gst_structure_new ("leanTsChecker",
//      "above", G_TYPE_BOOLEAN, above,
//      "timestamp", GST_TYPE_CLOCK_TIME, timestamp, NULL);

  uint64_t TotalPacketsParsed = me->pheaderAndPayloadStore->TotalPacketsParsed;
  s = gst_structure_new ("leanTsChecker", "packetsParsed ", G_TYPE_UINT64,
      TotalPacketsParsed, NULL);

  messageInABottle = gst_message_new_element (GST_OBJECT (me), s);
  gst_element_post_message (GST_ELEMENT (me), messageInABottle);

  g_print ("<><><><>< Pkts %d  <><><< \n", (int) TotalPacketsParsed);
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
    "running basic checks on an Mpeg2 systems TS",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
//###############################################################################
