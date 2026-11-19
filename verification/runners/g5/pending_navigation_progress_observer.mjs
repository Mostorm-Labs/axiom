// Passive private transport facts. Calls only the original request; no polling,
// mutation, extra SDK invocation, or native instrumentation is introduced.
import {installLiveObserver} from './live_erase_feedback_observer.mjs';
export function installPendingObserver() {
  installLiveObserver();
  const install = window.idleObservation.engine.bind(window.idleObservation);
  const facts = []; let lost = 0;
  window.pendingObservation = {read: () => ({values: [...facts], lost})};
  window.idleObservation.engine = module => {
    install(module);
    const original = module._axiom_js_call;
    module._axiom_js_call = function (ptr, ...args) {
      const input = JSON.parse(module.UTF8ToString(ptr));
      const at = performance.now();
      const output = original.call(this, ptr, ...args);
      const answer = JSON.parse(module.UTF8ToString(output));
      if (input.command === 'cpuPending' || input.command === 'pendingReceipt') {
        if (facts.length < 4096) facts.push({at, end: performance.now(), command: input.command,
          action: input.action, phase: window.idleObservation.summary().phase, result: answer});
        else ++lost;
      }
      return output;
    };
  };
}
