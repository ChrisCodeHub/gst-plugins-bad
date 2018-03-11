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

void UpdateTrackedPIDs (siPsi_TableHandler * handler);
void siPsi_StoreSectionSegment (siPsi_isKnownPID * progInfo,
    const uint8_t * data, uint8_t BytesOfPayload, uint8_t trace);
void SDTparse (const uint8_t * data, uint32_t dataLeft,
    siPsi_isKnownPID * progInfo, siPsi_TableHandler * handler);
void PMTparse (const uint8_t * data, uint32_t dataLeft,
    siPsi_isKnownPID * progInfo, siPsi_TableHandler * handler);
void PATparse (const uint8_t * data, uint32_t dataLeft,
    siPsi_isKnownPID * progInfo, siPsi_TableHandler * handler);
void sectionParse (const uint8_t * data, siPsi_isKnownPID * progInfo,
    siPsi_TableHandler * handler);
//##############################################################################
//  Infrastructure functions
//##############################################################################

//##############################################################################
// intialise the depacketiser / parser
siPsi_TableHandler *
siPsi_ParserInit (void)
{
  siPsi_TableHandler *handler;
  uint8_t maxPrograms = 32;
  g_print (" <<<<<< Initialised a sipsi parser >>>>>> \n");

  handler = g_new (siPsi_TableHandler, 1);
  handler->numberOfPMTsFound = 0;
  handler->numberOfPMTsToFind = 0;
  handler->PATvalid = FALSE;
  handler->PMTsvalid = FALSE;
  handler->SDTvalid = FALSE;
  handler->serviceList = g_new (siPsi_ServiceList, 1);

  handler->serviceList->maxNumberOfPrograms = maxPrograms;
  handler->serviceList->numberOfPrograms = 0;
  handler->serviceList->programList = g_new (ProgramDef, maxPrograms);
  for (uint32_t i = 0; i < handler->serviceList->maxNumberOfPrograms; i++) {
    uint8_t maxStreams = 24;
    handler->serviceList->programList[i].serviceName = g_malloc (100);
    handler->serviceList->programList[i].serviceName[0] = '\0';
    handler->serviceList->programList[i].maxNumberOfStreams = maxStreams;
    handler->serviceList->programList[i].numberOfStreams = 0;
    handler->serviceList->programList[i].streamdef =
        g_new (PMT_Streams, maxStreams);
  }

  handler->trackedPIDsMax = 64;
  handler->trackedPIDs = g_new (siPsi_isKnownPID, handler->trackedPIDsMax);

  for (uint16_t x = 0; x < handler->trackedPIDsMax; x++) {
    handler->trackedPIDs[x].tableBody = NULL;
  }

  handler->trackedPIDs[0].PID = 0x00;   // start with the PAT
  handler->trackedPIDs[0].TableType = PAT_TABLE;
  handler->trackedPIDs[1].PID = 0x11;   // start with the SDT
  handler->trackedPIDs[1].TableType = SDT_TABLE;
  handler->fixedPIDs = 2;
  handler->trackedPIDsLength = 2;

  for (uint16_t idx = 0; idx < handler->trackedPIDsLength; idx++) {
    handler->trackedPIDs[idx].parserState = WAITNG_FOR_START;
    handler->trackedPIDs[idx].version_number = 0xff;
    handler->trackedPIDs[idx].tableBody = g_malloc (1128);
    handler->trackedPIDs[idx].CurrentLength = 0;
  }
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
  if (handler->serviceList->programList != NULL)
    g_free (handler->serviceList->programList);
  if (handler->serviceList != NULL)
    g_free (handler->serviceList);

  for (uint32_t idx = 0; idx < handler->trackedPIDsMax; idx++) {
    if (handler->trackedPIDs[idx].tableBody != NULL)
      g_free (handler->trackedPIDs[idx].tableBody);
  }

  g_free (handler->trackedPIDs);
  g_free (handler);
}

//##############################################################################
//####################### Process the data functions ###########################
//##############################################################################

