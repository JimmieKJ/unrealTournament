/**
 * @file gpg/multiplayer_invitation.h
 * @copyright Copyright 2014 Google Inc. All Rights Reserved.
 * @brief Value object that represents an invitation to a turn based match.
 */

#ifndef GPG_MULTIPLAYER_INVITATION_H_
#define GPG_MULTIPLAYER_INVITATION_H_

#include <string>
#include <vector>
#include "gpg/multiplayer_participant.h"

namespace gpg {

class TurnBasedMatchImpl;

/**
 * A MultiplayerInvitationImpl is a TurnBasedMatchImpl.
 */
typedef TurnBasedMatchImpl MultiplayerInvitationImpl;

/**
 * A data structure containing data about the current state of an invitation to
 * a turn-based match.
 *
 * @ingroup ValueType
 */
class GPG_EXPORT MultiplayerInvitation {
 public:
  MultiplayerInvitation();

  /**
   * Constructs a <code>MultiplayerInvitation</code> from a
   * <code>shared_ptr</code> to a <code>MultiplayerInvitationImpl</code>.
   * Intended for internal use by the API.
   */
  explicit MultiplayerInvitation(
      std::shared_ptr<MultiplayerInvitationImpl const> impl);

  /**
   * Creates a copy of an existing <code>MultiplayerInvitation</code>.
   */
  MultiplayerInvitation(MultiplayerInvitation const &copy_from);

  /**
   * Moves an existing <code>MultiplayerInvitation</code> into a new one.
   */
  MultiplayerInvitation(MultiplayerInvitation &&move_from);

  /**
   * Assigns this <code>MultiplayerInvitation</code> by copying from another
   * one.
   */
  MultiplayerInvitation &operator=(MultiplayerInvitation const &copy_from);

  /**
   * Assigns this <code>MultiplayerInvitation</code> by moving another one into
   * it.
   */
  MultiplayerInvitation &operator=(MultiplayerInvitation &&move_from);

  /**
   * Returns true if this <code>MultiplayerInvitation</code> is populated with
   * data. Must be true in order for the getter functions (<code>Id</code>,
   * <code>Variant</code>, etc.) on this <code>MultiplayerInvitation</code>
   * object to be usable.
   */
  bool Valid() const;

  /**
   * Returns an ID that uniquely identifies this
   * <code>MultiplayerInvitation</code>. {@link Valid} must return true for this
   * function to be usable.
   */
  std::string const &Id() const;

  /**
   * Returns a game-specific variant identifier that a game can use to identify
   * game mode. {@link Valid} must return true for this function to be usable.
   */
  uint32_t Variant() const;

  /**
   * Returns the number of available auto-matching slots for the match for which
   * this object is an invitation. This value is equal to the number of
   * auto-matching slots with which the match was created, minus the number of
   * participants who have already been added via auto-matching. {@link Valid}
   * must return true for this function to be usable.
   */
  uint32_t AutomatchingSlotsAvailable() const;

  /**
   * Returns the time at which the <code>TurnBasedMatch</code> for this
   * invitation was created (expressed as milliseconds since the Unix epoch).
   * {@link Valid} must return true for this function to be usable.
   */
  std::chrono::milliseconds CreationTime() const;

  /**
   * Returns the participant who invited the local participant to the
   * <code>TurnBasedMatch</code> for this invitation. {@link Valid} must return
   * true for this function to be usable.
   */
  MultiplayerParticipant InvitingParticipant() const;

  /**
   * A vector of all participants in the <code>TurnBasedMatch</code> for this
   * invitation. {@link Valid} must return true for this function to be usable.
   */
  std::vector<MultiplayerParticipant> const &Participants() const;

 private:
  friend class TurnBasedMatchImpl;
  std::shared_ptr<MultiplayerInvitationImpl const> impl_;
};

}  // namespace gpg

#endif  // GPG_MULTIPLAYER_INVITATION_H_
