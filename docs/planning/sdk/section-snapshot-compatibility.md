# Section snapshot compatibility — implementation candidate

Section objects remain the existing ObjectKind 10/version 1 and retain their
existing object-content field and wire representation. A document containing a
Section is encoded only in Snapshot schema 3, with the explicit reader capability
bit `0x10` and the `section-v1` entry appended to the canonical requirements list.
The combined current reader profile is `0x1f`. Readers that do not advertise the
bit reject the document before restore; schema 1/2 remain unable to carry Section.
Snapshots without Section retain their previous requirements and canonical bytes.
No historical descriptor or SDK lock is changed.

Runtime restore uses the existing checkpoint binding and hierarchy validation,
then rebuilds the Scene in a fresh owner before publishing it. Capture uses an
externally supplied revision and recovery frontier; the data owner remains
responsible for validating and persisting that checkpoint and its resources.
Restore does not rebuild History or replay operations, and the caller must replay
only operations after the matching frontier. Closing the Runtime does not wait for
external persistence. A synchronous local-commit notification is not a durable
acknowledgment or a network delivery guarantee.

The narrow tests cover legacy rejection, missing requirement rejection, unchanged
non-Section bytes, new-reader round trip and hierarchy restore, and old-reader
rejection without restored objects. This candidate and its evidence require review
as a compatibility extension before being treated as a released wire contract.
