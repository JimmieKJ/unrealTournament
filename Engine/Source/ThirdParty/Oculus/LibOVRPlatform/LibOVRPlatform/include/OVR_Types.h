#ifndef OVR_TYPES_H
#define OVR_TYPES_H

#include "OVR_KeyValuePairType.h"
#include "OVR_MatchmakingCriterionImportance.h"
#include "OVR_VoipSampleRate.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Represents a single state change in the platform such as the
/// response to a request, or some new information from the backend.
typedef uint64_t ovrRequest;

typedef struct {
  const char *key;
  ovrKeyValuePairType valueType;

  const char *stringValue;
  int intValue;
  double doubleValue;

} ovrKeyValuePair;

typedef struct {
  const char *key;
  ovrMatchmakingCriterionImportance importance;

  ovrKeyValuePair *parameterArray;
  unsigned int parameterArrayCount;

} ovrMatchmakingCriterion;


typedef struct {
  ovrKeyValuePair *customQueryDataArray;
  unsigned int customQueryDataArrayCount;

  ovrMatchmakingCriterion *customQueryCriterionArray;
  unsigned int customQueryCriterionArrayCount;
} ovrMatchmakingCustomQueryData;

/// Describes the various results possible when attempting to initialize the platform.
/// Anything other than ovrPlatformInitialize_Success should generally be considered a
/// fatal error with respect to using the platform, as the platform is not guaranteed
/// to be legitimate or work correctly.
typedef enum ovrPlatformInitializeResult_ {
	ovrPlatformInitialize_Success = 0,
	ovrPlatformInitialize_Uninitialized = -1,
	ovrPlatformInitialize_PreLoaded = -2,
	ovrPlatformInitialize_FileInvalid = -3,
	ovrPlatformInitialize_SignatureInvalid = -4,
	ovrPlatformInitialize_UnableToVerify = -5,
	ovrPlatformInitialize_VersionMismatch = -6,
} ovrPlatformInitializeResult;


/// A unique identifier for some entity in the system (user, room, etc).
///
typedef uint64_t ovrID;

/*
 * Convert a string into an ovrID.  Returns false if the input is
 * malformed (either out of range, or not an integer).
 */
bool ovrID_FromString(ovrID *outId, const char* inId);

/*
 * Convert an ID back into a string.  This function round trips with
 * ovrID_FromString().  Note: the id format may change in the future.
 * Developers should not rely on the string representation being an
 * integer.
 *
 * Length of outParam should be > 20.
 */
bool ovrID_ToString(char *outParam, size_t bufferLength, ovrID id);

typedef void(*LogFunctionPtr)(const char *, const char *);
extern LogFunctionPtr DoLogging;

/// Callback used by the Voip subsystem for audio filtering
///
typedef void(*VoipFilterCallback)(int16_t pcmData[], size_t pcmDataLength, int frequency, int numChannels);


#ifdef __cplusplus
}
#endif

#endif
