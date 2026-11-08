# README assets

The two decorative illustrations were generated with Codex CLI's built-in image tool, one image per session. They do not represent product UI. Prompts and shared art direction are in `tools/readme-assets/manifest.mjs`; rerun `node tools/readme-assets/generate.mjs` from the repository root. Logs and full-resolution originals stay in the ignored `build/readme-assets` directory.

Both images were visually reviewed. Alpha was checked programmatically: both are fully opaque (255–255), with no semi-transparent pixels. They use a deliberate deep petrol backdrop. Images were resized with Lanczos and encoded as WebP at quality 88. No text or logo was generated.

`app-dark.webp` and `app-light.webp` are lossless conversions of real Qt test captures from GitHub Actions run 42485671206, commit 3599704. No UI was synthesized or retouched. The existing SVG logo is retained.

Exact image dimensions, alpha measurements, and byte sizes are recorded in `dimensions.json`. The four new images together are approximately 78 KB. Older banner and popup assets are retained for existing links but are no longer used by the README.