//##############################################################################
//##
gboolean
si_Psi_FindServicePIDsFromPCR (siPsi_TableHandler * handler,
    const uint16_t pcrPID_toFind, uint16_t * PIDsInService)
{
  if (handler->PMTsvalid == TRUE) {
    const siPsi_ServiceList *serviceList = handler->serviceList;
    const uint16_t readLimit = serviceList->numberOfPrograms;
    uint16_t readptr = 0;

    const ProgramDef *programDefiniton = &serviceList->programList[0];

    for (readptr = 0; readptr < readLimit; readptr++) {
      if (programDefiniton->pcrPID == pcrPID_toFind) {
        uint16_t streamsToRead = programDefiniton->numberOfStreams;
        PMT_Streams *streamDefs = programDefiniton->streamdef;
        for (uint8_t idx = 0; idx < streamsToRead; idx++) {
          if (pcrPID_toFind != streamDefs->streamPID) {
            *PIDsInService++ = streamDefs->streamPID;
          }
          streamDefs++;
        }
        break;
      }
      programDefiniton++;
    }
  }
  *PIDsInService = 0xffff;      // 0xffff acts as an end marker for the list
  return handler->PMTsvalid;
}


//##############################################################################
// called after seeing a new PAT, adds the PMT pids to the PIDs we are interested
// in.  Since its a new PAT, we delete all previous PMTs

void
UpdateTrackedPIDs (siPsi_TableHandler * handler)
{
  uint16_t index = handler->fixedPIDs;
  uint16_t numberOfPMTsToFind = 0;
  siPsi_isKnownPID *progInfo = &handler->trackedPIDs[index];

  for (index = handler->fixedPIDs; index < handler->trackedPIDsLength; index++) {
    if (progInfo->programNumber == 0) {
      progInfo->TableType = NIT_TABLE;
    } else {
      progInfo->TableType = PMT_TABLE;
      numberOfPMTsToFind++;
    }
    progInfo->parserState = WAITNG_FOR_START;
    progInfo->version_number = 0xff;
    progInfo->CurrentLength = 0;
    progInfo++;
  }

  handler->numberOfPMTsFound = 0;
  handler->numberOfPMTsToFind = numberOfPMTsToFind;
  handler->PATvalid = TRUE;
  handler->PMTsvalid = FALSE;
  handler->SDTvalid = FALSE;

  if (handler->shownDebug == 1) {
    g_print (" ~~~~~~~~~~~~~~~~~~~~ PAT listings ~~~~~~~~~~~~~~~~~~~~~~~ \n");
    for (index = handler->fixedPIDs; index < handler->trackedPIDsLength;
        index++) {
      g_print ("Prog# 0x%x  PID 0x%04x  Type %d\n",
          handler->trackedPIDs[index].programNumber,
          handler->trackedPIDs[index].PID,
          handler->trackedPIDs[index].TableType);
    }
  }
}

//###############################################################data###############
//
void
siPsi_StoreSectionSegment (siPsi_isKnownPID * progInfo,
    const uint8_t * data, uint8_t BytesOfPayload, uint8_t trace)
{
  uint16_t CurrentLength = progInfo->CurrentLength;
  uint8_t *storeAddress;

  if (progInfo->tableBody == NULL) {
    progInfo->tableBody = g_malloc (1128);
  }
  storeAddress = progInfo->tableBody;
  storeAddress += (CurrentLength);
  memcpy (storeAddress, data, BytesOfPayload);
  CurrentLength += BytesOfPayload;
  progInfo->CurrentLength = CurrentLength;
}


//#############################################################################
//##
//## once the SDT is parsed, show the complete, aggregration of data
// from PAT, PMT amd SDT
void
siPsi_DisplayServiceList (siPsi_TableHandler * handler)
{
  siPsi_ServiceList *serviceList = handler->serviceList;
  PMT_Streams *streamDefs;
  uint16_t programsInList = serviceList->numberOfPrograms;

  g_print ("\n ~~~~~~~~~~~~~~~~~~~~ SDT listings ~~~~~~~~~~~~~~~~~~~~~~~ \n");
  for (uint8_t idx = 0; idx < programsInList; idx++) {
    ProgramDef *programDefiniton = &serviceList->programList[idx];
    g_print ("Program[%02d]: pcr 0x%04x", programDefiniton->programNumber,
        programDefiniton->pcrPID);

    streamDefs = programDefiniton->streamdef;
    for (uint16_t cmp = 0; cmp < programDefiniton->numberOfStreams; cmp++) {
      g_print (" type[%02d] 0x%04x", streamDefs->streamType,
          streamDefs->streamPID);
      streamDefs++;
    }

    g_print (" %s \n", programDefiniton->serviceName);

  }
}


