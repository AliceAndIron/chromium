// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains type and function definitions relevant to Mojo invitation
// APIs.
//
// Note: This header should be compilable as C.

#ifndef MOJO_PUBLIC_C_SYSTEM_INVITATION_H_
#define MOJO_PUBLIC_C_SYSTEM_INVITATION_H_

#include <stdint.h>

#include "mojo/public/c/system/macros.h"
#include "mojo/public/c/system/platform_handle.h"
#include "mojo/public/c/system/system_export.h"
#include "mojo/public/c/system/types.h"

// Details regarding why an invited process has had its connection to this
// process terminated by the system. See |MojoProcessErrorHandler|.
struct MOJO_ALIGNAS(8) MojoProcessErrorDetails {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // The length of the string pointed to by |error_message| below.
  uint32_t error_message_length;

  // An error message corresponding to the reason why the connection was
  // terminated. This is an information message which may be useful to
  // developers.
  const char* error_message;
};
MOJO_STATIC_ASSERT(sizeof(MojoProcessErrorDetails) == 16,
                   "MojoProcessErrorDetails has wrong size.");

// An opaque process handle value which must be provided when sending an
// invitation to another process via a platform transport. See
// |MojoSendInvitation()|.
struct MOJO_ALIGNAS(8) MojoPlatformProcessHandle {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // The process handle value. For Windows this is a valid process HANDLE value.
  // For Fuchsia it must be |zx_handle_t| process handle, and for all other
  // POSIX systems, it's a PID.
  uint64_t value;
};
MOJO_STATIC_ASSERT(sizeof(MojoPlatformProcessHandle) == 16,
                   "MojoPlatformProcesHandle has wrong size.");

// Enumeration indicating the type of transport over which an invitation will be
// sent or received.
typedef uint32_t MojoInvitationTransportType;

// The channel transport type embodies common platform-specific OS primitives
// for FIFO message passing:
//   - For Windows, this is a named pipe.
//   - For Fuchsia, it's a channel.
//   - For all other POSIX systems, it's a Unix domain socket pair.
//
// See |MojoInvitationTransportHandle| for details.
#define MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL ((MojoInvitationTransportType)0)

// Similar to CHANNEL transport, but used for an endpoint which requires an
// additional step to accept an inbound connection. This corresponds to a
// bound listening socket on POSIX, or named pipe server handle on Windows.
//
// The remote endpoint should establish a working connection to the server side
// and wrap the handle to that connection using a CHANNEL transport.
#define MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL_SERVER \
  ((MojoInvitationTransportType)1)

// A transport endpoint over which an invitation may be sent or received via
// |MojoSendInvitation()| or |MojoAcceptInvitation()| respectively.
struct MOJO_ALIGNAS(8) MojoInvitationTransportEndpoint {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // The type of this transport endpoint. See |MojoInvitationTransportType|.
  MojoInvitationTransportType type;

  // The number of platform handles in |platform_handles| below.
  uint32_t num_platform_handles;

  // Platform handle(s) corresponding to the system object(s) backing this
  // endpoint. The concrete type of the handle(s) depends on |type|.
  //
  // For |MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL| endpoints:
  //   - On Windows, this is a single named pipe HANDLE
  //   - On Fuchsua, this is a single channel Fuchsia handle
  //   - On other POSIX systems, this is a single Unix domain socket file
  //     descriptor.
  const struct MojoPlatformHandle* platform_handles;
};
MOJO_STATIC_ASSERT_FOR_32_BIT(
    sizeof(MojoInvitationTransportEndpoint) == 16,
    "MojoInvitationTransportEndpoint has wrong size.");
MOJO_STATIC_ASSERT_FOR_64_BIT(
    sizeof(MojoInvitationTransportEndpoint) == 24,
    "MojoInvitationTransportEndpoint has wrong size.");

// Flags passed to |MojoCreateInvitation()| via |MojoCreateInvitationOptions|.
typedef uint32_t MojoCreateInvitationFlags;

// No flags. Default behavior.
#define MOJO_CREATE_INVITATION_FLAG_NONE ((MojoCreateInvitationFlags)0)

// Options passed to |MojoCreateInvitation()|.
struct MOJO_ALIGNAS(8) MojoCreateInvitationOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoCreateInvitationFlags|.
  MojoCreateInvitationFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(MojoCreateInvitationOptions) == 8,
                   "MojoCreateInvitationOptions has wrong size");

