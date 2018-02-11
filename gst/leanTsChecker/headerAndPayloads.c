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

#include "headerAndPayloads.h"

headerAndPayloadStore *
TS_levelParserInit (void)
{
  headerAndPayloadStore *phighLevelParser;
  g_print (" <<<<<< Initialised the TS_parserStage >>>>>> \n");
  phighLevelParser = g_new (headerAndPayloadStore, 1);

  // Make an adapter - this allows the arkward GstBuffer size to be handled
  phighLevelParser->adapter = gst_adapter_new ();

  return (phighLevelParser);
}


//##############################################################################
// CLEAN up code for this blob.  Free up memory and resources
//
void
TS_levelParserDispose (headerAndPayloadStore * store)
{
  g_print (" <<<<<< Diposed the TS_parserStage >>>>>> \n");

  g_object_unref (store->adapter);
  g_free (store);
}
