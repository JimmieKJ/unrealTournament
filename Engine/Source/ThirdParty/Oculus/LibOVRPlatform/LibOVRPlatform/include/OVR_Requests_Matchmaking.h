// This file was @generated with LibOVRPlatform/codegen/main. Do not modify it!

#ifndef OVR_REQUESTS_MATCHMAKING_H
#define OVR_REQUESTS_MATCHMAKING_H

#include "OVR_Types.h"
#include "OVR_Platform_Defs.h"

#include "OVR_MatchmakingStatApproach.h"
#include <stdbool.h>

/// \file
/// # Modes
///
/// There are three modes for matchmaking, each of which behaves very
/// differently.  However, the modes do share some of the same functions.
///
/// ## BOUT Mode
///
/// All users are in one pool, rooms are created for them once the match has
/// been made.  This mode is nice because users are all in the same queue which gives everyone maximum options.
///
/// ### Main flow
///
/// 1. Call ovr_Matchmaking_Enqueue.  You'll be continually re-enqueued until you join a room or cancel.
/// 2. Handle ovrMessage_Notification_Matchmaking_MatchFound notification
/// 3. Call ovr_Matchmaking_JoinRoom (on success) or ovr_Matchmaking_Cancel2 (on giving up)
/// 4. (for skill matching only) ovr_Matchmaking_StartMatch
/// 5. (for skill matching only) ovr_Matchmaking_ReportResultInsecure
///
/// ## ROOM Mode
///
/// There are two matchmaking queues.  Users are either hosting rooms or looking
/// for rooms hosted by someone else.
/// The host's room is created before the host is put into the matchmaking queue.
/// Room mode is not as good for matching as Bout mode because, as users are divided into
/// two queues, each person has fewer options.  Prefer Room mode when rooms themselves
/// have notable characteristics, such as when they outlive individual matches.
///
/// ### Main flow
///
/// For parties joining a room: as BOUT
/// For hosting a room:
///
/// 1. EITHER ovr_Matchmaking_CreateAndEnqueueRoom
///    OR ovr_Matchmaking_CreateRoom (handle response) and then ovr_Matchmaking_EnqueueRoom
///    Your room will be continually re-enqueued until you cancel or a match is found.
/// 2. Handle ovrMessage_Notification_Matchmaking_MatchFound notification or call ovr_Matchmaking_Cancel2
/// 3. (for skill matching only) ovr_Matchmaking_StartMatch
/// 4. (for skill matching only) ovr_Matchmaking_ReportResultInsecure
///
/// ## BROWSE Mode
///
/// As in Room mode, there is a queue of rooms looking for users to join them.  Users are enqueued just as
/// in Room mode, but in addition, they are returned a list of Rooms that are acceptable matches.
/// Filtering on the server side (such as for ping time or skill) happens just as in the other modes.  If you are using ping time optimization,
/// it may take a few seconds before anything is returned, as we perform test pings, which takes some time.
/// This mode has some disadvantages relative to BOUT and ROOM mode: there are more race conditions, for example when multiple users try
/// simultaneously to join the same room, or when a user attempts to join a room that has canceled.  However, this mode is
/// provided for when users should be given a choice of which room to join.
///
/// ### Main flow
///
/// For parties joining a room:
///
/// 1. Call ovr_Matchmaking_Browse any time you want a refreshed list of what's on the server.
/// 2. Handle ovrMessage_Matchmaking_Browse; choose a room from among the returned list
/// 3. Call ovr_Matchmaking_JoinRoom or ovr_Matchmaking_Cancel2 when the user is done searching and has chosen a room or given up.
/// 4. (for skill matching only) ovr_Matchmaking_StartMatch
/// 5. (for skill matching only) ovr_Matchmaking_ReportResultInsecure
///
/// For hosting a room: as ROOM mode
///
/// # Pools
///
/// 'pool' - this parameter for all APIs refers to a configuration for a game mode you set up in advance
/// at https://dashboard.oculus.com/application/[YOUR_APP_ID]/matchmaking
/// You can have many pools for a given application, and each will have a completely separate queue.
///
/// # Features
///
/// The matchmaking service supports several built-in features as well as 'custom criteria' - see below.
/// The best way to learn about currently supported features is to configure your pool at
/// https://dashboard.oculus.com/application/[YOUR_APP_ID]/matchmaking
/// NOTE: more than 2-player matchmaking is not yet supported for ROOM or BOUT mode, but it's coming soon.
///
/// # Match quality
///
/// Each rule (either your Custom criteria or built-in rules like skill matching and ping time optimization) yields a number
/// between 0 and 1.  These values are multiplied together to get a value between 0 and 1.  0.5 is considered to be
/// a marginal match, 0.9 an excellent match.
/// When a user is first enqueued, we have high standards (say, 0.9 or above).  As time goes on, standards decrease until we will allow
/// marginal matches (say, 0.5 or above).
///
/// ## Example 1
///
/// Two users have a peer-to-peer network round trip time of 50ms.  You configured your application to say that 100ms is a good time.  So let's say that 50ms
/// yields a really good value of 0.95.
/// User A has a medium-importance criterion (see "Custom criteria") that user B wields a banana katana, but B does not have one, yielding a value of 0.75.
/// The total match quality from A's perspective is 0.95 * 0.75 = .71 .  Thus, A and B will be matched but not before waiting a bit to
/// see if better matches come along.
///
/// ## Example 2
///
/// User A has a high-importance criterion (see "Custom criteria") that user B speaks English, but user B doesn't, yielding a value of 0.6 from A's perspective.
/// User A and B are a decent skill match, but not superb, yielding a value of 0.8.
/// Users A and B both require that each other are members of the Awesome Guild, and they both are, yielding a value of 1.0.
/// The total is 1.0 * 0.8 * 0.6 = 0.48.  This is less than 0.5 so the users are unlikely to ever be matched.
///
/// # Custom criteria
///
/// A room or user can both (a) specify facts about itself as well as (b) criteria (or filters) that it requires or
/// would prefer of ones it is matched with.  So "criteria" are applied to prospective matches and "query data"
/// describe yourself.
/// Queries are specified in advance at https://dashboard.oculus.com/application/[YOUR_APP_ID]/matchmaking,
/// and referenced here.
///
/// Each rule gives a value of 1.0 if it passes, but has a different "failure value" depending on its importance.
/// These are combined as described in the "Match quality" section above.
///
/// * Required - match will not occur if there's no match.  Failure value: 0.0
/// * High - match can be made if one high-importance rule fails.  Failure value: 0.59
/// * Medium - match can be made if two medium-importance rules fail.  Failure value: 0.77
/// * Low - match can be made if three low-importance rules miss, or one medium- and one low-importance rule misses.
///   Failure value: 0.84.
///
/// ## Example (C++)
///
///     static ovrKeyValuePair makePair(const char* key, int value) {
///       ovrKeyValuePair ret;
///       ret.key = key;
///       ret.valueType = ovrKeyValuePairType_Int;
///       ret.intValue = value;
///       return ret;
///     }
///
///     static ovrKeyValuePair makePair(const char* key, const char* value) {
///       ovrKeyValuePair ret;
///       ret.key = key;
///       ret.valueType = ovrKeyValuePairType_String;
///       ret.stringValue = value;
///       return ret;
///     }
///
///     static ovrKeyValuePair makePair(const char* key, double value) {
///       ovrKeyValuePair ret;
///       ret.key = key;
///       ret.valueType = ovrKeyValuePairType_Double;
///       ret.doubleValue = value;
///       return ret;
///     }
///
///     // facts about this user/room.  They will be vetted by any other party user
///     // might want to match with this one.
///     // The keys below were specified in the Matchmaking section at developer2.oculus.com
///     ovrKeyValuePair queryData[3] = {
///       makePair("value_of_pi", 3.14159),
///       makePair("num_dogs", 10),
///       makePair("favorite_hashtag", "YOLO"),
///     };
///
///     // criteria names were specified in the Matchmaking section at developer2.oculus.com
///     // This is what the user is requiring of the other user.
///     ovrMatchmakingCriterion queryCriteria[3] = {
///       // range [3.14, 3.15)
///       {"pi_close_enough", ovrMatchmaking_ImportanceHigh},
///       // favorite_hashtag == "YOLO"
///       {"yolo_favorite_hashtag", ovrMatchmaking_ImportanceRequired},
///       // num_dogs >= 7
///       {"sufficient_dogs", ovrMatchmaking_ImportanceLow}
///     };
///
///     ovrMatchmakingCustomQueryData customQuery = {
///       queryData,
///       3,
///       queryCriteria,
///       3
///     };
///
///     ovr_Matchmaking_Enqueue("my_pool", &customQuery);
///
/// # Debugging
///
/// To debug what's going on with your matchmaking pool, you can get snapshots of the queues using an
/// HTTP endpoint called matchmaking_pool_for_admin.  This endpoint is not intended to be called in
/// production.  Below examples will illustrate how to use it, and then there's a reference below
///
/// ## User-oriented snapshot
/// Find out what scores the logged-in user will assign to other users in the queue, and vice-versa.
///
///     // Global-scope somewhere.  This function is exported from our DLL but is not put in the headers
///     // because it is not intended for use in production.
///     OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_GraphAPI_Get(const char *url);
///
///     // In your code
///
///     ovrMatchmakingEnqueueResultHandle enqueueUserResult = ovr_Message_GetMatchmakingEnqueueResult(message);
///
///     auto requestHash = ovr_MatchmakingEnqueueResult_GetRequestHash(enqueueUserBResult);
///
///     // We can now inspect the queue to debug it.
///     char url[256];
///     snprintf(
///         url,
///         256,
///         "matchmaking_pool_for_admin/?pool=%s&trace_id=%s&fields=candidates,my_current_threshold",
///         matchmakingBoutWithQueryPool,
///         requestHash
///     );
///
///     ovr_GraphAPI_Get(url);
///
///     // In your handler
///     case ovr_GraphAPI_Get:
///         if (!ovr_Message_IsError(message)) {
///             const char * resultAsString = ovr_Message_GetString(message);
///             cout << "matchmaking_pool_for_admin results:" << endl << resultAsString << endl;
///
///             // Uses https://github.com/open-source-parsers/jsoncpp
///             Json::Reader reader;
///             Json::Value resultAsJson;
///             reader.parse(resultAsString, resultAsJson);
///             if (resultAsJson["candidates"][0]["can_match"].asBool()) {
///                 cout << "Yay!" << endl;
///             }
///         }
///
/// ## Reference
/// The following fields are currently exported:
///
/// candidates - A list of all the entries in the queue that the logged-in user is choosing among,
///     along with metadata about them
/// my_current_threshold - The minimum score (between 0 and 1) that is required for the logged-in
///     user to want to be matched with someone.  This number may vary based on factors like how long
///     the user has been enqueued.
///
///
///
///