// Flags passed to |MojoAttachMessagePipeToInvitation()| via
// |MojoAttachMessagePipeToInvitationOptions|.
typedef uint32_t MojoAttachMessagePipeToInvitationFlags;

// No flags. Default behavior.
#define MOJO_ATTACH_MESSAGE_PIPE_TO_INVITATION_FLAG_NONE \
  ((MojoAttachMessagePipeToInvitationFlags)0)

// Options passed to |MojoAttachMessagePipeToInvitation()|.
struct MOJO_ALIGNAS(8) MojoAttachMessagePipeToInvitationOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoAttachMessagePipeToInvitationFlags|.
  MojoAttachMessagePipeToInvitationFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(MojoAttachMessagePipeToInvitationOptions) == 8,
                   "MojoAttachMessagePipeToInvitationOptions has wrong size");

// Flags passed to |MojoExtractMessagePipeFromInvitation()| via
// |MojoExtractMessagePipeFromInvitationOptions|.
typedef uint32_t MojoExtractMessagePipeFromInvitationFlags;

// No flags. Default behavior.
#define MOJO_EXTRACT_MESSAGE_PIPE_FROM_INVITATION_FLAG_NONE \
  ((MojoExtractMessagePipeFromInvitationFlags)0)

// Options passed to |MojoExtractMessagePipeFromInvitation()|.
struct MOJO_ALIGNAS(8) MojoExtractMessagePipeFromInvitationOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoExtractMessagePipeFromInvitationFlags|.
  MojoExtractMessagePipeFromInvitationFlags flags;
};
MOJO_STATIC_ASSERT(
    sizeof(MojoExtractMessagePipeFromInvitationOptions) == 8,
    "MojoExtractMessagePipeFromInvitationOptions has wrong size");

// Flags passed to |MojoSendInvitation()| via |MojoSendInvitationOptions|.
typedef uint32_t MojoSendInvitationFlags;

// No flags. Default behavior.
#define MOJO_SEND_INVITATION_FLAG_NONE ((MojoSendInvitationFlags)0)

// Options passed to |MojoSendInvitation()|.
struct MOJO_ALIGNAS(8) MojoSendInvitationOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoSendInvitationFlags|.
  MojoSendInvitationFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(MojoSendInvitationOptions) == 8,
                   "MojoSendInvitationOptions has wrong size");

// Flags passed to |MojoAcceptInvitation()| via |MojoAcceptInvitationOptions|.
typedef uint32_t MojoAcceptInvitationFlags;

// No flags. Default behavior.
#define MOJO_ACCEPT_INVITATION_FLAG_NONE ((MojoAcceptInvitationFlags)0)

// Options passed to |MojoAcceptInvitation()|.
struct MOJO_ALIGNAS(8) MojoAcceptInvitationOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoAcceptInvitationFlags|.
  MojoAcceptInvitationFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(MojoAcceptInvitationOptions) == 8,
                   "MojoAcceptInvitationOptions has wrong size");

