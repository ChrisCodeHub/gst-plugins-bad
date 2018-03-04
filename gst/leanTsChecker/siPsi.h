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

  #ifndef __SI_PSI_H__
  #define __SI_PSI_H__

  #include <stdint.h>
  #include <gst/gst.h>

  #ifdef __cplusplus
  extern "C" {
  #endif /* __cplusplus */

 // from Table 2-31 – table_id assignment values in
 // the mpeg systems spec
  typedef enum
  {
    program_association_section = 0x00,
    TS_program_map_section = 0x02,
    SDT_section_actual_transport_stream = 0x42
  } eTableIDs;

  typedef enum
  {
    PAT_TABLE = 1,
    PMT_TABLE,
    SDT_TABLE,
    NIT_TABLE
  } esiPsi_TableType;

  typedef enum
  {
    WAITNG_FOR_START = 1,
    COLLATING_PIECES,
    PARSING_DIRECT,
    PARSING_STORED,
    ASLEEP,
  } esiPsi_TableParserState;

typedef enum
{
  RANDO_ERROR_CODE = 1,
  ANOTHER_ERROR_CODE,
} esiPsiErrorCodes;


  typedef struct _siPsi_TableHandler siPsi_TableHandler;
  typedef struct _siPsi_isKnownPID siPsi_isKnownPID;
  typedef struct _siPMT_Streams PMT_Streams;
  typedef struct _ProgramDef ProgramDef;
  typedef struct _siPsi_ServiceList siPsi_ServiceList;

  siPsi_TableHandler *siPsi_ParserInit (void);
  void siPsi_ParserDispose (siPsi_TableHandler * handler);
  gboolean siPsi_IsTable (uint8_t afFlags, siPsi_TableHandler * handler,
    uint16_t packetPID, const uint8_t * payloadData, uint8_t PUSI);
  void siPsi_DisplayServiceList (siPsi_TableHandler * handler);

  struct _siPsi_ServiceList{
    uint16_t numberOfPrograms;
    uint16_t maxNumberOfPrograms;
    ProgramDef *programList;
  };

  struct _ProgramDef{
    uint16_t programNumber;
    char*    serviceName;
    int16_t  pcrPID;
    uint16_t numberOfStreams;
    uint16_t maxNumberOfStreams;
    PMT_Streams *streamdef;
  };

  struct _siPMT_Streams{
    uint8_t streamType;
    uint16_t streamPID;
  };

  struct _siPsi_isKnownPID
  {
    esiPsi_TableType TableType;
    esiPsi_TableParserState parserState;
    uint8_t version_number;
    uint8_t table_id;
    uint16_t PID;
    uint16_t programNumber;
    uint16_t section_length;
    uint16_t CurrentLength;
    uint8_t* tableBody;
    void* TableInfo;
  };

  // this structure is the main "body" of the SI/PSI parser
  struct _siPsi_TableHandler{
    uint16_t trackedPIDsLength;
    uint16_t trackedPIDsMax;
    uint16_t fixedPIDs;
    uint16_t shownDebug;
    uint16_t numberOfPMTsToFind;
    uint16_t numberOfPMTsFound;
    gboolean PATvalid;
    gboolean PMTsvalid;
    gboolean SDTvalid;
    siPsi_isKnownPID *trackedPIDs;
    siPsi_ServiceList* serviceList;
  };


  #ifdef __cplusplus
  }
  #endif /* __cplusplus */

  #endif /*__SI_PSI_H__*/
