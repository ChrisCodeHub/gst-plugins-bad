/*
 * GStreamer
 *
 * unit test for (audio) parser
 *
 * Copyright (C) 2008 Nokia Corporation. All rights reserved.
 *
 * Contact: Stefan Kost <stefan.kost@nokia.com>
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

#include <gst/check/gstcheck.h>
#include "elements/parser.h"


/* context state variables */
const gchar *ctx_factory;
GstStaticPadTemplate *ctx_sink_template;
GstStaticPadTemplate *ctx_src_template;
GstCaps *ctx_input_caps;
GstCaps *ctx_output_caps;
guint ctx_discard = 0;
datablob ctx_headers[MAX_HEADERS] = { {NULL, 0}, };

VerifyBuffer ctx_verify_buffer = NULL;
ElementSetup ctx_setup = NULL;

gboolean ctx_no_metadata = FALSE;

/* helper variables */
GList *current_buf = NULL;

GstPad *srcpad, *sinkpad;
guint dataoffset = 0;

/* takes a copy of the passed buffer data */
static GstBuffer *
buffer_new (const unsigned char *buffer_data, guint size)
{
  GstBuffer *buffer;

  buffer = gst_buffer_new_and_alloc (size);
  if (buffer_data) {
    memcpy (GST_BUFFER_DATA (buffer), buffer_data, size);
  } else {
    guint i;
    /* Create a recognizable pattern (loop 0x00 -> 0xff) in the data block */
    for (i = 0; i < size; i++) {
      GST_BUFFER_DATA (buffer)[i] = i % 0x100;
    }
  }

  gst_buffer_set_caps (buffer, GST_PAD_CAPS (srcpad));
  GST_BUFFER_OFFSET (buffer) = dataoffset;
  dataoffset += size;
  return buffer;
}

/*
 * Adds buffer sizes together.
 */
static void
buffer_count_size (void *buffer, void *user_data)
{
  guint *sum = (guint *) user_data;
  *sum += GST_BUFFER_SIZE (buffer);
}

/*
 * Verify that given buffer contains predefined ADTS frame.
 */
static void
buffer_verify_data (void *buffer, void *user_data)
{
  buffer_verify_data_s *vdata;

  if (!user_data) {
    return;
  }

  vdata = (buffer_verify_data_s *) user_data;

  GST_DEBUG ("discard: %d", vdata->discard);
  if (vdata->discard) {
    if (ctx_verify_buffer)
      ctx_verify_buffer (vdata, buffer);
    vdata->buffer_counter++;
    if (vdata->buffer_counter == vdata->discard) {
      vdata->buffer_counter = 0;
      vdata->discard = 0;
    }
    return;
  }

  if (!ctx_verify_buffer || !ctx_verify_buffer (vdata, buffer)) {
    fail_unless (GST_BUFFER_SIZE (buffer) == vdata->data_to_verify_size);
    fail_unless (memcmp (GST_BUFFER_DATA (buffer), vdata->data_to_verify,
            vdata->data_to_verify_size) == 0);
  }

  if (vdata->buffers_before_offset_skip) {
    /* This is for skipping the garbage in some test cases */
    if (vdata->buffer_counter == vdata->buffers_before_offset_skip) {
      vdata->offset_counter += vdata->offset_skip_amount;
    }
  }
  if (!vdata->no_metadata) {
    fail_unless (GST_BUFFER_TIMESTAMP (buffer) == vdata->ts_counter);
    fail_unless (GST_BUFFER_DURATION (buffer) != 0);
    fail_unless (GST_BUFFER_OFFSET (buffer) == vdata->offset_counter);
  }

  if (vdata->caps) {
    GST_LOG ("%" GST_PTR_FORMAT " = %" GST_PTR_FORMAT " ?",
        GST_BUFFER_CAPS (buffer), vdata->caps);
    fail_unless (gst_caps_is_equal (GST_BUFFER_CAPS (buffer), vdata->caps));
  }

  vdata->ts_counter += GST_BUFFER_DURATION (buffer);
  vdata->offset_counter += GST_BUFFER_SIZE (buffer);
  vdata->buffer_counter++;
}

