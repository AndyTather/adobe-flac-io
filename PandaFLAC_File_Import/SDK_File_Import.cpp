/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 1992-2008, Adobe Systems Incorporated                 */
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
/*
	10/15/99	eks - Created, version 1.0
	12/7/99		bbb - Edited, changed header #inclusion
	3/27/01		bbb - Rework (and fixed reel info handling, converted to MALError usage)
	9/18/01		zal - 2 minor changes to fix crash and bad graph in imDataRateAnalysis
	1/13/03		zal - Updated to build with Adobe Premiere Pro headers, .cpp shared library
	3/7/03		zal - Updated for Adobe Premiere Pro features
	8/11/03		zal - Fixed video duration analysis
	1/6/04		zal - Added audio support for Premiere Pro, arbitrary audio sample rates,
				multi-channel audio, pixel aspect ratio, and fields; code cleanup
	3/7/04		zal - Added trimming support for Premiere Pro 1.5, imOpenFile and imCloseFile
	1/25/05		zal - Support for 24-bit video (no alpha channel), versioning, imSaveFile,
				and imDeleteFile
	10/26/05	zal - Support for new Premiere Pro 2.0 selectors
	12/13/05	zal - Asynchronous import
	6/20/06		zal	- High-bit video support (v410)
*/

#include "SDK_File_Import.h"
#ifdef PRWIN_ENV
//#include "SDK_Async_Import.h"
#endif

static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
static void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);

PREMPLUGENTRY DllExport xImportEntry (
	csSDK_int32		selector, 
	imStdParms		*stdParms, 
	void			*param1, 
	void			*param2)
{
	prMALError	result				= imUnsupported;

	switch (selector)
	{
		case imInit:
			result =	SDKInit(stdParms, 
								reinterpret_cast<imImportInfoRec*>(param1));
			break;

		// To be demonstrated
		// case imShutdown:



		// To be demonstrated
		// case imSetPrefs:

		case imGetInfo8:
			result =	SDKGetInfo8(stdParms, 
									reinterpret_cast<imFileAccessRec8*>(param1), 
									reinterpret_cast<imFileInfoRec8*>(param2));
			break;

		case imImportAudio7:
			result =	SDKImportAudio7(stdParms, 
										reinterpret_cast<imFileRef>(param1),
										reinterpret_cast<imImportAudioRec7*>(param2));
			break;

		case imOpenFile8:
			result =	SDKOpenFile8(	stdParms, 
										reinterpret_cast<imFileRef*>(param1), 
										reinterpret_cast<imFileOpenRec8*>(param2));
			break;
		
		case imQuietFile:
			result =	SDKQuietFile(	stdParms, 
										reinterpret_cast<imFileRef*>(param1), 
										reinterpret_cast<PrivateDataH>(param2)); 
			break;

		case imCloseFile:
			result =	SDKCloseFile(	stdParms, 
										reinterpret_cast<imFileRef*>(param1), 
										reinterpret_cast<PrivateDataH>(param2));
			break;

		case imGetIndFormat:
			result =	SDKGetIndFormat(stdParms, 
										reinterpret_cast<csSDK_size_t>(param1),
										reinterpret_cast<imIndFormatRec*>(param2));
			break;

		// Importers that support the Premiere Pro 2.0 API must return malSupports8 for this selector
		case imGetSupports8:
			result = malSupports8;
			break;


	}

	return result;
}


