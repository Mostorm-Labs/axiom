export const SDK_SEMVER = '0.5.0';
export const SDK_BUILD_NUMBER = 0;
export const SDK_VERSION = '0.5.0.0';
export class AxiomError extends Error {
  constructor(code, message = code, details = {}) {
    super(message); this.name = 'AxiomError'; this.code = code; this.details = details;
  }
}
