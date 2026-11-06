export const SDK_VERSION = '0.4.1';
export class AxiomError extends Error {
  constructor(code, message = code, details = {}) {
    super(message); this.name = 'AxiomError'; this.code = code; this.details = details;
  }
}