static prMALError 
SDKInit(
	imStdParms		*stdParms, 
	imImportInfoRec	*importInfo)
{
	importInfo->setupOnDblClk		= kPrFalse;		// If user dbl-clicks file you imported, pop your setup dialog
	//importInfo->canSave				= kPrTrue;		// Can 'save as' files to disk, real file only
	
	// imDeleteFile8 is broken on MacOS when renaming a file using the Save Captured Files dialog
	// So it is not recommended to set this on MacOS yet (bug 1627325)
	#ifdef PRWIN_ENV
	//importInfo->canDelete			= kPrTrue;		// File importers only, use if you only if you have child files
	#endif
	
	//importInfo->dontCache			= kPrFalse;		// Don't let Premiere cache these files
	//importInfo->hasSetup			= kPrTrue;		// Set to kPrTrue if you have a setup dialog
	//importInfo->keepLoaded			= kPrFalse;		// If you MUST stay loaded use, otherwise don't: play nice
	//importInfo->priority			= 0;
	//importInfo->canTrim				= kPrTrue;
	//importInfo->canCalcSizes		= kPrTrue;
	if (stdParms->imInterfaceVer >= IMPORTMOD_VERSION_6)
	{
		//importInfo->avoidAudioConform = kPrTrue;
	}							
	
	importInfo->noFile				= kPrFalse;		
	importInfo->addToMenu			= kPrFalse;		
	importInfo->canDoContinuousTime = kPrFalse;	
	importInfo->canCreate			= kPrFalse;
	importInfo->canSave				= kPrFalse;		
	importInfo->canDelete			= kPrFalse;			
	importInfo->canCalcSizes		= kPrFalse;
	importInfo->canTrim				= kPrFalse;
	importInfo->canCopy				= kPrFalse;
	importInfo->hasSetup			= kPrFalse;		
	importInfo->setupOnDblClk		= kPrFalse;		
	importInfo->dontCache			= kPrFalse;		
	importInfo->keepLoaded			= kPrFalse;		
	importInfo->priority			= 1;
	importInfo->avoidAudioConform	= kPrFalse;

	return malNoError;
}


//-------------------------------------------------------------------
//-------------------------------------------------------------------
// Get the prefs for the importer

static prMALError 
SDKGetPrefs8(
	imStdParms			*stdParms, 
	imFileAccessRec8	*fileInfo8, 
	imGetPrefsRec		*prefsRec)
{
	ImporterLocalRec8	*ldata;

	//-----------------
	// The first time you are called (or if you've been Quieted or Closed)
	// you will get asked for prefs data.  First time called, return the
	// size of the buffer you want Premiere to store prefs for your plug-in.

	// Note: if canOpen is not set to kPrTrue, I'm not getting this selector. Why?
	// Answer: because this selector is associated directly with "hasSetup"

	if(prefsRec->prefsLength == 0)
	{
		prefsRec->prefsLength = sizeof(ImporterLocalRec8);
	}
	else
	{
		ldata = (ImporterLocalRec8 *)prefsRec->prefs;
		prUTF16CharCopy(ldata->fileName, fileInfo8->filepath);
	}
	return malNoError;
}


//-------------------------------------------------------------------
//-------------------------------------------------------------------
//	Open the file and return the pointer to the file.
//	NOTE: This selector will only be called if "canOpen" was set to kPrTrue in imInit  
//

