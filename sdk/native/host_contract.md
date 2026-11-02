# Native Host contract

A Host must provide the existing Runtime platform seam, not a parallel editor:

1. Create one `Session` and open a document with a full 128-bit ID.
2. On view attach, create independent canonical and transparent preview Skia
   providers and bind both through `RuntimePlatformAccess::core(owner)`.
3. Convert native events into `PlatformPointerBatch`; Runtime owns hit testing,
   AutoIntent, pressure, brush geometry, erase semantics and commit.
4. Service `RuntimeFrameOpportunity`; present provider work and echo the exact
   work/generation receipt through `finishFrameOpportunity` and
   `receivePlatformPresented`.
5. On resize/lost, cancel contacts and transition provider generations; on
   rebind, preserve document/History and reject stale commands.
6. On unmount, detach providers and input only. Close is an explicit Controller
   action after data custody and IME completion.

The provider performs surface/context/present/readback only. It does not own
semantic objects, selection, history, brush profiles or viewport intent.