#ifdef __cplusplus
extern "C" {
#endif

// A callback which may be invoked by the system when a connection to an invited
// process is terminated due to a communication error (i.e. the invited process
// has sent a message which fails some validation check in the system). See
// |MojoSendInvitation()|.
//
// |context| is the value of |context| given to |MojoSendInvitation()| when
// inviting the process for whom this callback is being invoked.
typedef void (*MojoProcessErrorHandler)(
    uintptr_t context,
    const struct MojoProcessErrorDetails* details);

// Creates a new invitation to be sent to another process.
//
// An invitation is used to invite another process to join this process's
// IPC network. The caller must already be a member of a Mojo network, either
// either by itself having been previously invited, or by being the Mojo broker
// process initialized via private Mojo APIs (i.e. the EDK).
//
// Invitations can have message pipes attached to them, and these message pipes
// are used to bootstrap Mojo IPC between the inviter and the invitee. See
// |MojoAttachMessagePipeToInvitation()| for attaching message pipes, and
// |MojoSendInvitation()| for sending an invitation.
//
// |options| controls behavior. May be null for default behavior.
// |invitation_handle| must be the address of storage for a MojoHandle value
//     to be output upon success.
//
// NOTE: If discarding an invitation instead of sending it with
// |MojoSendInvitation()|, you must close its handle (i.e. |MojoClose()|) to
// avoid leaking associated system resources.
//
// Returns:
//   |MOJO_RESULT_OK| if the invitation was created successfully. The new
//       invitation's handle is stored in |*invitation_handle| before returning.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |options| was non-null but malformed.
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if a handle could not be allocated for the
//       new invitation.
MOJO_SYSTEM_EXPORT MojoResult
MojoCreateInvitation(const struct MojoCreateInvitationOptions* options,
                     MojoHandle* invitation_handle);

// Attaches a message pipe endpoint to an invitation.
//
// This creates a new message pipe which will span the boundary between the
// calling process and the invitation's eventual target process. One end of the
// new pipe is attached to the invitation while the other end is returned to the
// caller. Every attached message pipe has an arbitrary |name| value which
// identifies it within the invitation.
//
// Message pipes can be extracted by the recipient by calling
// |MojoExtractMessagePipeFromInvitation()|. It is up to applications to
// communicate out-of-band or establish a convention for how attached pipes
// are named.
//
// |invitation_handle| is the invitation to which a pipe should be attached.
// |name| is an arbitrary name to give this pipe, required to extract the pipe
//     on the receiving end of the invitation. Note that the name is scoped to
//     this invitation only, so e.g. multiple invitations may attach pipes with
//     the name |0|, but any given invitation may only have a single pipe
//     attached with that name.
// |options| controls behavior. May be null for default behavior.
// |message_pipe_handle| is the address of storage for a MojoHandle value.
//     Upon success, the handle of the local endpoint of the new message pipe
//     will be stored here.
//
// Returns:
//   |MOJO_RESULT_OK| if the pipe was created and attached successfully. The
//       local endpoint of the pipe has its handle stored in
//       |*message_pipe_handle| before returning. The other endpoint is attached
//       to the invitation.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |invitation_handle| was not an invitation
//       handle, |options| was non-null but malformed, or |message_pipe_handle|
//       was null.
//   |MOJO_RESULT_ALREADY_EXISTS| if |name| was already in use for this
//       invitation.
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if a handle could not be allocated for the
//       new local message pipe endpoint.
MOJO_SYSTEM_EXPORT MojoResult MojoAttachMessagePipeToInvitation(
    MojoHandle invitation_handle,
    uint64_t name,
    const struct MojoAttachMessagePipeToInvitationOptions* options,
    MojoHandle* message_pipe_handle);

// Extracts a message pipe endpoint from an invitation.
//
// |invitation_handle| is the invitation from which to extract the endpoint.
// |name| is the name of the endpoint within the invitation. This corresponds
//     to the name that was given to |MojoAttachMessagePipeToInvitation()| when
//     the endpoint was attached.
// |options| controls behavior. May be null for default behavior.
// |message_pipe_handle| is the address of storage for a MojoHandle value.
//     Upon success, the handle of the extracted message pipe endpoint will be
//     stored here.
//
// Note that it is possible to extract an endpoint from an invitation even
// before the invitation has been sent to a remote process. If this is done and
// then the invitation is sent, the receiver will not see this endpoint as it
// will no longer be attached.
//
// Returns:
//   |MOJO_RESULT_OK| if a new local message pipe endpoint was successfully
//       created and returned in |*message_pipe_handle|. Note that the
//       association of this endpoint with an invitation attachment is
//       necessarily an asynchronous operation, and it is not known at return
//       whether an attachment named |name| actually exists on the invitation.
//       As such, the operation may still fail eventually, resuling in a broken
//       pipe, i.e. the extracted pipe will signal peer closure.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |invitation_handle| was not an invitation
//       handle, |options| was non-null but malformed, or |message_pipe_handle|
//       was null.
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if a handle could not be allocated for the
//       new local message pipe endpoint.
//   |MOJO_RESULT_NOT_FOUND| if it is known at call time that there is no pipe
//       named |name| attached to the invitation. This is possible if the
//       invtation was created within the calling process by
//       |MojoCreateInvitation()|.
MOJO_SYSTEM_EXPORT MojoResult MojoExtractMessagePipeFromInvitation(
    MojoHandle invitation_handle,
    uint64_t name,
    const struct MojoExtractMessagePipeFromInvitationOptions* options,
    MojoHandle* message_pipe_handle);

// Sends an invitation on a transport endpoint to bootstrap IPC between the
// calling process and another process.
//
// |invitation_handle| is the handle of the invitation to send.
// |process_handle| is an opaque, platform-specific handle to the remote
//     process. See |MojoPlatformProcessHandle|. This is not necessarily
//     required to be a valid process handle, but on some platforms (namely
//     Windows and Mac) it's important if the invitation target will need to
//     send or receive any kind of platform handles (including shared memory)
//     over Mojo message pipes.
// |transport_endpoint| is one endpoint of a platform transport primitive, the
//     other endpoint of which should be established within the process
//     corresponding to |*process_handle|. See |MojoInvitationTransportEndpoint|
//     for details.
// |error_handler_context| is an arbitrary value to be associated with this
//     process invitation. Its only relevance is that if the connection to this
//     invitee is terminated at any point due to e.g. a message validation
//     error, |error_handler| will be invoked with this value for its |context|
//     argument.
// |error_handler| is a function to invoke if the connection to the invitee is
//     ever terminated by the system due to certain error conditions, generally
//     constrained to message validation errors (i.e. the process is behaving
//     badly). May be null.
// |options| controls behavior. May be null for default behavior.
//
// This assumes ownership of any platform handles in |transport_endpoint| if
// and only if returning |MOJO_RESULT_OK|. In that case, |invitation_handle| is
// also invalidated.
//
// NOTE: It's pointless to send an invitation without at least one message pipe
// attached, so it is considered an error to attempt to do so.
//
// Returns:
//   |MOJO_RESULT_OK| if the invitation was successfully sent over the
//       transport. |invitation_handle| is implicitly closed. Note that this
//       does not guarantee that the invitation has been received by the target
//       yet, or that it ever will be (e.g. the target process may terminate
//       first or simply fail to accept the invitation).
//   |MOJO_RESULT_INVALID_ARGUMENT| if |invitation_handle| was not an invitation
//       handle, |transport_endpoint| was null or malformed, or |options| was
//       non-null but malformed.
//   |MOJO_RESULT_ABORTED| if the system failed to issue any necessary
//       communication via |transport_endpoint|, possibly due to a configuration
//       issue with the endpoint. The caller may attempt to correct this
//       situation and call again.
//   |MOJO_RESULT_FAILED_PRECONDITION| if there were no message pipes attached
//       to the invitation. The caller may correct this situation and call
//       again.
//   |MOJO_RESULT_UNIMPLEMENTED| if the transport endpoint type is not supported
//       by the system's version of Mojo.
MOJO_SYSTEM_EXPORT MojoResult MojoSendInvitation(
    MojoHandle invitation_handle,
    const struct MojoPlatformProcessHandle* process_handle,
    const struct MojoInvitationTransportEndpoint* transport_endpoint,
    MojoProcessErrorHandler error_handler,
    uintptr_t error_handler_context,
    const struct MojoSendInvitationOptions* options);

// Accepts an invitation from a transport endpoint to complete IPC bootstrapping
// between the calling process and whoever sent the invitation from the other
// end of the transport.
//
// |transport_endpoint| is one endpoint of a platform transport primitive, the
//     other endpoint of which should be established within a process
//     who has sent or will send an invitation via that endpoint. See
//     |MojoInvitationTransportEndpoint| for details.
// |options| controls behavior. May be null for default behavior.
// |invitation_handle| is the address of storage for a MojoHandle value. Upon
//     success, the handle of the accepted invitation will be stored here.
//
// Once an invitation is accepted, message pipes endpoints may be extracted from
// it by calling |MojoExtractMessagePipeFromInvitation()|.
//
// Note that it is necessary to eventually close (i.e. |MojoClose()|) any
// accepted invitation handle in order to clean up any associated system
// resources. If an accepted invitation is closed while it still has message
// pipes attached (i.e. not exracted as above), those pipe endpoints are also
// closed.
//
// Returns:
//   |MOJO_RESULT_OK| if the invitation was successfully accepted. The handle
//       to the invitation is stored in |*invitation_handle| before returning.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |transport_endpoint| was null or
//       malfored, |options| was non-null but malformed, or |invitation_handle|
//       was null.
//   |MOJO_RESULT_ABORTED| if the system failed to receive any communication via
//       |transport_endpoint|, possibly due to some configuration error. The
//       caller may attempt to correct this situation and call again.
//   |MOJO_RESULT_UNIMPLEMENTED| if the transport endpoint type is not supported
//       by the system's version of Mojo.
MOJO_SYSTEM_EXPORT MojoResult MojoAcceptInvitation(
    const struct MojoInvitationTransportEndpoint* transport_endpoint,
    const struct MojoAcceptInvitationOptions* options,
    MojoHandle* invitation_handle);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_C_SYSTEM_INVITATION_H_
