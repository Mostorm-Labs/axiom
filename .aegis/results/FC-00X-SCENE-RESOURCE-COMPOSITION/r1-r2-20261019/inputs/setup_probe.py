from pathlib import Path
r=Path(__file__).resolve().parent
old=Path(r"D:/axreview/fc00x-20261018/runtime-public-boundary-reverify-v5/boundary_probe.cpp").read_text()
old=old.replace('#include <thread>','#include <thread>\n#include <functional>')
old=old.replace('assert(c.finishFrameOpportunity({b.previewWork,true})&&c.previewRenderState().submissionCount==n);','assert(!c.finishFrameOpportunity({b.previewWork,true})&&c.previewRenderState().submissionCount==n);')
old=old.replace('assert(wa.previewWork==wb.previewWork&&accepted&&after==wb.previewRevision&&!b.previewRenderState().dirty);','assert(wa.previewWork!=wb.previewWork&&!accepted&&after==before&&b.previewRenderState().dirty); assert(b.finishFrameOpportunity({wb.previewWork,true}));')
i=old.index('int main(){')
extra=(r/'extra.cppfrag').read_text()
old=old[:i]+extra+'\nint main(){std::setvbuf(stdout,nullptr,_IONBF,0);publicEditingAndFences();diagnostics();idleResource();feedbackMapping();recoveryAndTarget();autoFrames();uiMode();restoreVariants();previewPending();mappingCapacity();previewRetryAndRestore();completionIdentity();crossInstanceCompletion();acceptedMetrics();activationFailure();canonicalCrossInstanceObservation();reentrantCloseThenResizeObservation();std::printf("independent residual oracle failures=%d\\n",remainingOracleFailures);return remainingOracleFailures;}\n'
(r/'boundary_probe.cpp').write_text(old)
(r/'build_probe.py').write_text(Path(r"D:/axreview/fc00x-20261018/runtime-public-boundary-reverify-v5/build_probe.py").read_text())


