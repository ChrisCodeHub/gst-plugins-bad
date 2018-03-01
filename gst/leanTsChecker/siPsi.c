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



#include "siPsi.h"
#include <string.h>

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

  handler->serviceList = g_new (siPsi_ServiceList, 1);
  uint8_t maxPrograms = 32;
  handler->serviceList->maxNumberOfPrograms = maxPrograms;
  handler->serviceList->numberOfPrograms = 0;
  handler->serviceList->programList = g_new (ProgramDef, maxPrograms);
  for (uint32_t i = 0; i < handler->serviceList->maxNumberOfPrograms; i++) {
    uint8_t maxStreams = 12;
    handler->serviceList->programList[i].serviceName = g_malloc (100);
    handler->serviceList->programList[i].maxNumberOfStreams = maxStreams;
    handler->serviceList->programList[i].numberOfStreams = 0;
    handler->serviceList->programList[i].streamdef =
        g_new (PMT_Streams, maxStreams);
  }

  handler->trackedPIDsMax = 256;
  handler->trackedPIDs = g_new (siPsi_isKnownPID, handler->trackedPIDsMax);
  handler->trackedPIDsLength = 1;
  for (uint16_t x = 0; x < handler->trackedPIDsMax; x++) {
    handler->trackedPIDs[x].tableBody = NULL;
    handler->trackedPIDs[x].TableInfo = NULL;
  }

  handler->trackedPIDs[0].PID = 0x00;   // start with just the PAT
  handler->trackedPIDs[0].TableType = PAT_TABLE;
  handler->trackedPIDs[0].parserState = WAITNG_FOR_START;
  handler->trackedPIDs[0].version_number = 0xff;
  handler->trackedPIDs[0].tableBody = g_malloc (1024);
  handler->trackedPIDs[0].CurrentLength = 0;
  handler->trackedPIDs[0].TableInfo = NULL;
  handler->shownDebug = 0;
  return (handler);
}


//##############################################################################
// CLEAN up code for this blob.  Free up memory and resources
//
void
siPsi_ParserDispose (siPsi_TableHandler * handler)
{

  g_print (" <<<<<< Diposed a sipsi parser >>>>>> \n");


  for (uint32_t i = 0; i < handler->serviceList->maxNumberOfPrograms; i++) {
    g_free (handler->serviceList->programList[i].serviceName);
    g_free (handler->serviceList->programList[i].streamdef);
  }
  g_free (handler->serviceList->programList);
  g_free (handler->serviceList);

  for (uint32_t idx = 0; idx < handler->trackedPIDsLength; idx++) {
    g_free (handler->trackedPIDs[idx].tableBody);
    if (handler->trackedPIDs[idx].TableInfo != NULL) {
      g_free (handler->trackedPIDs[idx].TableInfo);
    }
  }
  g_free (handler->trackedPIDs);

  g_free (handler);
}

//##############################################################################
//####################### Process the data functions ###########################
//##############################################################################

//##############################################################################
// called after seeing a new PAT, adds the PMT pids to the PIDs we are interested
// in.  Since its a new PAT, we delete all the other "tracked pids"entries

void
UpdateTrackedPIDs (siPsi_TableHandler * handler)
{
  uint16_t index = handler->trackedPIDsLength;
  siPsi_isKnownPID *progInfo = &handler->trackedPIDs[1];

  for (index = 1; index < handler->trackedPIDsLength; index++) {
    (progInfo->programNumber == 0) ? (progInfo->TableType = NIT_TABLE)
        : (progInfo->TableType = PMT_TABLE);

    progInfo->parserState = WAITNG_FOR_START;
    progInfo->version_number = 0xff;
    progInfo->CurrentLength = 0;
    progInfo++;
  }

  // if(handler->shownDebug == 0)
  // {
  //   g_print(" ~~~~~~~~~~~~~~~~~~~~ PAT listigs ~~~~~~~~~~~~~~~~~~~~~~~ \n");
  //   handler->shownDebug = 1;
  //   for (index = 1; index <   handler->trackedPIDsLength; index++){
  //     g_print(" 0x%x     0x%x    %d\n", handler->trackedPIDs[index].programNumber,
  //                                       handler->trackedPIDs[index].PID,
  //                                       handler->trackedPIDs[index].TableType);
  // }

}

//##############################################################################
//
void
siPsi_StoreSectionSegment (siPsi_isKnownPID * trackedPIDs,
    const uint8_t * data, uint8_t BytesOfPayload)
{
  uint16_t CurrentLength = trackedPIDs->CurrentLength;
  uint8_t *storeAddress = &trackedPIDs->tableBody[CurrentLength];
  memcpy (storeAddress, data, BytesOfPayload);
  CurrentLength += BytesOfPayload;
  trackedPIDs->CurrentLength = CurrentLength;
}



//#############################################################################
//##
void
SDTparse (const uint8_t * data, uint32_t dataLeft, siPsi_isKnownPID * progInfo)
{
  g_print (" ~~~~~~~~~~~~~~~~   SDT ~~~~~~~~~~~~~~~~~~ \n");
}


