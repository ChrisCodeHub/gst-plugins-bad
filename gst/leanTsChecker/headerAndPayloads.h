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

  #ifndef __HeaderAndPayloads_H__
  #define __HeaderAndPayloads_H__

  #include <stdint.h>
  #include <gst/gst.h>
  #include <gst/base/gstadapter.h>
  #include "siPsi.h"

  #ifdef __cplusplus
  extern "C" {
  #endif /* __cplusplus */


  typedef struct _perPIDStatusInfo perPIDStatusInfo;
  typedef struct _headerAndPayloadStore headerAndPayloadStore;

  headerAndPayloadStore* TS_levelParserInit(void);
  void TS_levelParserDispose(headerAndPayloadStore* Store);
  void TS_pushDataIn(headerAndPayloadStore *store, GstBuffer *buf, gboolean checkSync);
  int TS_parseTsPacketHeader(headerAndPayloadStore *store);

  struct _perPIDStatusInfo{
    uint16_t PID;
    uint8_t  lastContinuityCount;
    uint32_t packetsSincePCR;
    uint32_t bitrate;
    uint64_t CCErrorsSoFar;
    uint64_t lastPCR_value;
    uint64_t lastPCRPAcketNumber_thisPID;
    uint64_t lastPCRPAcketNumber_allTS;
    uint64_t packetsOnThisPID;
    gboolean isKnownPID;
    gboolean haveSeenPCR;
    gboolean haveSentDebug;

  };


   /*  */
   struct _headerAndPayloadStore {
     int32_t adapterFillLevel;
     uint64_t TotalPacketsParsed;
     perPIDStatusInfo *pPIDStats;
     siPsi_TableHandler *pSiPsiParser;
     GstAdapter *adapter;
     GstClockTime startTime;

     uint32_t topOfPIDStore;

   };





  #ifdef __cplusplus
  }
  #endif /* __cplusplus */

  #endif /*HeaderAndPayloads*/