/// Modes: BROWSE
/// 
/// Return a list of matchmaking rooms in the current pool filtered by skill
/// and ping (if enabled). This also enqueues the user in the matchmaking
/// queue. When the user has made a selection, call ovr_Matchmaking_JoinRoom on
/// one of the rooms that was returned. If the user stops browsing, call
/// ovr_Matchmaking_Cancel2.
/// 
/// In addition to the list of rooms, enqueue results are also returned. Call
/// ovr_MatchmakingBrowseResult_GetEnqueueResult to obtain them. See
/// OVR_MatchmakingEnqueueResult.h for details.
/// \param pool A BROWSE type matchmaking pool.
/// \param customQueryData Optional. Custom query data.
///
/// A message with type ::ovrMessage_Matchmaking_Browse will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// If no error occurred, the message will contain a payload of type ::ovrMatchmakingBrowseResultHandle.
/// Extract the payload from the message handle with ::ovr_Message_GetMatchmakingBrowseResult().
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_Matchmaking_Browse(const char *pool, ovrMatchmakingCustomQueryData *customQueryData);

/// DEPRECATED. Use Cancel2.
/// \param pool The pool in question.
/// \param requestHash Returned from ovr_Matchmaking_[CreateAnd]Enqueue[Room], which is used to find your entry in the queue.
///
/// A message with type ::ovrMessage_Matchmaking_Cancel will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// This response has no payload. If no error occured, the request was successful. Yay!
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_Matchmaking_Cancel(const char *pool, const char *requestHash);