//#############################################################################
//##
void
PMTparse (const uint8_t * data, uint32_t dataLeft, siPsi_isKnownPID * progInfo,
    siPsi_TableHandler * handler)
{
  gboolean showDebug = TRUE;
  uint16_t pcrPID = (GST_READ_UINT16_BE (data)) & 0x1fff;
  data += 2;
  dataLeft -= 2;

  uint16_t programInfoLength = (GST_READ_UINT16_BE (data)) & 0x0fff;
  data += 2;
  dataLeft -= 2;

  data += programInfoLength;
  dataLeft -= programInfoLength;

  siPsi_ServiceList *serviceList = handler->serviceList;
  uint16_t wrPtr = serviceList->numberOfPrograms;

  ProgramDef *programDefiniton = &serviceList->programList[wrPtr];
  uint16_t maxStreams = programDefiniton->maxNumberOfStreams;
  programDefiniton->numberOfStreams = 0;
  PMT_Streams *streamDefs = programDefiniton->streamdef;

  programDefiniton->pcrPID = pcrPID;
  programDefiniton->programNumber = progInfo->programNumber;

  while (dataLeft > 4) {
    streamDefs->streamType = *data++;
    streamDefs->streamPID = (GST_READ_UINT16_BE (data)) & 0x1fff;
    data += 2;
    dataLeft -= 3;
    streamDefs++;
    if (programDefiniton->numberOfStreams < maxStreams) {
      programDefiniton->numberOfStreams++;
    }

    uint16_t esInfoLength = (GST_READ_UINT16_BE (data)) & 0x0fff;
    data += 2;
    dataLeft -= 2;
    // skip additional descriptors for now
    data += esInfoLength;
    dataLeft -= esInfoLength;
  }

  wrPtr++;
  if (wrPtr < serviceList->maxNumberOfPrograms) {
    serviceList->numberOfPrograms = wrPtr;
  }

  if (showDebug == TRUE) {
    g_print ("PMT[%02d]: pcr 0x%04x", programDefiniton->programNumber,
        programDefiniton->pcrPID);

    streamDefs = programDefiniton->streamdef;
    for (uint16_t idx = 0; idx < programDefiniton->numberOfStreams; idx++) {
      g_print (" type[%02d] 0x%4x", streamDefs->streamType,
          streamDefs->streamPID);
      streamDefs++;
    }
    g_print ("\n");

  }
//  g_print ("PMT[%02d]: pcr 0x%x  %d strPID 0x%x   ~~~~~~~~ \n", progInfo->programNumber,
  //          pcrPID, programInfoLength, streamType, streamPID, dataLeft);

  // CRC is last 4 bytes, ignore for now
}

//#############################################################################
//##
void
PATparse (const uint8_t * data, uint32_t dataLeft, siPsi_isKnownPID * progInfo,
    siPsi_TableHandler * handler)
{
  uint16_t maxEntries = handler->trackedPIDsMax;
  uint8_t idx = 1;

  while (dataLeft > 4) {
    handler->trackedPIDs[idx].programNumber = GST_READ_UINT16_BE (data);
    data += 2;
    handler->trackedPIDs[idx].PID = GST_READ_UINT16_BE (data) & 0x1FFF;
    data += 2;
    dataLeft -= 4;
    if (idx < maxEntries) {
      idx++;
    }
  }

  handler->trackedPIDsLength = idx;

  // CRC is last 4 bytes, ignore for now


}

//#############################################################################
//##
// on entry we are pointing immediatly below the section_length fields
// of the header
void
sectionParse (const uint8_t * data, siPsi_isKnownPID * progInfo,
    siPsi_TableHandler * handler)
{
  uint8_t currentNext;
  uint8_t versionNumber, previousVersionNumber;
  uint16_t dataLeft = progInfo->section_length;

  data += 2;                    // skip transport_stream_id
  dataLeft -= 2;

  versionNumber = (*data >> 1) & 0x1f;
  currentNext = *data++ & 1;
  dataLeft -= 1;

  previousVersionNumber = progInfo->version_number;
  if ((currentNext == 1) && (versionNumber != previousVersionNumber)) {
    progInfo->version_number = versionNumber;
    data += 2;                  // skip section_number, last_section_number
    dataLeft -= 2;
    if (progInfo->table_id == program_association_section) {
      PATparse (data, dataLeft, progInfo, handler);
      UpdateTrackedPIDs (handler);
    } else if (progInfo->table_id == TS_program_map_section) {
      PMTparse (data, dataLeft, progInfo, handler);
    }
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
siPsi_IsTable (uint8_t afFlags, siPsi_TableHandler * handler,
    uint16_t packetPID, const uint8_t * payloadData, uint8_t PUSI)
{
  uint8_t pointerField = 0;
  uint8_t BytesOfPayload = 184;
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

          if (BytesOfPayload >= 3) {
            progInfo->table_id = *payloadData++;
            uint16_t section_length = GST_READ_UINT16_BE (payloadData);
            progInfo->section_length = section_length & 0x0fff;
            payloadData += 2;
            BytesOfPayload -= 3;

            if (progInfo->section_length <= BytesOfPayload) {
              sectionParse (payloadData, progInfo, handler);
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
          const uint8_t *storedPayload = &progInfo->tableBody[0];
          progInfo->table_id = *storedPayload++;
          uint16_t section_length = GST_READ_UINT16_BE (storedPayload);
          progInfo->section_length = section_length & 0x0fff;
          storedPayload += 2;
          CurrentLength -= 3;

          if (progInfo->section_length <= CurrentLength) {
            sectionParse (storedPayload, progInfo, handler);
          }
        }
      }
    }
  }

  return match;
}
