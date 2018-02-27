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
  #include <gst/base/gstadapter.h>

  #ifdef __cplusplus
  extern "C" {
  #endif /* __cplusplus */

  typedef enum
  {
    PAT_TABLE = 1,
    PMT_TABLE,
    SDT_TABLE
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

  siPsi_TableHandler *siPsi_ParserInit (void);
  void siPsi_ParserDispose (siPsi_TableHandler * handler);


  struct _siPsi_TableHandler{
    GstAdapter *adapter;
    uint16_t trackedPIDsLength;
    uint16_t trackedPIDsMax;
    siPsi_isKnownPID *trackedPIDs;
  };


  struct _siPsi_isKnownPID
  {
    esiPsi_TableType TableType;
    esiPsi_TableParserState parserState;
    uint8_t version_number;
    uint8_t table_id;
    uint16_t PID;
    uint16_t section_length;
    uint16_t CurrentLength;
    uint8_t* tableBody;
    void* TableInfo;
  };

  #ifdef __cplusplus
  }
  #endif /* __cplusplus */

  #endif /*__SI_PSI_H__*/
