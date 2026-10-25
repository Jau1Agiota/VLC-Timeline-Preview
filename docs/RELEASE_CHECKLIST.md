# Future Windows portable release gate

**Status: blocked; no publishing workflow or release permission is added.**

The application is an unofficial VideoLAN modification. Keep VLC's original
README, COPYING, COPYING.LIB, AUTHORS, THANKS and component-specific notices.
Do not imply VideoLAN endorsement. Source-only tools are distinct from a complete
application package and must not be presented as the application download.

Before an authorized public release:

- Resolve the existing original-runtime corresponding-source and bundled binary
  inventory obligations documented in `original-runtime/REVIEW.md` and the
  Stage 2.4.8 report. Match revisions, patches, configurations, static dependencies,
  copyright notices and source-delivery instructions to the actual binary.
- Obtain human redistribution review and explicit user approval. GPL licensing
  itself is not a prohibition, but missing source evidence is not clearance.
- Pass physical Windows acceptance, including offline first launch and optional
  association/default-selection tests. Record the exact source/driver commits.
- Prepare one versioned Windows x64 ZIP retaining `Start VLC Timeline Preview.exe`,
  verified original runtime pin, relative paths, license notices and complete
  corresponding-source delivery. Never use the private-prototype false-clearance
  manifest as a public approval artifact.
- Independently verify payload inventory, direct/recursive imports, all file hashes,
  ZIP readback and final SHA-256 sidecar. No debug caches, private user profiles,
  temporary downloads or unnecessary archives in the application ZIP.
- Draft installation notes: extract to a writable folder; no installer/elevation;
  x64 Vulkan loader prerequisite; first launch offline; optional per-user registry
  changes only; uninstall registration before moving/deleting.
- Only after separate approval, design a least-privilege manual publishing job
  using the normal Actions token, versioned filenames, checksum/source links,
  no overwrites and explicit gate review. Never use a personal access token to
  bypass this stage's restrictions.

Stage E contains no Release upload code, `contents: write`, tag creation or
public third-party binary delivery. The tools-only pilot uses `contents: read`
and temporary Actions artifacts, and is not a release-clearance test.
