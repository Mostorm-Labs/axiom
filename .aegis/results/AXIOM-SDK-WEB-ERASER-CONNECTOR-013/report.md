# External Web eraser modes and connector preset

The existing Runtime exposes object and partial erasing. The sample now selects each through existing Facade controls, checks the two receipts and reflects the active mode. The sample initializes its orthogonal connector preset before any interaction; previously it only set that preset after clicking Connector, so selection attachment clicks could use the Runtime straight default. No Runtime interface is changed.

The complete outside-repository build and actual Chrome test returned `PASS_SCOPED` (`raw/external-run.json`), with two expected negative package checks. A fresh browser run (`raw/browser-final.log`) checked selected mode and pixels for partial erase, whole-stroke object deletion, auto-created canonical orthogonal connector, movement off-axis and a rendered vertical segment, Undo and existing interaction regressions. Browser input was synthesized with CDP, not physical hardware. The before-fix missing buttons and missing orthogonal preset checks are retained as RED.

Straight horizontal or vertical rendering for aligned, clear endpoints remains expected. Physical touch, actual IME and full platform acceptance were not established by this report.
