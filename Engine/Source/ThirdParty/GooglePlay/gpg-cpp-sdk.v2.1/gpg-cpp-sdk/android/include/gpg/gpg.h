/**
 * @file gpg/gpg.h
 * @brief Top-level header that includes all Play Games functionality.
 */

#ifndef GPG_GPG_H_
#define GPG_GPG_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include "gpg/achievement.h"
#include "gpg/achievement_manager.h"
#include "gpg/builder.h"
#include "gpg/common.h"
#include "gpg/common_default_callbacks.h"
#include "gpg/common_error_status.h"
#include "gpg/debug.h"
#include "gpg/default_callbacks.h"
#include "gpg/endpoint_discovery_listener_helper.h"
#include "gpg/event.h"
#include "gpg/event_manager.h"
#include "gpg/game_services.h"
#include "gpg/i_cross_app_endpoint_discovery_listener.h"
#include "gpg/i_endpoint_discovery_listener.h"
#include "gpg/i_message_listener.h"
#include "gpg/i_real_time_event_listener.h"
#include "gpg/leaderboard.h"
#include "gpg/leaderboard_manager.h"
#include "gpg/message_listener_helper.h"
#include "gpg/multiplayer_invitation.h"
#include "gpg/multiplayer_participant.h"
#include "gpg/nearby_connections.h"
#include "gpg/nearby_connections_builder.h"
#include "gpg/nearby_connections_status.h"
#include "gpg/nearby_connection_types.h"
#include "gpg/participant_results.h"
#include "gpg/platform_configuration.h"
#include "gpg/platform_defines.h"
#include "gpg/player.h"
#include "gpg/playerstats.h"
#include "gpg/player_level.h"
#include "gpg/player_manager.h"
#include "gpg/quest.h"
#include "gpg/quest_manager.h"
#include "gpg/quest_milestone.h"
#include "gpg/real_time_event_listener_helper.h"
#include "gpg/real_time_multiplayer_manager.h"
#include "gpg/real_time_room.h"
#include "gpg/real_time_room_config.h"
#include "gpg/real_time_room_config_builder.h"
#include "gpg/score.h"
#include "gpg/score_page.h"
#include "gpg/score_summary.h"
#include "gpg/snapshot_manager.h"
#include "gpg/snapshot_metadata.h"
#include "gpg/snapshot_metadata_change.h"
#include "gpg/snapshot_metadata_change_cover_image.h"
#include "gpg/snapshot_metadata_change_builder.h"
#include "gpg/stats_manager.h"
#include "gpg/status.h"
#include "gpg/turn_based_match.h"
#include "gpg/turn_based_match_config.h"
#include "gpg/turn_based_match_config_builder.h"
#include "gpg/turn_based_multiplayer_manager.h"
#include "gpg/types.h"

#if GPG_ANDROID

#include "gpg/android_initialization.h"
#include "gpg/android_support.h"

#endif  // GPG_ANDROID

#if GPG_IOS

#include "gpg/ios_support.h"

#endif  // GPG_IOS

#endif  // GPG_GAME_SERVICES_H_
