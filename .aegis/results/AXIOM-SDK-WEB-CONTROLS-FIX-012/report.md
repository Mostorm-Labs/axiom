# External Web startup and selected-tool feedback

On this Windows host the previously used Python simple HTTP server returned `text/plain` for both `.mjs` modules. The browser rejected the modules, while the Runtime alone set READY; the page had no installed tool or canvas handlers. Chrome tests used a custom server with the correct MIME and therefore missed the deployment failure.

The exported `serve.py` explicitly serves ES modules as JavaScript. READY now waits for the browser input handlers; dynamic import errors show a visible failure. Applied tools are indicated with `aria-pressed` and blue styling; one-shot commands do not change the active tool.

- The full outside-repository Web build and browser runner returned `PASS_SCOPED` (`raw/browser.json`); two incompatible-package negative controls intentionally returned 1.
- `node verification/sdk_web_external/verify_manual_browser.mjs out/sdk-usable-controls-v1 <chrome>` returned 0. It used the exported server, real headless Chrome CDP pointer dispatch, verified rendered rectangle pixels and tool selection, and checked that incorrect MIME reports failure instead of READY. It also covered synthetic IME, Ink, selection, Group, image/crop, Section and wheel operations (`raw/browser-final.log`).
- `node verification/sdk_web_external/pointer_input_test.mjs` returned 0. The previously missing blue button and missing server checks failed in `raw/button-red.log` and `raw/server-red.log`.
- The user confirmed the new Rectangle button turns blue and a drawn rectangle is visible in the in-app browser. Other physical-input and IME results remain unverified.
