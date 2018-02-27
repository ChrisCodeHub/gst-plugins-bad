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


#include "siPsi.h"
#include <string.h>


const uint32_t maxTablesTracked = 128;

//##############################################################################
//  Infracstructure functions
//##############################################################################

//##############################################################################
// intialise the depacketiser / parser
siPsi_TableHandler *
siPsi_ParserInit (void)
{
  siPsi_TableHandler *handler;
  g_print (" <<<<<< Initialised a sipsi parser >>>>>> \n");
  handler = g_new (siPsi_TableHandler, 1);

  // Make an adapter - this allows the arkward GstBuffer size to be handled
  handler->adapter = gst_adapter_new ();

  handler->trackedPIDsMax = 256;
  handler->trackedPIDs = g_new (siPsi_isKnownPID, handler->trackedPIDsMax);

  handler->trackedPIDsLength = 1;
  handler->trackedPIDs[0].PID = 0x00;   // start with just the PAT
  handler->trackedPIDs[0].TableType = PAT_TABLE;
  handler->trackedPIDs[0].parserState = WAITNG_FOR_START;
  handler->trackedPIDs[0].version_number = 0xff;
  handler->trackedPIDs[0].tableBody = g_malloc (1024);
  handler->trackedPIDs[0].CurrentLength = 0;

  return (handler);
}


//##############################################################################
// CLEAN up code for this blob.  Free up memory and resources
//
void
siPsi_ParserDispose (siPsi_TableHandler * handler)
{
  g_print (" <<<<<< Diposed a sipsi parser >>>>>> \n");

  for (uint32_t idx = 0; idx < handler->trackedPIDsLength; idx++) {
    g_free (handler->trackedPIDs[idx].tableBody);
  }

  g_free (handler->trackedPIDs);
  g_object_unref (handler->adapter);

  g_free (handler);
}

//##############################################################################
//####################### Process the data functions ###########################
//##############################################################################

//##############################################################################
//
void
siPsi_PATparser (siPsi_TableHandler * handler)
{
  g_print (" ~~~~~~~~~~~~~~~~   PAT ~~~~~~~~~~~~~~~~~~ \n");
  return;
}

//##############################################################################
//
void
siPsi_PMTparser (siPsi_TableHandler * handler)
{
  g_print (" ~~~~~~~~~~~~~~~~   PMT ~~~~~~~~~~~~~~~~~~ \n");
  return;
}

//##############################################################################
//
void
siPsi_SDTparser (siPsi_TableHandler * handler)
{
  g_print (" ~~~~~~~~~~~~~~~~   SDT ~~~~~~~~~~~~~~~~~~ \n");
  return;
}



void
siPsi_StoreSectionSegment (siPsi_isKnownPID * trackedPIDs,
    uint8_t * data, uint8_t BytesOfPayload)
{
  uint16_t CurrentLength = trackedPIDs->CurrentLength;
  uint8_t *storeAddress = &trackedPIDs->tableBody[CurrentLength];
  memcpy (storeAddress, data, BytesOfPayload);
  CurrentLength += BytesOfPayload;
  trackedPIDs->CurrentLength = CurrentLength;
}


//#############################################################################
//##
//##

void
PATparse (uint8_t * data, uint32_t dataLeft, siPsi_isKnownPID * progInfo)
{
  while (dataLeft > 4) {
    uint16_t programNumber;
    uint16_t program_map_PID;

    programNumber = GST_READ_UINT16_BE (data);
    data += 2;
    program_map_PID = (GST_READ_UINT16_BE (data) & 0x1FFF);
    data += 2;
    dataLeft -= 4;
  }
}

//#############################################################################
//##
//##

