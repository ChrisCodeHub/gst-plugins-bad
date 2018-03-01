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

 //#####################################
 // Useful chants
 // gst_adapter_flush(store->adapter, 188);
 // nbytes = gst_adapter_available_fast (store->adapter);
 // gst_adapter_clear(store->adapter);
 //#####################################


#include "headerAndPayloads.h"

typedef enum
{
  BAD_TS_SYNC = 1,
  OUT_OF_BUFFERS
} ePacketErrorCodes;

const uint32_t maxPIDsTracked = 128;

//##############################################################################
//  Infracstructure functions
//##############################################################################

//##############################################################################
// intialise the depacketiser / parser
headerAndPayloadStore *
TS_levelParserInit (void)
{
  headerAndPayloadStore *store;
  g_print (" <<<<<< Initialised the TS_parserStage >>>>>> \n");
  store = g_new (headerAndPayloadStore, 1);

  // Make an adapter - this allows the arkward GstBuffer size to be handled
  store->adapterFillLevel = 0;
  store->adapter = gst_adapter_new ();

  // latch the start time, useful for sending periodic messages on the
  // message bus to the applicationGstClockTime
  //store->startTime = gst_clock_get_time(GstSystemClock);

  store->topOfPIDStore = 0;
  store->TotalPacketsParsed = 0;
  store->pPIDStats = g_new (perPIDStatusInfo, maxPIDsTracked);
  store->pSiPsiParser = siPsi_ParserInit ();

  for (uint32_t idx = 0; idx < maxPIDsTracked; idx++) {
    store->pPIDStats[idx].haveSeenPCR = FALSE;
    store->pPIDStats[idx].CCErrorsSoFar = 0;
    store->pPIDStats[idx].packetsOnThisPID = 0;
    store->pPIDStats[idx].haveSentDebug = FALSE;
  }

  return (store);
}


//##############################################################################
// CLEAN up code for this blob.  Free up memory and resources
//
void
TS_levelParserDispose (headerAndPayloadStore * store)
{
  g_print (" <<<<<< Diposed the TS_parserStage >>>>>> \n");
  siPsi_ParserDispose (store->pSiPsiParser);

  g_object_unref (store->adapter);
  g_free (store->pPIDStats);
  g_free (store);
}

//##############################################################################
//####################### Process the data functions ###########################
//##############################################################################