prMALError 
SDKOpenFile8(
	imStdParms		*stdParms, 
	imFileRef		*file, 
	imFileOpenRec8	*fileOpenInfo)
{
	prMALError			result = malNoError;
	PrivateDataH	pdH;
	
	if(fileOpenInfo->privatedata) //If we have private data, good, otherwise allocate it
	{
		pdH = (PrivateDataH)fileOpenInfo->privatedata;
	}
	else
	{
		pdH = (PrivateDataH)stdParms->piSuites->memFuncs->newHandle(sizeof(PrivateData));
		fileOpenInfo->privatedata = (void*)pdH;//(long)pdH;
	}	

	if (fileOpenInfo->inReadWrite == kPrOpenFileAccess_ReadOnly) //Open the file
	{
		(*pdH)->fileRef = CreateFileW(fileOpenInfo->fileinfo.filepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	}
	else if (fileOpenInfo->inReadWrite == kPrOpenFileAccess_ReadWrite)
	{
		(*pdH)->fileRef = CreateFileW(fileOpenInfo->fileinfo.filepath, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	}
	
	if ((*pdH)->fileRef == imInvalidHandleValue) //If something broke, let the host know. Otherwise fill out fileinfo.
	{
		result = imBadFile;
	}
	else
	{
		*file = (*pdH)->fileRef;
		fileOpenInfo->fileinfo.fileref = (*pdH)->fileRef;
		fileOpenInfo->fileinfo.filetype = 'FLAC';
	}

	return result;
}


//-------------------------------------------------------------------
//-------------------------------------------------------------------
//	"Quiet" the file (it's being closed, but you maintain your Private data).  
//	
//	NOTE:	If you don't set any privateData, you will not get an imCloseFile call
//			so close it up here.

static prMALError 
SDKQuietFile(
	imStdParms			*stdParms, 
	imFileRef			*SDKfileRef, 
	PrivateDataH				privateData)
{
	// If file has not yet been closed
	#ifdef PRWIN_ENV
	if (SDKfileRef && *SDKfileRef != imInvalidHandleValue)
	{
		CloseHandle (*SDKfileRef);
		*SDKfileRef = imInvalidHandleValue;
	}
	#else
		FSCloseFork (reinterpret_cast<intptr_t>(*SDKfileRef));
		*SDKfileRef = imInvalidHandleValue;
	#endif

	return malNoError; 
}

//-------------------------------------------------------------------
//-------------------------------------------------------------------
//	Close the file.  You MUST have allocated Private data in imGetPrefs or you will not
//	receive this call.

static prMALError 
SDKCloseFile(
	imStdParms			*stdParms, 
	imFileRef			*file,
	PrivateDataH				pdH) 
{
	if (file && *file) //If we have a file handle, close it
	{
		SDKQuietFile (stdParms, file, pdH);
	}

	if (pdH) //If we have private data, release it and clean up
	{

		(*pdH)->suiteBasic->ReleaseSuite(kPrSDKAudioSuite, kPrSDKAudioSuiteVersion);

		if((*pdH)->flacDecoder != NULL)
		{
			FLAC__stream_decoder_finish((*pdH)->flacDecoder);
			FLAC__stream_decoder_delete((*pdH)->flacDecoder);
		}

		if((*pdH)->audioConversionBuffer)
		{
			stdParms->piSuites->memFuncs->disposePtr((*pdH)->audioConversionBuffer);
		}

		stdParms->piSuites->memFuncs->disposeHandle(reinterpret_cast<char**>(pdH));
	}

	return malNoError;
}



static prMALError 
SDKGetIndFormat(
	imStdParms		*stdParms, 
	csSDK_size_t	index, 
	imIndFormatRec	*SDKIndFormatRec)
{
	prMALError	result		= malNoError;
	char formatname[255]	= "Panda FLAC Audio";
	char shortname[32]		= "FLAC Audio File";
	char platformXten[256]	= "flac\0\0";

	switch(index)
	{
		//	Add a case for each filetype.
		
	case 0:		
		
			SDKIndFormatRec->filetype			= 'FLAC';

			#ifdef PRWIN_ENV
			strcpy_s(SDKIndFormatRec->FormatName, sizeof (SDKIndFormatRec->FormatName), formatname);				// The long name of the importer
			strcpy_s(SDKIndFormatRec->FormatShortName, sizeof (SDKIndFormatRec->FormatShortName), shortname);		// The short (menu name) of the importer
			strcpy_s(SDKIndFormatRec->PlatformExtension, sizeof (SDKIndFormatRec->PlatformExtension), platformXten);	// The 3 letter extension
			#else
			strcpy(SDKIndFormatRec->FormatName, formatname);			// The Long name of the importer
			strcpy(SDKIndFormatRec->FormatShortName, shortname);		// The short (menu name) of the importer
			strcpy(SDKIndFormatRec->PlatformExtension, platformXten);	// The 3 letter extension
			#endif

			break;
		

	default:
		result = imBadFormatIndex;
	}
	return result;
}


//-------------------------------------------------------------------
//-------------------------------------------------------------------
// ImportAudio7 - Gets audio chunks from the file 
// Standard file importers get the chunks from disk, 
//	a synthetic importer generates the chunks. 
// SDK format audio is always at 32-bit float
// NOTE: A chunk is an arbitrary length of data

static prMALError 
SDKImportAudio7(
	imStdParms			*stdParms, 
	imFileRef			SDKfileRef, 
	imImportAudioRec7	*audioRec7)
{
	prMALError			result				= malNoError;
//	ImporterLocalRec8H	ldataH				= reinterpret_cast<ImporterLocalRec8H>(audioRec7->privateData);
	PrAudioSample		startAudioPosition	= 0,
						numAudioFrames		= 0;
/*	
	calculateAudioRequest(	audioRec7,
							(*ldataH)->theFile.numSampleFrames,
							&((*ldataH)->audioPosition),
							&startAudioPosition,
							&numAudioFrames);

	setPointerToAudioStart(	ldataH,
							startAudioPosition,
							SDKfileRef);

	readAudioToBuffer(	numAudioFrames,
						(*ldataH)->theFile.numSampleFrames,
						GetNumberOfAudioChannels ((*ldataH)->theFile.channelType),
						SDKfileRef,
						audioRec7->buffer);
*/
		PrivateDataH pdH = reinterpret_cast<PrivateDataH>(audioRec7->privateData); 

	stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(pdH)); //Lock private data

	if(!(*pdH)->flacErrorCode) //Everything appears to be working
	{
		if (audioRec7->position < 0) //Handle sequential audio access
		{
			startAudioPosition = (*pdH)->audioPosition;
		}
		else
		{
			startAudioPosition = audioRec7->position;
		}
		if (startAudioPosition + audioRec7->size > (*pdH)->audioNumberOfSamples)
		{
			numAudioFrames = (*pdH)->audioNumberOfSamples - startAudioPosition;
			(*pdH)->audioPosition = (*pdH)->audioNumberOfSamples;
		}
		else
		{
			numAudioFrames = audioRec7->size;
			(*pdH)->audioPosition = startAudioPosition + audioRec7->size;
		}

		if((*pdH)->audioConversionBuffer) //See if we've already allocated a buffer, resize it if needed
		{
			if((*pdH)->audioBytesPerSample == 3)
			{
				stdParms->piSuites->memFuncs->setPtrSize((PrMemoryPtr*)(&((*pdH)->audioConversionBuffer)), audioRec7->size * 4 * (*pdH)->audioChannels);
			}
			else
			{
				stdParms->piSuites->memFuncs->setPtrSize((PrMemoryPtr*)(&((*pdH)->audioConversionBuffer)), audioRec7->size * (*pdH)->audioBytesPerSample * (*pdH)->audioChannels);
			}
		}
		else
		{
			if((*pdH)->audioBytesPerSample == 3)
			{
				(*pdH)->audioConversionBuffer = stdParms->piSuites->memFuncs->newPtr(audioRec7->size * 4 * (*pdH)->audioChannels);
			}
			else
			{
				(*pdH)->audioConversionBuffer = stdParms->piSuites->memFuncs->newPtr(audioRec7->size * (*pdH)->audioBytesPerSample * (*pdH)->audioChannels);
			}
		}

		(*pdH)->flacCurrentPosition = startAudioPosition;
		(*pdH)->flacSamplesToRead = numAudioFrames;
		(*pdH)->flacBufferPosition = 0;

		while((*pdH)->flacSamplesToRead > 0)
		{
			FLAC__stream_decoder_seek_absolute((*pdH)->flacDecoder, (*pdH)->flacCurrentPosition);
		}

		if((*pdH)->audioBytesPerSample == 1) //Convert 8 bit audio
		{
			(*pdH)->suiteAudio->UninterleaveAndConvertFrom8BitInteger((*pdH)->audioConversionBuffer, audioRec7->buffer, (*pdH)->audioChannels, audioRec7->size);
		}
		else if((*pdH)->audioBytesPerSample == 2) //Convert 16 bit audio
		{
			(*pdH)->suiteAudio->UninterleaveAndConvertFrom16BitInteger((short*)(*pdH)->audioConversionBuffer, audioRec7->buffer, (*pdH)->audioChannels, audioRec7->size);
		}
		else if((*pdH)->audioBytesPerSample == 3 || (*pdH)->audioBytesPerSample == 4) 
		{
			(*pdH)->suiteAudio->UninterleaveAndConvertFrom32BitInteger((csSDK_int32*)(*pdH)->audioConversionBuffer, audioRec7->buffer, (*pdH)->audioChannels, audioRec7->size);
		}
		else //Something is wrong, return silence
		{
			memset(audioRec7->buffer, 0, audioRec7->size);
		}
	}
	else //Something is wrong, return silence
	{
		memset(audioRec7->buffer, 0, audioRec7->size);
	}

	stdParms->piSuites->memFuncs->unlockHandle(reinterpret_cast<char**>(pdH)); //Unlock private data

	return result;
}




prMALError MultiStreamAudioTesting(
	ImporterLocalRec8H	ldataH,
	imFileInfoRec8		*SDKFileInfo8)
{
	prMALError returnValue = malNoError;

	SDKFileInfo8->streamsAsComp = kPrFalse;

	if ((**ldataH).theFile.hasVideo)
	{
		if (((**ldataH).theFile.channelType == kPrAudioChannelType_Mono) && (SDKFileInfo8->streamIdx < 7))
		{
			if (SDKFileInfo8->streamIdx == 0)
			{
				SDKFileInfo8->hasVideo			= kPrTrue;
				SDKFileInfo8->hasAudio			= kPrFalse;
				returnValue = imIterateStreams;
			}
			else if (SDKFileInfo8->streamIdx < 6)
			{
				SDKFileInfo8->hasVideo			= kPrFalse;
				SDKFileInfo8->hasAudio			= kPrTrue;
				returnValue = imIterateStreams;
			}
			else
			{
				SDKFileInfo8->hasVideo			= kPrFalse;
				SDKFileInfo8->hasAudio			= kPrTrue;
				returnValue = imBadStreamIndex;
			}
		}

		else if (((**ldataH).theFile.channelType == kPrAudioChannelType_Stereo) && (SDKFileInfo8->streamIdx < 4))
		{
			if (SDKFileInfo8->streamIdx == 0)
			{
				SDKFileInfo8->hasVideo			= kPrTrue;
				SDKFileInfo8->hasAudio			= kPrTrue;
				returnValue = imIterateStreams;
			}
			else if (SDKFileInfo8->streamIdx < 3)
			{
				SDKFileInfo8->hasVideo			= kPrFalse;
				SDKFileInfo8->hasAudio			= kPrTrue;
				returnValue = imIterateStreams;
			}
			else
			{
				SDKFileInfo8->hasVideo			= kPrFalse;
				SDKFileInfo8->hasAudio			= kPrTrue;
				returnValue = imBadStreamIndex;
			}
		}
	}

	else // No video
	{
		if (((**ldataH).theFile.channelType == kPrAudioChannelType_Mono) && (SDKFileInfo8->streamIdx < 6))
		{
			if (SDKFileInfo8->streamIdx < 5)
			{
				SDKFileInfo8->hasVideo			= kPrFalse;
				SDKFileInfo8->hasAudio			= kPrTrue;
				returnValue = imIterateStreams;
			}
			else
			{
				SDKFileInfo8->hasVideo			= kPrFalse;
				SDKFileInfo8->hasAudio			= kPrTrue;
				returnValue = imBadStreamIndex;
			}
		}

		else if (((**ldataH).theFile.channelType == kPrAudioChannelType_Stereo) && (SDKFileInfo8->streamIdx < 4))
		{
			if (SDKFileInfo8->streamIdx < 3)
			{
				SDKFileInfo8->hasVideo			= kPrFalse;
				SDKFileInfo8->hasAudio			= kPrTrue;
				returnValue = imIterateStreams;
			}
			else
			{
				SDKFileInfo8->hasVideo			= kPrFalse;
				SDKFileInfo8->hasAudio			= kPrTrue;
				returnValue = imBadStreamIndex;
			}
		}
	}

	return returnValue;
}




/* 
	Populate the imFileInfoRec8 structure describing this file instance
	to Premiere.  Check file validity, allocate any private instance data 
	to share between different calls.
*/
prMALError 
SDKGetInfo8(
	imStdParms			*stdParms, 
	imFileAccessRec8	*fileAccessInfo, 
	imFileInfoRec8		*fileInfo)
{
	prMALError					result				= malNoError;
	PrivateDataH pdH = reinterpret_cast<PrivateDataH>(fileInfo->privatedata); 

	stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(pdH)); //Lock private data

	(*pdH)->flacErrorCode = 0;
	(*pdH)->flacDecoder = 0;

	char filepathASCII[255];
	int stringLength = (int)wcslen(reinterpret_cast<const wchar_t*>(fileAccessInfo->filepath));
	wcstombs_s(	NULL, reinterpret_cast<char*>(filepathASCII), sizeof (filepathASCII), fileAccessInfo->filepath, stringLength);

	FLAC__bool ok = true;
    FLAC__StreamDecoderInitStatus init_status;
	if(((*pdH)->flacDecoder = FLAC__stream_decoder_new()) == NULL)
	{
		(*pdH)->flacErrorCode = 1;
	}

	FLAC__stream_decoder_set_md5_checking((*pdH)->flacDecoder, true);

	init_status = FLAC__stream_decoder_init_file((*pdH)->flacDecoder, filepathASCII, write_callback, NULL, error_callback, pdH);
	if(init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK )
	{
		return imBadFile; //Error, bail out
	}
	FLAC__stream_decoder_process_until_end_of_metadata((*pdH)->flacDecoder);
	FLAC__stream_decoder_process_single((*pdH)->flacDecoder);

	(*pdH)->audioChannels = FLAC__stream_decoder_get_channels((*pdH)->flacDecoder);
	(*pdH)->audioNumberOfSamples = FLAC__stream_decoder_get_total_samples((*pdH)->flacDecoder);
	(*pdH)->audioNumberOfSamplesPerSecond = FLAC__stream_decoder_get_sample_rate((*pdH)->flacDecoder);
	(*pdH)->audioBytesPerSample = FLAC__stream_decoder_get_bits_per_sample((*pdH)->flacDecoder) / 8;
	(*pdH)->audioPosition = 0;

	//Fill out the general file info
	fileInfo->accessModes						= kSeparateSequentialAudio;
	fileInfo->hasDataRate						= kPrFalse;	
	fileInfo->hasVideo							= kPrFalse;
	fileInfo->hasAudio							= kPrTrue;
	fileInfo->alwaysUnquiet						= 0;
	fileInfo->highMemUsage						= 0;
	fileInfo->audInfo.numChannels				= (*pdH)->audioChannels;
	fileInfo->audInfo.sampleRate				= (float)(*pdH)->audioNumberOfSamplesPerSecond;
	fileInfo->audDuration						= (*pdH)->audioNumberOfSamples;

	if((*pdH)->audioBytesPerSample == 1)
	{
		fileInfo->audInfo.sampleType = kPrAudioSampleType_8BitInt;
	}
	else if((*pdH)->audioBytesPerSample == 2)
	{
		fileInfo->audInfo.sampleType = kPrAudioSampleType_16BitInt;
	}
	else if((*pdH)->audioBytesPerSample == 3)
	{
		fileInfo->audInfo.sampleType = kPrAudioSampleType_24BitInt;
	}
	else if((*pdH)->audioBytesPerSample == 4)
	{
		fileInfo->audInfo.sampleType = kPrAudioSampleType_32BitInt;
	}
	else
	{
		fileInfo->audInfo.sampleType = kPrAudioSampleType_Other;
	}

	(*pdH)->suiteBasic = stdParms->piSuites->utilFuncs->getSPBasicSuite(); //Allocate an audio suite
	if ((*pdH)->suiteBasic)
	{
		(*pdH)->suiteBasic->AcquireSuite (kPrSDKAudioSuite, kPrSDKAudioSuiteVersion, (const void**)&(*pdH)->suiteAudio);
	}

	stdParms->piSuites->memFuncs->unlockHandle(reinterpret_cast<char**>(pdH)); //Unlock private data

	return result;
}


FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
	PrivateDataH	pdH = reinterpret_cast<PrivateDataH>(client_data);

	int channelMap[6] = {0, 1, 4, 5, 2, 3};

	for(int i = 0; i != frame->header.blocksize && (*pdH)->flacSamplesToRead > 0; i++)
	{
		for(int chan = 0; chan != (*pdH)->audioChannels; chan++)
		{
			if((*pdH)->audioBytesPerSample == 3)
			{
				int byteDst = (4 * (*pdH)->audioChannels * (*pdH)->flacBufferPosition) + (4 * channelMap[chan]);
				(*pdH)->audioConversionBuffer[byteDst+0] =  0;
				(*pdH)->audioConversionBuffer[byteDst+1] =  ((char*)&(buffer[chan][i]))[0];
				(*pdH)->audioConversionBuffer[byteDst+2] =  ((char*)&(buffer[chan][i]))[1];
				(*pdH)->audioConversionBuffer[byteDst+3] =  ((char*)&(buffer[chan][i]))[2];
			}
			else
			{
				int byteDst = ((*pdH)->audioBytesPerSample * (*pdH)->audioChannels * (*pdH)->flacBufferPosition) + ((*pdH)->audioBytesPerSample * channelMap[chan]);
				memcpy(&((*pdH)->audioConversionBuffer[byteDst]), &(buffer[chan][i]), (*pdH)->audioBytesPerSample);
			}

		}

		(*pdH)->flacSamplesToRead--;
		(*pdH)->flacBufferPosition++;
		(*pdH)->flacCurrentPosition++;
	}

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
}

