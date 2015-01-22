/**
 * @file gpg/types.h
 * @copyright Copyright 2014 Google Inc. All Rights Reserved.
 * @brief Assorted types.
 */

#ifndef GPG_TYPES_H_
#define GPG_TYPES_H_

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

#include <chrono>
#include "gpg/common.h"

namespace gpg {

/**
 * Data type used in specifying timeout periods for attempted operations.
 */

typedef std::chrono::milliseconds Timeout;

/**
 * Data type used to specify timestamps. Relative to the epoch (1/1/1970).
 */

typedef std::chrono::milliseconds Timestamp;

/**
 * Data type used to specify durations in milliseconds.
 */

typedef std::chrono::milliseconds Duration;

/**
 * Values specifying where to get data from when retrieving achievement,
 * leaderboard, and other data.
 * When the setting is CACHE_OR_NETWORK, the system resorts to the local
 * cache when no network connection is available.
 */
enum class GPG_EXPORT DataSource {
  CACHE_OR_NETWORK,
  NETWORK_ONLY
};

/**
 * Values used to specify the level of logging.
 */
enum class GPG_EXPORT LogLevel {
  VERBOSE,
  INFO,
  WARNING,
  ERROR
};

/**
 * Values used to specify the type of authorization operation to perform.
 */
enum class GPG_EXPORT AuthOperation {
  SIGN_IN,
  SIGN_OUT
};
/**
 * Values used to specify the resolution at which to fetch a particular image.
 */
enum class GPG_EXPORT ImageResolution {
  ICON,
  HI_RES
};

/**
 * Values used to specify achievement type.
 * A player makes gradual progress (steps) toward an incremental achievement.
 * He or she completes a standard achievement in a single step.
 */

enum class GPG_EXPORT AchievementType {
  STANDARD = 1,
  INCREMENTAL = 2
};

/**
 * Values used to specify achievement state.
 * A hidden achievement is one whose existence a player has not yet discovered.
 * make him or her aware of it.
 * A revealed achievement is one that the player knows about, but has not yet
 * earned.
 * An unlocked achievement is one that the player has earned.
 */

enum class GPG_EXPORT AchievementState {
  // These are ordered such that only increasing transitions are possible
  HIDDEN = 1,
  REVEALED = 2,
  UNLOCKED = 3
};

/**
 * Values specifying whether an event is hidden to the player,
 * or visible to them.
 */

enum class GPG_EXPORT EventVisibility {
  HIDDEN = 1,
  REVEALED = 2,
};

/**
 * Values specifying how whether larger or smaller scores should be interpreted
 * as better in the context of a leaderboard.
 */
enum class GPG_EXPORT LeaderboardOrder {
  LARGER_IS_BETTER,
  SMALLER_IS_BETTER
};

/**
 * Values specifying whether rankings are displayed on a leaderboard in order
 * of score or player.
 */
enum class GPG_EXPORT LeaderboardStart {
  TOP_SCORES,
  PLAYER_CENTERED
};

/**
 * Values that specify the period of time that a leaderboard covers.
 */
enum class GPG_EXPORT LeaderboardTimeSpan {
  DAILY,
  WEEKLY,
  ALL_TIME
};

/**
 * Values that specify whether a leaderboard can be viewed by anyone with a
 * Google+ account (public), or only members of a player's Google+ circles
 * (social).
 */

enum class GPG_EXPORT LeaderboardCollection {
  PUBLIC,
  SOCIAL
};

/**
 * Values used to specify the state of a participant within a
 * <code>TurnBasedMatch</code>.
 */

enum class GPG_EXPORT ParticipantStatus {
  INVITED = 1,
  JOINED = 2,
  DECLINED = 3,
  LEFT = 4,
  NOT_INVITED_YET = 5,
  FINISHED = 6,
  UNRESPONSIVE = 7
};

/**
 * Values used to specify the outcome of a <code>TurnBasedMatch</code>
 * for a participant.
 */

enum class GPG_EXPORT MatchResult {
  DISAGREED = 1,
  DISCONNECTED = 2,
  LOSS = 3,
  NONE = 4,
  TIE = 5,
  WIN = 6
};

/**
 * Values used to specify the status of a <code>TurnBasedMatch</code> for the
 * local participant.
 */

enum class GPG_EXPORT MatchStatus {
  INVITED = 1,
  THEIR_TURN = 2,
  MY_TURN = 3,
  PENDING_COMPLETION = 4,
  COMPLETED = 5,
  CANCELED = 6,
  EXPIRED = 7
};

/**
 * A struct containing flags which can be provided to
 * {@link QuestManager::FetchList} in order to filter the results returned.
 */
struct GPG_EXPORT QuestFetchFlags {
  enum : int32_t {
    UPCOMING = 1 << 0,
    OPEN = 1 << 1,
    ACCEPTED = 1 << 2,
    COMPLETED = 1 << 3,
    COMPLETED_NOT_CLAIMED = 1 << 4,
    EXPIRED = 1 << 5,
    ENDING_SOON = 1 << 6,
    FAILED = 1 << 7,
    ALL = -1
  };
};

/** Values used to specify the <code>Quest</code> state. */

enum class GPG_EXPORT QuestState {
  UPCOMING = 1,
  OPEN = 2,
  ACCEPTED = 3,
  COMPLETED = 4,
  EXPIRED = 5,
  FAILED = 6
};

/** Values used to specify the QuestMilestone state. */

enum class GPG_EXPORT QuestMilestoneState {
  NOT_COMPLETED = 1,
  COMPLETED_NOT_CLAIMED = 2,
  CLAIMED = 3
};

/**
 * Values used to specify the type of update being reported by a
 * <code>TurnBasedMultiplayer</code> callback.
 *
 * @see <code>gpg::GameServices::Builder::SetOnTurnBasedMatchEvent</code>
 * @see <code>gpg::GameServices::Builder::SetOnMultiplayerInvitationEvent</code>
 */
enum class GPG_EXPORT TurnBasedMultiplayerEvent {
  /** A multiplayer match was updated while the app was running. */
  UPDATED,
  /**
   * A multiplayer match was updated, and the app was launched in response to
   * this update.
   */
  UPDATED_FROM_APP_LAUNCH,
  /** A match has been removed from the device, and should no longer be used. */
  REMOVED
};

/** Values used to specify the Snapshot conflict resolution policy.
  *
  * @see <code>gpg::SnapshotManager::Open</code>
  */
enum class GPG_EXPORT SnapshotConflictPolicy {
  MANUAL,
  LONGEST_PLAYTIME,
  BASE_WINS,
  REMOTE_WINS
};

}  // namespace gpg

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop  // GCC diagnostic ignored "-Wattributes"
#endif

#endif  // GPG_TYPES_H_