void
sectionParse (uint8_t * data, siPsi_isKnownPID * progInfo)
{
  uint8_t currentNext;
  uint8_t versionNumber, previousVersionNumber;
  uint16_t dataLeft = progInfo->section_length;

  data += 2;                    // skip transport_stream_id
  dataLeft -= 2;

  currentNext = *data & 1;
  versionNumber = (*data >> 1) & 0x1f;

  previousVersionNumber = progInfo->version_number;
  if ((currentNext == 1) && (versionNumber == previousVersionNumber)) {
    progInfo->version_number = versionNumber;
    data += 2;                  // skip section_number, last_section_number
    dataLeft -= 2;
    if (progInfo->table_id == 0x00) {
      PATparse (data, dataLeft, progInfo);
    }
    // CRC is last 4 bytes, ignore for now
  }
  progInfo->parserState = WAITNG_FOR_START;
}


//##############################################################################
// check if teh PID is in the list of pids to be parsed in this filter
// NOTE only some SI/PSI PIDs get filtered, just enough to know what is in the
// TS stream.
// NOTE the simple array here will struggle for 24x7 operation when tables come
// and go - needs replacing with a list or hash tabl  TODO
gboolean
siPsi_isSiPsiPID (siPsi_TableHandler * handler, uint16_t packetPID,
    uint8_t * payloadData, uint8_t BytesOfPayload, uint8_t PUSI)
{
  uint8_t pointerField = 0;
  uint16_t trackedPIDsLength = handler->trackedPIDsLength;
  gboolean match = FALSE;

  for (uint16_t x = 0; x < trackedPIDsLength; x++) {
    if (packetPID == handler->trackedPIDs[x].PID) {
      match = TRUE;
      siPsi_isKnownPID *progInfo = &handler->trackedPIDs[x];

      if (progInfo->parserState == WAITNG_FOR_START) {
        // step 1: we have a TS packet with some payload data, first need to
        // make sure we have collected a full table (they span multiple TS packets)
        // step 2: once we have a full packet, can parse it and just store
        // relevant data
        // NOTE, once we've had a packet and processed it, check to seen
        // if its the same version as the one we have done when a new one comes
        // in to allow us to stop looking at the packet early

        if (PUSI == 1)          // new table starts in this packet
        {
          pointerField = *payloadData++;        // offset to start of data
          BytesOfPayload--;
          payloadData += pointerField;  // point to 1st byte of table
          BytesOfPayload -= pointerField;
          progInfo->parserState = COLLATING_PIECES;

          // if there at least 3 bytes of payload its possible to get the
          // table_id and the section_length fields from this data.  need
          // the section length to know when all of the data is collected and
          // we can move on to parsing
          progInfo->section_length = 0xffff;    // set to MAX value of uint16_t

          if (BytesOfPayload >= 3) {
            progInfo->table_id = *payloadData++;
            progInfo->section_length =
                (GST_READ_UINT16_BE (payloadData) & 0x0FFF) + 3;
            payloadData += 2;
            BytesOfPayload -= 3;

            if (progInfo->section_length <= BytesOfPayload) {
              sectionParse (payloadData, progInfo);
            }
          } else {
            siPsi_StoreSectionSegment (progInfo, payloadData, BytesOfPayload);
          }
        }
      }

      if (progInfo->parserState == COLLATING_PIECES) {
        siPsi_StoreSectionSegment (progInfo, payloadData, BytesOfPayload);

        uint16_t CurrentLength = progInfo->CurrentLength;

        if (CurrentLength >= 3) {
          uint8_t *storedPayload = &progInfo->tableBody[0];
          progInfo->table_id = *storedPayload++;
          progInfo->section_length =
              (GST_READ_UINT16_BE (storedPayload) & 0x0FFF) + 3;
          storedPayload += 2;
          CurrentLength -= 3;

          if (progInfo->section_length <= CurrentLength) {
            sectionParse (storedPayload, progInfo);
          }
        }
      }

      switch (handler->trackedPIDs[x].TableType) {
        case PAT_TABLE:
          siPsi_PATparser (handler);
          break;

        case PMT_TABLE:
          siPsi_PMTparser (handler);
          break;

        case SDT_TABLE:
          siPsi_SDTparser (handler);
          break;

        default:
          break;
      }
      break;
    }
  }

  return match;
}
