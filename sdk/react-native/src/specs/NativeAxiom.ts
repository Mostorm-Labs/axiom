import type {TurboModule} from 'react-native';
import {TurboModuleRegistry} from 'react-native';

// These are low-frequency control/IO messages. Native pointer input stays native.
export interface Spec extends TurboModule {
  createSession(options: string, snapshot: ReadonlyArray<number>): Promise<string>;
  request(session: string, request: string, bytes: ReadonlyArray<number>): Promise<string>;
  close(session: string): Promise<void>;
}
export default TurboModuleRegistry.getEnforcing<Spec>('AxiomNative');
