import {AxiomError} from '../axiom.mjs';
import {copyValue} from './values.mjs';

// Private adapter. The underlying engine is never returned to consumers.
// No state, history, identity allocation or command semantics live here.
export function adaptController(engine) {
  let closed = false, closing = false;
  const subscriptions = new Set();
  const copy = copyValue;
  const check = () => {if (closed) throw new AxiomError('CLOSED'); if (closing) throw new AxiomError('BUSY');};
  const adapt = (source, root = false) => {
    const target = {};
    for (const [key, value] of Object.entries(source)) {
      if (root && key === 'canvas') continue;
      if (root && key === 'version') {target.version = value; continue;}
      if (root && key === 'close') {
        target.close = async () => {
          if (closed) return;
          if (closing) throw new AxiomError('BUSY');
          closing = true;
          // Invalidate already queued callbacks before any await.
          for (const subscription of subscriptions) subscription.active = false;
          try {await value.call(source); closed = true; for (const s of subscriptions) s.off(); subscriptions.clear();}
          catch (error) {for (const s of subscriptions) s.active = true; throw error;}
          finally {closing = false;}
        };
      } else if (root && ['subscribeState', 'subscribeOperations'].includes(key)) {
        target[key] = listener => {
          check();
          if (closing) throw new AxiomError('BUSY');
          if (typeof listener !== 'function') throw new AxiomError('INVALID_ARGUMENT');
          const s = {active:true, off:()=>{}};
          s.off = value.call(source, event => {
            const snapshot = copy(event);
            // Web SDK dispatch is already outside the native callback. Return
            // the listener result/exception so it retains custody on failure.
            if (s.active && !closed && !closing) return listener(snapshot);
          });
          subscriptions.add(s);
          return () => {s.active = false; s.off(); subscriptions.delete(s);};
        };
      } else if (typeof value === 'function') {
        target[key] = async (...args) => {check(); return value.apply(source, copy(args));};
      } else if (value && typeof value === 'object') target[key] = adapt(value);
    }
    return Object.freeze(target);
  };
  return adapt(engine, true);
}