/// Modes: BOUT, BROWSE, ROOM
/// 
/// Makes a best effort to cancel a previous Enqueue request before a match
/// occurs. Typically triggered when a user gives up waiting, or, for BROWSE
/// mode, when the host of a room wants to begin the game. If you don't cancel
/// but the user goes offline, the user/room will be timed out of the queue
/// within 30 seconds.
///
/// A message with type ::ovrMessage_Matchmaking_Cancel2 will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// This response has no payload. If no error occured, the request was successful. Yay!
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_Matchmaking_Cancel2();

/// Modes: BROWSE, ROOM
/// 
/// See overview documentation above.
/// 
/// Create a matchmaking room, join it, and enqueue it. This is the preferred
/// method. But, if you do not wish to automatically enqueue the room, you can
/// call CreateRoom instead.
/// 
/// Visit https://developer2.oculus.com/application/[YOUR_APP_ID]/matchmaking
/// to set up pools and queries
/// \param pool The matchmaking pool to use, which is defined for the app.
/// \param maxUsers DEPRECATED: This will not do anything.  Max Users is defined per pool.
/// \param subscribeToUpdates If true, sends a message with type ovrMessage_RoomUpdateNotification when the room data changes, such as when users join or leave.
/// \param customQueryData Optional.  See "Custom criteria" section above.
///
/// A message with type ::ovrMessage_Matchmaking_CreateAndEnqueueRoom will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// If no error occurred, the message will contain a payload of type ::ovrMatchmakingEnqueueResultAndRoomHandle.
/// Extract the payload from the message handle with ::ovr_Message_GetMatchmakingEnqueueResultAndRoom().
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_Matchmaking_CreateAndEnqueueRoom(const char *pool, unsigned int maxUsers, bool subscribeToUpdates, ovrMatchmakingCustomQueryData *customQueryData);