//#############################################################################
//##
void
SDTparse (const uint8_t * data, uint32_t dataLeft, siPsi_isKnownPID * progInfo,
    siPsi_TableHandler * handler)
{
  uint16_t serviceID;
  uint16_t desriptorLength;
  uint16_t programsInList;
  gboolean foundServiceIDinPMT = FALSE;
  char *serviceNameString;

  siPsi_ServiceList *serviceList = handler->serviceList;
  programsInList = serviceList->numberOfPrograms;

  // skip original_network_id (16 uimsbf) and reserved_future_use (8 bslbf)
  data += 3;
  dataLeft -= 3;

  while (dataLeft > 4) {
    serviceID = (GST_READ_UINT16_BE (data));
    data += 2;
    dataLeft -= 2;
    // skip  reserved_future_use :6, EIT_schedule_flag :1
    //       EIT_present_following_flag 1 bslbf
    data += 1;
    dataLeft -= 1;
    desriptorLength = (GST_READ_UINT16_BE (data)) & 0x0fff;
    data += 2;
    dataLeft -= 2;

    // if we haven't seen a PMT entry for this service, don't go for the
    // descriptors as there is no point
    foundServiceIDinPMT = FALSE;
    for (uint8_t idx = 0; idx < programsInList; idx++) {
      ProgramDef *programDefiniton = &serviceList->programList[idx];
      if (programDefiniton->programNumber == serviceID) {
        foundServiceIDinPMT = TRUE;
        serviceNameString = programDefiniton->serviceName;
        break;
      }
    }

    const uint8_t *dscrData;
    uint8_t descTag;
    uint8_t length;
    // descriptor parsng if we have a PMT slot to put them in
    if (foundServiceIDinPMT == TRUE) {
      const uint8_t *localData = data;
      int16_t bytesleft = desriptorLength;
      while (bytesleft > 0) {
        dscrData = localData;
        descTag = *dscrData++;
        length = *dscrData++;
        if (descTag == 0x48) {
          uint8_t serviceProviderNameLength;
          uint8_t serviceNameLength;
          dscrData++;           // skip service type
          serviceProviderNameLength = *dscrData++;
          dscrData += serviceProviderNameLength;
          serviceNameLength = *dscrData++;
          memcpy (serviceNameString, dscrData, serviceNameLength);
          serviceNameString[serviceNameLength] = '\0';
        }
        localData += length + 2;
        bytesleft -= (length + 2);
      }
    }
    data += desriptorLength;
    dataLeft -= desriptorLength;
  }

  // ignore CRC (4bytes)
  if (handler->shownDebug == 1) {
    siPsi_DisplayServiceList (handler);
  }
  handler->SDTvalid = TRUE;
}


