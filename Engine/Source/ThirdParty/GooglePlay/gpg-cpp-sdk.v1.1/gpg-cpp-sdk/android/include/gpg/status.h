/**
 * @file gpg/status.h
 * @copyright Copyright 2014 Google Inc. All Rights Reserved.
 * @brief Enumerations used to represent the status of a request.
 */

#ifndef GPG_STATUS_H_
#define GPG_STATUS_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

// GCC Has a bug in which it throws spurious errors on "enum class" types
// when using __attribute__ with them. Suppress this.
// http://gcc.gnu.org/bugzilla/show_bug.cgi?id=43407
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif

#include "gpg/common.h"

namespace gpg {

/**
 *  A struct containing all possible status codes that can be returned by
 *  our APIs. A positive status indicates success, a negative one failure.
 *  The API never directly returns these values, instead doing so via one of
 *  several more specific status enums classes.
 */
struct GPG_EXPORT BaseStatus {
  /**
   * The type of the status values contained within {@link BaseStatus}.
   */
  enum StatusCode {
    VALID = 1,
    VALID_BUT_STALE = 2,
    VALID_WITH_CONFLICT = 3,
    FLUSHED = 4,
    ERROR_LICENSE_CHECK_FAILED = -1,
    ERROR_INTERNAL = -2,
    ERROR_NOT_AUTHORIZED = -3,
    ERROR_VERSION_UPDATE_REQUIRED = -4,
    ERROR_TIMEOUT = -5,
    ERROR_CANCELED = -6,
    ERROR_MATCH_ALREADY_REMATCHED = -7,
    ERROR_INACTIVE_MATCH = -8,
    ERROR_INVALID_RESULTS = -9,
    ERROR_INVALID_MATCH = -10,
    ERROR_MATCH_OUT_OF_DATE = -11,
    ERROR_UI_BUSY = -12,
    ERROR_QUEST_NO_LONGER_AVAILABLE = -13,
    ERROR_QUEST_NOT_STARTED = -14,
    ERROR_MILESTONE_ALREADY_CLAIMED = -15,
    ERROR_MILESTONE_CLAIM_FAILED = -16
  };
};

/**
 *   The set of possible values representing the result of an attempted
 *   operation.
 */
enum class GPG_EXPORT ResponseStatus {
  VALID = BaseStatus::VALID,
  VALID_BUT_STALE = BaseStatus::VALID_BUT_STALE,
  ERROR_LICENSE_CHECK_FAILED = BaseStatus::ERROR_LICENSE_CHECK_FAILED,
  ERROR_INTERNAL = BaseStatus::ERROR_INTERNAL,
  ERROR_NOT_AUTHORIZED = BaseStatus::ERROR_NOT_AUTHORIZED,
  ERROR_VERSION_UPDATE_REQUIRED = BaseStatus::ERROR_VERSION_UPDATE_REQUIRED,
  ERROR_TIMEOUT = BaseStatus::ERROR_TIMEOUT,
};

/**
 *  The set of possible values representing the result of a flush
 *  attempt.
 */
enum class GPG_EXPORT FlushStatus {
  FLUSHED = BaseStatus::FLUSHED,
  ERROR_INTERNAL = BaseStatus::ERROR_INTERNAL,
  ERROR_NOT_AUTHORIZED = BaseStatus::ERROR_NOT_AUTHORIZED,
  ERROR_VERSION_UPDATE_REQUIRED = BaseStatus::ERROR_VERSION_UPDATE_REQUIRED,
  ERROR_TIMEOUT = BaseStatus::ERROR_TIMEOUT,
};

/**
 *  The set of possible values representing the result of an authorization
 *  attempt.
 */
enum class GPG_EXPORT AuthStatus {
  VALID = BaseStatus::VALID,
  ERROR_INTERNAL = BaseStatus::ERROR_INTERNAL,
  ERROR_NOT_AUTHORIZED = BaseStatus::ERROR_NOT_AUTHORIZED,
  ERROR_VERSION_UPDATE_REQUIRED = BaseStatus::ERROR_VERSION_UPDATE_REQUIRED,
  ERROR_TIMEOUT = BaseStatus::ERROR_TIMEOUT,
};

/**
 *  The set of possible values representing the result of a UI attempt.
 */
enum class GPG_EXPORT UIStatus {
  VALID = BaseStatus::VALID,
  ERROR_INTERNAL = BaseStatus::ERROR_INTERNAL,
  ERROR_NOT_AUTHORIZED = BaseStatus::ERROR_NOT_AUTHORIZED,
  ERROR_VERSION_UPDATE_REQUIRED = BaseStatus::ERROR_VERSION_UPDATE_REQUIRED,
  ERROR_TIMEOUT = BaseStatus::ERROR_TIMEOUT,
  ERROR_CANCELED = BaseStatus::ERROR_CANCELED,
  ERROR_UI_BUSY = BaseStatus::ERROR_UI_BUSY
};

/**
 *  The set of possible values representing the result of a multiplayer
 *  operation.
 */
enum class GPG_EXPORT MultiplayerStatus {
  VALID = BaseStatus::VALID,
  VALID_BUT_STALE = BaseStatus::VALID_BUT_STALE,
  ERROR_INTERNAL = BaseStatus::ERROR_INTERNAL,
  ERROR_NOT_AUTHORIZED = BaseStatus::ERROR_NOT_AUTHORIZED,
  ERROR_VERSION_UPDATE_REQUIRED = BaseStatus::ERROR_VERSION_UPDATE_REQUIRED,
  ERROR_TIMEOUT = BaseStatus::ERROR_TIMEOUT,
  ERROR_MATCH_ALREADY_REMATCHED = BaseStatus::ERROR_MATCH_ALREADY_REMATCHED,
  ERROR_INACTIVE_MATCH = BaseStatus::ERROR_INACTIVE_MATCH,
  ERROR_INVALID_RESULTS = BaseStatus::ERROR_INVALID_RESULTS,
  ERROR_INVALID_MATCH = BaseStatus::ERROR_INVALID_MATCH,
  ERROR_MATCH_OUT_OF_DATE = BaseStatus::ERROR_MATCH_OUT_OF_DATE
};
/**
 *  The set of possible values representing the result of an accept-quest
 *  operation.
 */
enum class GPG_EXPORT QuestAcceptStatus {
  VALID = BaseStatus::VALID,
  ERROR_INTERNAL = BaseStatus::ERROR_INTERNAL,
  ERROR_NOT_AUTHORIZED = BaseStatus::ERROR_NOT_AUTHORIZED,
  ERROR_TIMEOUT = BaseStatus::ERROR_TIMEOUT,
  ERROR_QUEST_NO_LONGER_AVAILABLE = BaseStatus::ERROR_QUEST_NO_LONGER_AVAILABLE,
  ERROR_QUEST_NOT_STARTED = BaseStatus::ERROR_QUEST_NOT_STARTED
};

/**
 * The set of possible values representing the result of a claim-milestone
 * operation.
 */
enum class GPG_EXPORT QuestClaimMilestoneStatus {
  VALID = BaseStatus::VALID,
  ERROR_INTERNAL = BaseStatus::ERROR_INTERNAL,
  ERROR_NOT_AUTHORIZED = BaseStatus::ERROR_NOT_AUTHORIZED,
  ERROR_TIMEOUT = BaseStatus::ERROR_TIMEOUT,
  ERROR_MILESTONE_ALREADY_CLAIMED = BaseStatus::ERROR_MILESTONE_ALREADY_CLAIMED,
  ERROR_MILESTONE_CLAIM_FAILED = BaseStatus::ERROR_MILESTONE_CLAIM_FAILED,
};

/**
 * The set of possible values representing the result of a snapshot open
 * operation.
 */
enum class GPG_EXPORT SnapshotOpenStatus {
  VALID = BaseStatus::VALID,
  VALID_WITH_CONFLICT = BaseStatus::VALID_WITH_CONFLICT,
  ERROR_INTERNAL = BaseStatus::ERROR_INTERNAL,
  ERROR_NOT_AUTHORIZED = BaseStatus::ERROR_NOT_AUTHORIZED,
  ERROR_TIMEOUT = BaseStatus::ERROR_TIMEOUT,
};

/// Returns true if a <code>BaseStatus</code> represents a successful operation.
bool IsSuccess(BaseStatus::StatusCode) GPG_EXPORT;

/// Returns true if authorization completed succesfully.
bool IsSuccess(AuthStatus) GPG_EXPORT;

/// Returns true if an attempted operation resulted in a successful response.
bool IsSuccess(ResponseStatus) GPG_EXPORT;

/// Returns true if a flush completed successfully.
bool IsSuccess(FlushStatus) GPG_EXPORT;

/// Returns true if a UI operation completed successfully.
bool IsSuccess(UIStatus) GPG_EXPORT;

/// Returns true if a multiplayer operation completed successfully.
bool IsSuccess(MultiplayerStatus) GPG_EXPORT;

/// Returns true if an accept quest operation completed successfully.
bool IsSuccess(QuestAcceptStatus) GPG_EXPORT;

/// Returns true if a claim quest milestone operation completed successfully.
bool IsSuccess(QuestClaimMilestoneStatus) GPG_EXPORT;

/// Returns true if a snapshot open operation completed successfully.
bool IsSuccess(SnapshotOpenStatus) GPG_EXPORT;

/// Returns true if a BaseStatus represents an unsuccessful operation.
bool IsError(BaseStatus::StatusCode) GPG_EXPORT;

/// Returns true if auhorization did not complete successfully.
bool IsError(AuthStatus) GPG_EXPORT;

/// Returns true if an attempted operation does not result in a successful
/// response.
bool IsError(ResponseStatus) GPG_EXPORT;

/// Returns true if a flush did not complete successfully.
bool IsError(FlushStatus) GPG_EXPORT;

/// Returns true if a UI operation did not complete successfully.
bool IsError(UIStatus) GPG_EXPORT;

/// Returns true if a Multiplayer operation did not complete successfully.
bool IsError(MultiplayerStatus) GPG_EXPORT;

/// Returns true if an accept quest operation did not complete successfully.
bool IsError(QuestAcceptStatus) GPG_EXPORT;

/// Returns true if a claim quest milestone operation did not complete
/// successfully.
bool IsError(QuestClaimMilestoneStatus) GPG_EXPORT;

/// Returns true if a snapshot open operation did not complete successfully.
bool IsError(SnapshotOpenStatus) GPG_EXPORT;

}  // namespace gpg

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop  // GCC diagnostic ignored "-Wattributes"
#endif

#endif  // GPG_STATUS_H_
