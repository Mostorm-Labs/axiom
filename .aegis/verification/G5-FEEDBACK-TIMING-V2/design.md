# Feedback timing v2

Verification owns this observation design;Runtime retains all semantic/render ownership. Scope is verification tooling only. Observer wraps real browser calls,records after successful return,tracks actual input-event and RAF callback boundaries,and schedules no background work. Content draw/blit is only an observed default-framebuffer-operation proxy;clear-only activity is retained separately. Neither is a camera-qualified or physical presentation receipt.

Legacy summaries remain available. New inputs carry exact starts/ends and per-call records. Unknown old data stays unknown. The right-censored no-content case cannot become a zero-latency pass. Last-event coarse receipt timing starts from actual last dispatch rather than the sampler's final awaited RAF.

Qualification uses deterministic hand-derived oracles plus real WebGL and unchanged normal installed SDKs. The real-browser clear-only control is an observer witness,not a replacement Runtime performance test. ABBA pilot uses whole-run/block units and never silently removes warmup,first-response,tail or long-task observations. A two-block pilot is diagnostic;it cannot establish nonregression.

A successor P31 must freeze at least8 independent blocks percase and8 prespecified family endpoints. The one-sided exact sign test uses binomial p=.5,with ties treated conservatively as non-wins. [NIST Sign Test](https://www.itl.nist.gov/div898/software/dataplot/refman1/auxillar/signtest.htm) provides the primary statistical reference. Family threshold is.05/8;8 uniform signs give1/256=.00390625. State the independence/common-median assumptions;ABBA does not eliminate nonlinear environmental drift. Keep raw extremes and absolute SLO findings alongside population-effect evidence.

This new verification design does not alter earlier failed verdicts. Return this stage before authorizing any Render coverage implementation.