static GstElement *
setup_element (const gchar * factory, ElementSetup setup,
    GstStaticPadTemplate * sink_template,
    GstCaps * sink_caps, GstStaticPadTemplate * src_template,
    GstCaps * src_caps)
{
  GstElement *element;
  GstBus *bus;

  if (setup) {
    element = setup (factory);
  } else {
    element = gst_check_setup_element (factory);
  }
  srcpad = gst_check_setup_src_pad (element, src_template, src_caps);
  sinkpad = gst_check_setup_sink_pad (element, sink_template, sink_caps);
  gst_pad_set_active (srcpad, TRUE);
  gst_pad_set_active (sinkpad, TRUE);

  bus = gst_bus_new ();
  gst_element_set_bus (element, bus);

  fail_unless (gst_element_set_state (element,
          GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE,
      "could not set to playing");

  buffers = NULL;
  return element;
}

static void
cleanup_element (GstElement * element)
{
  GstBus *bus;

  /* Free parsed buffers */
  gst_check_drop_buffers ();

  bus = GST_ELEMENT_BUS (element);
  gst_bus_set_flushing (bus, TRUE);
  gst_object_unref (bus);

  gst_pad_set_active (srcpad, FALSE);
  gst_pad_set_active (sinkpad, FALSE);
  gst_check_teardown_src_pad (element);
  gst_check_teardown_sink_pad (element);
  gst_check_teardown_element (element);
}

/* inits a standard test */
void
gst_parser_test_init (GstParserTest * ptest, guint8 * data, guint size,
    guint num)
{
  /* need these */
  fail_unless (ctx_factory != NULL);
  fail_unless (ctx_src_template != NULL);
  fail_unless (ctx_sink_template != NULL);

  /* basics */
  memset (ptest, 0, sizeof (*ptest));
  ptest->factory = ctx_factory;
  ptest->factory_setup = ctx_setup;
  ptest->sink_template = ctx_sink_template;
  ptest->src_template = ctx_src_template;
  ptest->framed = TRUE;
  /* could be NULL if not relevant/needed */
  ptest->src_caps = ctx_input_caps;
  ptest->sink_caps = ctx_output_caps;
  memcpy (ptest->headers, ctx_headers, sizeof (ptest->headers));
  ptest->discard = ctx_discard;
  /* some data that pleases caller */
  ptest->series[0].data = data;
  ptest->series[0].size = size;
  ptest->series[0].num = num;
  ptest->series[0].fpb = 1;
  ptest->series[1].fpb = 1;
  ptest->series[2].fpb = 1;
  ptest->no_metadata = ctx_no_metadata;
}

/*
 * Test if the parser pushes clean data properly.
 */
void
gst_parser_test_run (GstParserTest * test, GstCaps ** out_caps)
{
  buffer_verify_data_s vdata = { 0, 0, 0, NULL, 0, NULL, FALSE, 0, 0, 0 };
  GstElement *element;
  GstBuffer *buffer = NULL;
  GstCaps *src_caps;
  guint i, j, k;
  guint frames = 0, size = 0;

  element = setup_element (test->factory, test->factory_setup,
      test->sink_template, NULL, test->src_template, test->src_caps);

  /* push some setup headers */
  for (j = 0; j < G_N_ELEMENTS (test->headers) && test->headers[j].data; j++) {
    buffer = buffer_new (test->headers[j].data, test->headers[j].size);
    fail_unless_equals_int (gst_pad_push (srcpad, buffer), GST_FLOW_OK);
  }

  for (j = 0; j < 3; j++) {
    for (i = 0; i < test->series[j].num; i++) {
      /* sanity enforcing */
      for (k = 0; k < MAX (1, test->series[j].fpb); k++) {
        if (!k)
          buffer = buffer_new (test->series[j].data, test->series[j].size);
        else {
          GstCaps *caps = gst_buffer_get_caps (buffer);

          buffer = gst_buffer_join (buffer,
              buffer_new (test->series[j].data, test->series[j].size));
          if (caps) {
            gst_buffer_set_caps (buffer, caps);
            gst_caps_unref (caps);
          }
        }
      }
      fail_unless_equals_int (gst_pad_push (srcpad, buffer), GST_FLOW_OK);
      if (j == 0)
        vdata.buffers_before_offset_skip++;
      else if (j == 1)
        vdata.offset_skip_amount += test->series[j].size * test->series[j].fpb;
      if (j != 1) {
        frames += test->series[j].fpb;
        size += test->series[j].size * test->series[j].fpb;
      }
    }
  }
  gst_pad_push_event (srcpad, gst_event_new_eos ());

  if (G_LIKELY (test->framed))
    fail_unless_equals_int (g_list_length (buffers) - test->discard, frames);

  /* if all frames are identical, do extended test,
   * otherwise only verify total data size */
  if (test->series[0].data && (!test->series[2].size ||
          (test->series[0].size == test->series[2].size && test->series[2].data
              && !memcmp (test->series[0].data, test->series[2].data,
                  test->series[0].size)))) {
    vdata.data_to_verify = test->series[0].data;
    vdata.data_to_verify_size = test->series[0].size;
    vdata.caps = test->sink_caps;
    vdata.discard = test->discard;
    vdata.no_metadata = test->no_metadata;
    g_list_foreach (buffers, buffer_verify_data, &vdata);
  } else {
    guint datasum = 0;

    g_list_foreach (buffers, buffer_count_size, &datasum);
    size -= test->dropped;
    fail_unless_equals_int (datasum, size);
  }

  src_caps = gst_pad_get_negotiated_caps (sinkpad);
  GST_LOG ("output caps: %" GST_PTR_FORMAT, src_caps);

  if (test->sink_caps) {
    GST_LOG ("%" GST_PTR_FORMAT " = %" GST_PTR_FORMAT " ?", src_caps,
        test->sink_caps);
    fail_unless (gst_caps_is_equal (src_caps, test->sink_caps));
  }

  if (out_caps)
    *out_caps = src_caps;
  else
    gst_caps_unref (src_caps);

  cleanup_element (element);
}

/*
 * Test if the parser pushes clean data properly.
 */
void
gst_parser_test_normal (guint8 * data, guint size)
{
  GstParserTest ptest;

  gst_parser_test_init (&ptest, data, size, 10);
  gst_parser_test_run (&ptest, NULL);
}

/*
 * Test if parser drains its buffers properly. Even one single frame
 * should be drained and pushed forward when EOS occurs. This single frame
 * case is special, since normally the parser needs more data to be sure
 * about stream format. But it should still push the frame forward in EOS.
 */
void
gst_parser_test_drain_single (guint8 * data, guint size)
{
  GstParserTest ptest;

  gst_parser_test_init (&ptest, data, size, 1);
  gst_parser_test_run (&ptest, NULL);
}

/*
 * Make sure that parser does not drain garbage when EOS occurs.
 */
void
gst_parser_test_drain_garbage (guint8 * data, guint size, guint8 * garbage,
    guint gsize)
{
  GstParserTest ptest;

  gst_parser_test_init (&ptest, data, size, 1);
  ptest.series[1].data = garbage;
  ptest.series[1].size = gsize;
  ptest.series[1].num = 1;
  gst_parser_test_run (&ptest, NULL);
}

/*
 * Test if parser splits a buffer that contains two frames into two
 * separate buffers properly.
 */
void
gst_parser_test_split (guint8 * data, guint size)
{
  GstParserTest ptest;

  gst_parser_test_init (&ptest, data, size, 10);
  ptest.series[0].fpb = 2;
  gst_parser_test_run (&ptest, NULL);
}

/*
 * Test if the parser skips garbage between frames properly.
 */
void
gst_parser_test_skip_garbage (guint8 * data, guint size, guint8 * garbage,
    guint gsize)
{
  GstParserTest ptest;

  gst_parser_test_init (&ptest, data, size, 10);
  ptest.series[1].data = garbage;
  ptest.series[1].size = gsize;
  ptest.series[1].num = 1;
  ptest.series[2].data = data;
  ptest.series[2].size = size;
  ptest.series[2].num = 10;
  gst_parser_test_run (&ptest, NULL);
}

/*
 * Test if the src caps are set according to stream format.
 */
void
gst_parser_test_output_caps (guint8 * data, guint size,
    const gchar * input_caps, const gchar * output_caps)
{
  GstParserTest ptest;

  gst_parser_test_init (&ptest, data, size, 10);
  if (input_caps) {
    ptest.src_caps = gst_caps_from_string (input_caps);
    fail_unless (ptest.src_caps != NULL);
  }
  if (output_caps) {
    ptest.sink_caps = gst_caps_from_string (output_caps);
    fail_unless (ptest.sink_caps != NULL);
  }
  gst_parser_test_run (&ptest, NULL);
  if (ptest.sink_caps)
    gst_caps_unref (ptest.sink_caps);
  if (ptest.src_caps)
    gst_caps_unref (ptest.src_caps);
}

/*
 * Test if the src caps are set according to stream format.
 */
GstCaps *
gst_parser_test_get_output_caps (guint8 * data, guint size,
    const gchar * input_caps)
{
  GstParserTest ptest;
  GstCaps *out_caps;

  gst_parser_test_init (&ptest, data, size, 10);
  if (input_caps) {
    ptest.src_caps = gst_caps_from_string (input_caps);
    fail_unless (ptest.src_caps != NULL);
  }
  gst_parser_test_run (&ptest, &out_caps);
  if (ptest.src_caps)
    gst_caps_unref (ptest.src_caps);

  return out_caps;
}