//#############################################################################
//##
void
PMTparse (const uint8_t * data, uint32_t dataLeft, siPsi_isKnownPID * progInfo,
    siPsi_TableHandler * handler)
{
  gboolean showDebug = FALSE;
  uint16_t pcrPID;
  uint16_t programInfoLength;
  uint16_t wrPtr;
  uint16_t maxStreams;
  uint16_t esInfoLength;
  siPsi_ServiceList *serviceList;
  ProgramDef *programDefiniton;
  PMT_Streams *streamDefs;

  pcrPID = (GST_READ_UINT16_BE (data)) & 0x1fff;
  data += 2;
  dataLeft -= 2;

  programInfoLength = (GST_READ_UINT16_BE (data)) & 0x0fff;
  data += 2;
  dataLeft -= 2;

  data += programInfoLength;
  dataLeft -= programInfoLength;

  serviceList = handler->serviceList;
  wrPtr = serviceList->numberOfPrograms;

  programDefiniton = &serviceList->programList[wrPtr];
  maxStreams = programDefiniton->maxNumberOfStreams;
  programDefiniton->numberOfStreams = 0;
  streamDefs = programDefiniton->streamdef;

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

    esInfoLength = (GST_READ_UINT16_BE (data)) & 0x0fff;
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

  handler->numberOfPMTsFound++;
  if (handler->numberOfPMTsFound == handler->numberOfPMTsToFind) {
    handler->PMTsvalid = TRUE;
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
  // CRC is last 4 bytes, ignore for now
}

//#############################################################################
//##
void
PATparse (const uint8_t * data, uint32_t dataLeft, siPsi_isKnownPID * progInfo,
    siPsi_TableHandler * handler)
{
  uint16_t maxEntries = handler->trackedPIDsMax;
  uint8_t idx = handler->fixedPIDs;

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

  // for SDT, THIS streams SDT is table_id 0x42 (DVB Blue book)
  // ignore this section if SDT and for a different TS

  if ((progInfo->TableType != SDT_TABLE) || (progInfo->table_id == 0x42)) {

    data += 2;                  // skip transport_stream_id
    dataLeft -= 2;

    versionNumber = (*data >> 1) & 0x1f;
    currentNext = *data++ & 1;
    dataLeft -= 1;

    previousVersionNumber = progInfo->version_number;
    if ((currentNext == 1) && (versionNumber != previousVersionNumber)) {
      progInfo->version_number = versionNumber;
      data += 2;                // skip section_number, last_section_number
      dataLeft -= 2;
      if (progInfo->table_id == program_association_section) {
        PATparse (data, dataLeft, progInfo, handler);
        UpdateTrackedPIDs (handler);
      } else if (progInfo->table_id == TS_program_map_section) {
        PMTparse (data, dataLeft, progInfo, handler);
      } else if (progInfo->table_id == SDT_section_actual_transport_stream) {
        SDTparse (data, dataLeft, progInfo, handler);
      }
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

  // first check of there is adaption field to skip over
  if ((afFlags & 0x20) == 0x20) {
    uint8_t adaptationLength = *payloadData++;
    BytesOfPayload--;
    if (adaptationLength != 0) {
      payloadData += adaptationLength;
      BytesOfPayload -= adaptationLength;
    }
  }

  for (uint16_t x = 0; x < trackedPIDsLength; x++) {
    if (packetPID == handler->trackedPIDs[x].PID) {
      siPsi_isKnownPID *progInfo = &handler->trackedPIDs[x];
      match = TRUE;
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
          progInfo->CurrentLength = 0;

          // if there at least 3 bytes of payload its possible to get the
          // table_id and the section_length fields from this data.  need
          // the section length to know when all of the data is collected and
          // we can move on to parsing

          if (BytesOfPayload >= 3) {
            uint16_t section_length;
            progInfo->table_id = *payloadData++;
            section_length = GST_READ_UINT16_BE (payloadData);
            progInfo->section_length = section_length & 0x0fff;
            payloadData += 2;
            BytesOfPayload -= 3;

            if ((progInfo->TableType == SDT_TABLE) &&
                ((progInfo->table_id != 0x42)
                    || (handler->PMTsvalid == FALSE))) {
              progInfo->parserState = WAITNG_FOR_START;
            } else {
              if (progInfo->section_length <= BytesOfPayload) {
                sectionParse (payloadData, progInfo, handler);
              } else {
                payloadData -= 3;       // need to store FROM table_id onwards
                BytesOfPayload += 3;
                siPsi_StoreSectionSegment (progInfo, payloadData,
                    BytesOfPayload, 1);
              }
            }
          }
        }
      } else if (progInfo->parserState == COLLATING_PIECES) {
        uint16_t CurrentLength;
        siPsi_StoreSectionSegment (progInfo, payloadData, BytesOfPayload, 3);
        CurrentLength = progInfo->CurrentLength;

        if (CurrentLength >= 3) {
          uint16_t section_length;
          const uint8_t *storedPayload = &progInfo->tableBody[0];
          progInfo->table_id = *storedPayload++;
          section_length = GST_READ_UINT16_BE (storedPayload);
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