int
TS_parseTsPacketHeader (headerAndPayloadStore * store)
{
  gboolean isTable;
  // check that the first byte is a 0x47
  // get the PID
  // adaptation fields to see if there is a PCR present
  // latch the Continuity count (CC from here on) check for cc errors

  while (store->adapterFillLevel >= 188) {
    // get access to the data in the adapter store
    const guint8 *inputData = gst_adapter_map (store->adapter, 184);

    if (*inputData++ != 0x47) {
      g_print ("\n\n ############   LOST SYNC ################### \n\n");
      return BAD_TS_SYNC;
    }

    uint8_t PUSI = (*inputData >> 6) & 1;
    uint16_t PID = GST_READ_UINT16_BE (inputData) & 0x1FFF;
    inputData += 2;

    uint8_t CCcount = *inputData & 0x0f;
    uint8_t afFlags = *inputData & 0x30;
    inputData++;


    uint32_t activeIndex;
    uint32_t currentMaxFill = store->topOfPIDStore;

    for (activeIndex = 0; activeIndex < currentMaxFill; activeIndex++) {
      if (PID == store->pPIDStats[activeIndex].PID) {
        break;
      }
    }

    if (activeIndex == store->topOfPIDStore) {
      if (store->topOfPIDStore < maxPIDsTracked) {
        store->pPIDStats[activeIndex].PID = PID;
        store->pPIDStats[activeIndex].lastContinuityCount = CCcount;
        store->pPIDStats[activeIndex].haveSeenPCR = FALSE;
        store->topOfPIDStore++;
      } else {
        g_print ("<><><> OUT OF BUFFERS IN TS_parseTsPacketHeader ><><><>\n");
        return OUT_OF_BUFFERS;
      }
    } else {
      uint16_t expectedContiuityCount;
      expectedContiuityCount =
          store->pPIDStats[activeIndex].lastContinuityCount;
      if ((afFlags & 0x30) != 0x30) {
        expectedContiuityCount = (expectedContiuityCount + 1) & 0xf;
        if (expectedContiuityCount != CCcount) {
          store->pPIDStats[activeIndex].CCErrorsSoFar++;
          //  g_print("<><><> CC ERROR on PID 0x%x ><><><>\n", store->pPIDStats[activeIndex].PID);
        }
      }
      store->pPIDStats[activeIndex].lastContinuityCount = CCcount;
      store->pPIDStats[activeIndex].packetsOnThisPID++;
    }

    store->pPIDStats[activeIndex].packetsSincePCR++;

    isTable =
        siPsi_IsTable (afFlags, store->pSiPsiParser, PID, inputData, PUSI);

    if (isTable == FALSE) {
      // adaptation field bits signify if there is payload only, padding or a
      // mix of the two.  If top bit (of two bits) is set then we need to check
      // what info is present

      if ((afFlags & 0x20) == 0x20) {
        uint8_t adaptationLength = *inputData++;
        if (adaptationLength != 0) {
          uint8_t pcrFlag = (*inputData++) & 0x10;
          if (pcrFlag) {
            uint64_t PCR_top33Bits;
            uint32_t PCR_bottom9Bits;
            uint64_t fullPCR_27Mhz;
            PCR_top33Bits = (inputData[0] << 25) +
                (inputData[1] << 17) +
                (inputData[2] << 9) +
                (inputData[3] << 1) + ((inputData[4] >> 7) & 1);

            PCR_top33Bits = (inputData[0] << 24) + (inputData[1] << 16) +
                (inputData[2] << 8) + (inputData[3]);
            PCR_top33Bits *= (double) 2;
            PCR_top33Bits += ((inputData[4] >> 7) & 1);
            PCR_bottom9Bits = ((inputData[4] & 1) << 8) + (inputData[5]);
            fullPCR_27Mhz = (PCR_top33Bits * (double) 300) + PCR_bottom9Bits;

            if (store->pPIDStats[activeIndex].haveSeenPCR == FALSE) {
              store->pPIDStats[activeIndex].haveSeenPCR = TRUE;
              store->pPIDStats[activeIndex].packetsSincePCR = 0;
            } else {
              // seen a PCR before so we can work out a bitrate -
              // bits/sec is (packets * 8 * 188)*27000000 / deltaPCR
              uint64_t deltaPCR;
              uint64_t packets;
              uint32_t bitrate;
              uint64_t previousPCR =
                  store->pPIDStats[activeIndex].lastPCR_value;
              if (previousPCR >= fullPCR_27Mhz) {
                deltaPCR = (double) 0x1ffffffff - previousPCR;
                deltaPCR = deltaPCR + fullPCR_27Mhz;
              } else {
                deltaPCR = fullPCR_27Mhz - previousPCR;
              }

              packets = store->pPIDStats[activeIndex].packetsSincePCR;
              uint64_t multipler =
                  (double) 8 * (double) 188 * (double) 27000000;
              bitrate = (uint32_t) ((packets * multipler) / deltaPCR);
              store->pPIDStats[activeIndex].bitrate = bitrate;

              //if (store->pPIDStats[activeIndex].haveSentDebug == FALSE) {
              //  store->pPIDStats[activeIndex].haveSentDebug = TRUE;
              //  g_print (" PID 0x%x   bitrate %d   packtes %d  CCerr %d\n",
              //      store->pPIDStats[activeIndex].PID,
              //      store->pPIDStats[activeIndex].bitrate,
              //      (uint32_t) packets,
              //      (uint32_t)store->pPIDStats[activeIndex].CCErrorsSoFar);
              //}
            }
            store->pPIDStats[activeIndex].lastPCR_value = fullPCR_27Mhz;
            store->pPIDStats[activeIndex].packetsSincePCR = 0;
          }
        }
      }
    }
    gst_adapter_flush (store->adapter, 188);
    store->adapterFillLevel -= 188;
  }

  store->TotalPacketsParsed++;  // only count is there is at least a 0x47 there

  return 0;
}

//##############################################################################
// push the buffers that arrive on the input (aka srcpad) into the adapter and
// count the amount of data sat in the buffer (aka adapter)
//
void
TS_pushDataIn (headerAndPayloadStore * store, GstBuffer * buf,
    gboolean checkSync)
{
  gsize SizeOfBuffer = gst_buffer_get_size (buf);
  gst_adapter_push (store->adapter, buf);
  store->adapterFillLevel += SizeOfBuffer;

  // check for sync bytes to make sure this is a valid TS
  if ((checkSync == TRUE) && (store->adapterFillLevel >= 188)) {
    TS_parseTsPacketHeader (store);
  }

}
