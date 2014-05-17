/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 1999-2008, Adobe Systems Incorporated                 */
/* All Rights Reserved.                                            */
/*                                                                 */
/* NOTICE:  All information contained herein is, and remains the   */
/* property of Adobe Systems Incorporated and its suppliers, if    */
/* any.  The intellectual and technical concepts contained         */
/* herein are proprietary to Adobe Systems Incorporated and its    */
/* suppliers and may be covered by U.S. and Foreign Patents,       */
/* patents in process, and are protected by trade secret or        */
/* copyright law.  Dissemination of this information or            */
/* reproduction of this material is strictly forbidden unless      */
/* prior written permission is obtained from Adobe Systems         */
/* Incorporated.                                                   */
/*                                                                 */
/*******************************************************************/

#ifndef SDKFILEIMPORT_H
#define SDKFILEIMPORT_H

#include	"SDK_File.h"
#include	<stdio.h>
#include	<time.h>

// For performance testing
// Defining will cause duration of imImportImage calls to be
// written to C:\Premiere SDK logs\ImportImage_times.tab
//#define PERFORMANCE_TESTING

#ifdef PERFORMANCE_TESTING
#include <time.h>
#include <sys\timeb.h>
#include <direct.h>
#endif

#include "FLAC/stream_decoder.h"
// For testing multi-stream audio support
// Defining will cause the importer to import an SDK clip with mono audio
// as having 6 identical audio streams (6 mono channels),
// and an SDK clip with stereo audio
// as having 4 stereo streams (8 channels in stereo pairs)
//#define MULTISTREAM_AUDIO_TESTING

// Constants
#define		IMPORTER_NAME			"Panda FLAC Importer"


// Declare plug-in entry point with C linkage
extern "C" {
PREMPLUGENTRY DllExport xImportEntry (csSDK_int32	selector, 
									  imStdParms	*stdParms, 
									  void			*param1, 
									  void			*param2);

}


typedef struct
{	
	PrAudioSample		audioPosition;
	__int64				audioNumberOfSamples;
	csSDK_int32					audioNumberOfSamplesPerSecond;
	csSDK_int32					audioBytesPerSample;
	csSDK_int32					audioChannels;
	char*				audioConversionBuffer;

	__int64				flacCurrentPosition;
	csSDK_int32					flacBufferPosition;
	__int64				flacSamplesToRead;

	HANDLE				fileRef;
	SPBasicSuite		*suiteBasic;
	PrSDKAudioSuite		*suiteAudio;

	FLAC__StreamDecoder	*flacDecoder;
	csSDK_int32					flacErrorCode;

} PrivateData, *PrivateDataPtr, **PrivateDataH;


static prMALError 
SDKInit(
	imStdParms		*stdParms, 
	imImportInfoRec	*importInfo);

prMALError 
SDKOpenFile8(
	imStdParms			*stdParms, 
	imFileRef			*SDKfileRef, 
	imFileOpenRec8		*SDKfileOpenRec8);

static prMALError	
SDKQuietFile(
	imStdParms			*stdParms, 
	imFileRef			*SDKfileRef, 
	PrivateDataH		privateData);

static prMALError	
SDKCloseFile(
	imStdParms			*stdParms, 
	imFileRef			*SDKfileRef,
	PrivateDataH		privateData);

static prMALError 
SDKGetIndFormat(
	imStdParms			*stdparms, 
	csSDK_size_t		index, 
	imIndFormatRec		*SDKIndFormatRec);

static prMALError 
SDKImportAudio7(
	imStdParms			*stdParms, 
	imFileRef			SDKfileRef, 
	imImportAudioRec7	*SDKImportAudioRec7);

prMALError 
SDKGetInfo8(
	imStdParms			*stdparms, 
	imFileAccessRec8	*fileAccessInfo8, 
	imFileInfoRec8		*SDKFileInfo8);



#endif