/// Create a matchmaking room and join it, but do not enqueue the room. After
/// creation, you can call EnqueueRoom. However, Oculus recommends using
/// CreateAndEnqueueRoom instead.
/// 
/// Visit https://developer2.oculus.com/application/[YOUR_APP_ID]/matchmaking
/// to set up pools and queries
/// \param pool The matchmaking pool to use, which is defined for the app.
/// \param maxUsers DEPRECATED: This will not do anything.  Max Users is defined per pool.
/// \param subscribeToUpdates If true, sends a message with type ovrMessage_RoomUpdateNotification when room data changes, such as when users join or leave.
///
/// A message with type ::ovrMessage_Matchmaking_CreateRoom will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// If no error occurred, the message will contain a payload of type ::ovrRoomHandle.
/// Extract the payload from the message handle with ::ovr_Message_GetRoom().
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_Matchmaking_CreateRoom(const char *pool, unsigned int maxUsers, bool subscribeToUpdates);

/// Modes: BOUT, ROOM (User only)
/// 
/// See overview documentation above.
/// 
/// Enqueue yourself to await an available matchmaking room. The platform
/// returns a ovrMessage_MatchmakingMatchFoundNotification message when a match
/// is found. Call ovr_Matchmaking_JoinRoom on the returned room. The response
/// contains useful information to display to the user to set expectations for
/// how long it will take to get a match.
/// 
/// If the user stops waiting, call ovr_Matchmaking_Cancel2.
/// \param pool The pool to enqueue in.
/// \param customQueryData Optional.  See "Custom criteria" section above.
///
/// A message with type ::ovrMessage_Matchmaking_Enqueue will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// If no error occurred, the message will contain a payload of type ::ovrMatchmakingEnqueueResultHandle.
/// Extract the payload from the message handle with ::ovr_Message_GetMatchmakingEnqueueResult().
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_Matchmaking_Enqueue(const char *pool, ovrMatchmakingCustomQueryData *customQueryData);

