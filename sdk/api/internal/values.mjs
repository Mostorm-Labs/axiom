import {AxiomError} from '../axiom.mjs';

// The portable contract contains value DTOs and owned byte arrays only. Hermes
// need not provide the browser structuredClone API for control admission.
export function copyValue(value, ancestors = new Set()) {
  if (value === null || value === undefined || ['string', 'boolean'].includes(typeof value)) return value;
  if (typeof value === 'number' && Number.isFinite(value)) return value;
  if (Object.prototype.toString.call(value) === '[object Uint8Array]') return new Uint8Array(value);
  if (typeof value !== 'object' || ancestors.has(value)) throw new AxiomError('INVALID_ARGUMENT', 'Expected an acyclic value DTO');
  if (!Array.isArray(value) && Object.getPrototypeOf(value) !== Object.prototype && Object.getPrototypeOf(value) !== null)
    throw new AxiomError('INVALID_ARGUMENT', 'Expected a plain value DTO');
  ancestors.add(value);
  try {
    return Array.isArray(value) ? value.map(v => copyValue(v, ancestors))
      : Object.fromEntries(Object.entries(value).map(([key, v]) => [key, copyValue(v, ancestors)]));
  } finally { ancestors.delete(value); }
}
