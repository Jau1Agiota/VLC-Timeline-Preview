# Actual Wine compatibility measurements

These records describe cloud Wine 11 / official VLC 3.0.24 tests, not native Windows results. Environment, fixture generation, compiler provenance, policy and limitations are in [the investigation report](../../../../docs/STAGE_2_3_1_NEAR_END.md).

- `original.csv`: original helper; six target failures and three controls.
- `rejected_50ms.csv`: unsuccessful earlier-seek experiment; not delivered.
- `focused_tail.csv`: delivered tail policy; six target images manually read as 29,833 ms. Other observations remain blank.
- `final_artifact.csv`: fresh delivery-executable smoke run; six additional BMPs manually read as 29,833 ms.
- `deadline_controls.csv`: 12 earlier-position VFR controls and three 100 ms deadline runs.
- `regression.csv`: clean full 168-attempt video matrix. All image timestamps remain unreviewed in these two files, including separately repeated target cases. Deliberate deadline failures are not accuracy-gate inputs.
- `fixture_manifest.json`: hashes and metadata of the newly generated cloud fixtures. These are not claimed to match the user's original fixture bytes.

`observed_ms` comes only from reading actual captured BMP text. `effective_start_ms` is a seek request, never an observed timestamp. Empty observations are intentionally unverified. Image presence and exit zero mean candidate capture only. Original JSON/stderr, BMPs, execution harnesses and binaries are supplied in the accompanying downloadable evidence archive outside Git; they are not installed into VLC.