/// Modes: BROWSE (for Rooms only), ROOM
/// 
/// See the overview documentation above. Enqueue yourself to await an
/// available matchmaking room. ovrMessage_MatchmakingMatchFoundNotification
/// gets enqueued when a match is found.
/// 
/// The response contains useful information to display to the user to set
/// expectations for how long it will take to get a match.
/// 
/// If the user stops waiting, call ovr_Matchmaking_Cancel2.
/// \param roomID Returned either from ovrMessage_MatchmakingMatchFoundNotification or from ovr_Matchmaking_CreateRoom.
/// \param customQueryData Optional.  See the "Custom criteria" section above.
///
/// A message with type ::ovrMessage_Matchmaking_EnqueueRoom will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// If no error occurred, the message will contain a payload of type ::ovrMatchmakingEnqueueResultHandle.
/// Extract the payload from the message handle with ::ovr_Message_GetMatchmakingEnqueueResult().
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_Matchmaking_EnqueueRoom(ovrID roomID, ovrMatchmakingCustomQueryData *customQueryData);

/// Gets the matchmaking stats for the current user
/// 
/// Given a pool it will look up the current user's wins, loss, draws and skill
/// level. The skill level return will be between 1 and maxLevel. The approach
/// will dictate how should the skill level rise toward the max level.
/// \param pool The pool to look in
/// \param maxLevel The maximum skill level achievable
/// \param approach The growth function of how the skill levels should approach to the max level.  TRAILING is recommended for displaying to users
///
/// A message with type ::ovrMessage_Matchmaking_GetStats will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// If no error occurred, the message will contain a payload of type ::ovrMatchmakingStatsHandle.
/// Extract the payload from the message handle with ::ovr_Message_GetMatchmakingStats().
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_Matchmaking_GetStats(const char *pool, unsigned int maxLevel, ovrMatchmakingStatApproach approach);

/// Modes: BOUT, BROWSE, ROOM
/// 
/// Joins a room returned by a previous call to ovr_Matchmaking_Enqueue or
/// ovr_Matchmaking_Browse.
/// \param roomID ID of a room previously returned from ovrMessage_MatchmakingMatchFoundNotification or ovr_Message_MatchmakingBrowse.
/// \param subscribeToUpdates If true, sends a message with type ovrMessage_RoomUpdateNotification when room data changes, such as when users join or leave.
///
/// A message with type ::ovrMessage_Matchmaking_JoinRoom will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// If no error occurred, the message will contain a payload of type ::ovrRoomHandle.
/// Extract the payload from the message handle with ::ovr_Message_GetRoom().
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_Matchmaking_JoinRoom(ovrID roomID, bool subscribeToUpdates);

/// Modes: BOUT, BROWSE, ROOM
/// 
/// See the overview documentation above.
/// 
/// Call this after calling ovr_Matchmaking_StartMatch to begin a rated skill
/// match and after the match finishes. The service will record the result and
/// update the skills of all players involved, based on the results. This
/// method is insecure because, as a client API, it is susceptible to tampering
/// and therefore cheating to manipulate skill ratings.
///
/// A message with type ::ovrMessage_Matchmaking_ReportResultInsecure will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// This response has no payload. If no error occured, the request was successful. Yay!
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_Matchmaking_ReportResultInsecure(ovrID roomID, ovrKeyValuePair *data, unsigned int numItems);

/// Modes: BOUT, BROWSE, ROOM
/// 
/// For pools with skill-based matching. See overview documentation above.
/// 
/// Call after calling ovr_Matchmaking_JoinRoom when the players are present to
/// begin a rated match for which you plan to report the results (using
/// ovr_Matchmaking_ReportResultInsecure).
///
/// A message with type ::ovrMessage_Matchmaking_StartMatch will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// This response has no payload. If no error occured, the request was successful. Yay!
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_Matchmaking_StartMatch(ovrID roomID);

#endif
