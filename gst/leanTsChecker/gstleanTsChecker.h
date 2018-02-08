/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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

// NOTE information in this .h file is based on
// https://gstreamer.freedesktop.org/documentation/plugin-development/basics/boiler.html

#ifndef __GST_leanTsChecker_H__
#define __GST_leanTsChecker_H__

#include <stdint.h>
#include <gst/gst.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _GstleanTsChecker GstleanTsChecker;
typedef struct _GstleanTsCheckerClass GstleanTsCheckerClass;

/* Definition of structure storing data for this element. */
struct _GstleanTsChecker {
  GstElement  element;

  uint32_t totalCalls;
  uint64_t TotalBytesPassed;
  gboolean silent;
  gboolean active;
  uint16_t pidToTrack;

  GstPad *sinkpad;
  GstPad *srcpad;
};

/* Standard definition defining a class for this element. */
struct _GstleanTsCheckerClass {
  GstElementClass  parent_class;
};

/* Standard macros for defining types for this element.  */
#define GST_TYPE_leanTsChecker (gst_leanTsChecker_get_type())
#define GST_leanTsChecker(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_leanTsChecker,GstleanTsChecker))
#define GST_leanTsChecker_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_leanTsChecker, GstleanTsCheckerClass))
#define GST_IS_leanTsChecker(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_leanTsChecker))
#define GST_IS_leanTsChecker_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_leanTsChecker))

/* Standard function returning type information. */
GType gst_leanTsChecker_get_type(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GST_leanTsChecker_H__ */
