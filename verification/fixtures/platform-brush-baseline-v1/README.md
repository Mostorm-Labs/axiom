# platform-brush-baseline-v1

This is the shared A/B corpus for the Ink Playground platform adapters. It is
one fixed `vector-solid-v1` package plus a single-finger stroke and a two-finger
viewport claim sequence. Platform raw IDs are mapped through `logical_sources`;
the comparator never treats an Android pointer ID, DOM pointer ID, or Win32 ID
as canonical identity by itself.

Coordinates and pressure are canonicalized at 1/1000 before comparison.
