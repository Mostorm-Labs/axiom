export const SDK_VERSION = '0.3.11';
export class AxiomError extends Error {
  constructor(code, message = code, details = {}) {
    super(message); this.name = 'AxiomError'; this.code = code; this.details = details;
  }
}
