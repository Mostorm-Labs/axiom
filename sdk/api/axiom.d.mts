export const SDK_VERSION: '0.3.11';
export class AxiomError extends Error {code: string; details: Record<string,unknown>; constructor(code:string,message?:string,details?:Record<string,unknown>);}
export type Id128 = string;
export type Revision = string;
export interface ImageExportOptions {
  format?:'png'|'jpg'|'jpeg'; scale?:number; padding?:number; quality?:number;
  background?:'document'|'transparent'|Color; includePattern?:boolean;
  bounds?:[number,number,number,number]; generation?:Revision;
}
export interface ImageExportResult {
  bytes:Uint8Array; mimeType:'image/png'|'image/jpeg'; width:number; height:number;
  bounds:[number,number,number,number]; generation:Revision;
}
export interface CommitStamp {runtimeEpoch:Revision;ordinal:Revision;}
export type DataProgress = 0|1|2|3;
export interface OperationMetadata {version:1;owner:string;binding:Revision;operationId:Id128;documentId:Id128;kind:number;source:0|1|2|3;beforeGeneration:Revision;afterGeneration:Revision;commit:CommitStamp;localDurable:DataProgress;cloudSynced:DataProgress;}
export interface OperationPacket extends OperationMetadata {bytes:Uint8Array;}
export interface DataStatus {attached:boolean;closing:boolean;fault:boolean;pending:number;pendingBytes:number;capacity:number;byteCapacity:number;statusCapacity:4096;statusRecords:number;latest:OperationMetadata|null;releasedOrdinal:Revision;}
export interface Stamp {readonly owner:string;readonly token:[Revision,Revision,Revision,Revision,Revision,Revision,Revision,Revision];}
export type Color = [number,number,number,number];
export interface InkBrushConfig {version?:1|2;family:1|2|3|4|5|6;roundMode?:1|2;pressureSource?:1|2;size?:number;opacity?:number;color?:Color;markerAngle?:number;markerFlatness?:number;density?:number;textureScale?:number;deposition?:number;wetEdge?:number;laserFadeMs?:number;laserTailLength?:number;thinning?:number;streamline?:number;smoothing?:number;easing?:1|2|3|4;pressureModel?:1|2;startTaper?:number;endTaper?:number;startCap?:boolean;endCap?:boolean;}
export interface DocumentBackground {mode:'plain'|'dots'|'grid';color:Color;patternColor:Color;}
export type PropertyValue = {type:'boolean';value:boolean}|{type:'float';value:number}|{type:'color';value:Color}|{type:'fill';value:Color|null}|{type:'patternFill';value:{version:1;color:Color;kind:1|2;angle:number;gap:number;width:number}}|{type:'blend'|'decoration';value:number}|{type:'stroke';value:Stroke|null};
export interface Stroke {color:Color;width:number;cap:1|2|3;join:{kind:'miter';limit:number}|{kind:'round'|'bevel'};dash:{segments:number[];offset:number}|null;}
export interface MultiProperty {total:number;eligible:Id128[];excluded:{objectId:Id128;reason:number}[];mixed:boolean;partial:boolean;available:boolean;availability:number;strokeWidthAvailable:boolean;uniformValue:PropertyValue|null;}
export interface MultiProjection {available:boolean;stamp:Stamp;roots:Id128[];properties:Record<string,MultiProperty>;}
export interface ResourceToken {readonly owner:string;readonly instance:Revision;resourceId:Id128;documentBinding:Revision;providerEpoch:Revision;resourceVersion:Revision;expectedSha256:string;requestId:Revision;lifetimeGeneration:Revision;}
export interface ResourceCatalog {revision:Revision;binding:Revision;entries:(InitialResources['entries'][number]&{token:ResourceToken|null})[];}
export interface InitialResources {revision:Revision;binding:Revision;entries:{resourceId:Id128;kind:1|2|3;revision:Revision;sha256:string;byteLength:Revision;decodeVersion:number;layoutVersion:number;metadata:number[]}[];}
export interface TextToken {runtime:Revision;attachment:Revision;view:Revision;adapter:Revision;session:Revision;projection:Revision;document:Id128;object:Id128;row:Id128;column:Id128;}
export interface TextSnapshot {phase:0|1|2|3|4;token:TextToken;sequence:Revision;composition:Revision;effect:Revision;projection:number[];preedit:number[];selection:[number,number];replacement:[number,number];width:number;format:TextFormat;formatObject:Id128;mixedFormat:boolean;geometry:{available:boolean;caret:number[];selection:number[][];diagnostics:string[]};}
export interface FontEntry {id:Id128;family:string;revision:Revision;contentHash:string;byteLength:number;status:number;diagnostic:string;}
export interface FontCatalog {entries:FontEntry[];manifestRevision:Revision;bindingGeneration:Revision;}
export type Endpoint = {point:[number,number]}|{objectId:Id128;port?:1|2|3|4};
export interface ImageContent {resourceId:Id128;width:number;height:number;intrinsicWidth:number;intrinsicHeight:number;sourceRect:[number,number,number,number];}
export interface CommandResult {ok:true;requestId?:Revision;revision?:Revision;section?:{objectId:Id128;operationId:Id128;generation:Revision;canonical:number;projection:number;failure:number};}
export interface RuntimeState {documentId:Id128;target:{runtimeEpoch:Revision;viewId:Revision;viewEpoch:Revision;attachmentEpoch:Revision};generation:Revision;selectionRevision:Revision;selectionMode:0|1|2;eraserMode:1|2;tool:number;shape:number;creationMode:boolean;objects:number;activeContacts:number;selected:Id128[];primary:Id128;canUndo:boolean;canRedo:boolean;text:{phase:number;session:Revision;sequence:Revision;composition:Revision;selection:[number,number]};lastCommit:{operationId:Id128;runtimeEpoch:Revision;ordinal:Revision;presentedGeneration:Revision;localDurable:DataProgress;cloudSynced:DataProgress};data:Omit<DataStatus,'latest'>&{latest:Omit<OperationMetadata,'owner'>|null};table:{objectId:Id128;rowId:Id128;columnId:Id128};camera:{scale:number;x:number;y:number};surface:{available:boolean;width:number;height:number};}
export interface SectionOptions {objectId?:Id128;objects?:Id128[];parentId?:Id128|null;title?:string;boundary?:[number,number,number,number];generation?:Revision;selectionRevision?:Revision;}
export interface TextFormat {color?:[number,number,number,number];fontSize?:number;font?:Id128;bold?:boolean;italic?:boolean;underline?:boolean;strikethrough?:boolean;bulleted?:boolean;alignment?:1|2|3;}
export type AppearanceStamp = readonly [Revision,Revision,Revision,Revision,Revision,Revision,Revision,Revision];
export interface AppearanceValues {
 enabled:boolean;roughness:number;seed:Revision|null;styleVersion:1;fillKind:'none'|'solid'|'hachure'|'crosshatch';
 cornerRadius:number;fill:Color;stroke:Color;angle:number;gap:number;hatchWidth:number;strokeWidth:number;strokeEnabled:boolean;opacity:number;dash:number[];dashOffset:number;
}
export type AppearancePatch = Partial<Omit<AppearanceValues,'seed'|'styleVersion'>> & {reseed?:boolean};
export interface AppearanceSnapshot {available:boolean;writable:boolean;stamp:Stamp;eligible:Id128[];excluded:Id128[];fillEligible:Id128[];fillExcluded:Id128[];strokeEligible:Id128[];strokeExcluded:Id128[];radiusEligible:Id128[];radiusExcluded:Id128[];values:AppearanceValues;mixed:(keyof AppearanceValues)[];objects:{objectId:Id128;values:AppearanceValues}[];}
export interface AppearanceToken {readonly owner:string;readonly stamp:AppearanceStamp;readonly serial:Revision;}
export interface AppearanceResult extends CommandResult {applied:true;changed:boolean;generation:Revision;token:AppearanceToken;}
export interface AppearanceAPI {
 read():AppearanceSnapshot;readDefaults():AppearanceSnapshot;
 patch(patch:AppearancePatch,observation?:Pick<AppearanceSnapshot,'stamp'|'eligible'>):Promise<AppearanceResult>;
 setDefaults(patch:AppearancePatch):Promise<AppearanceResult>;
 begin(observation?:Pick<AppearanceSnapshot,'stamp'|'eligible'>):Promise<AppearanceResult>;
 updatePreview(token:AppearanceToken,patch:AppearancePatch):Promise<AppearanceResult>;
 commit(token:AppearanceToken):Promise<AppearanceResult>;cancel(token:AppearanceToken):Promise<AppearanceResult>;
}
export interface TableTarget {objectId:Id128;rowId:Id128;columnId:Id128;generation?:Revision;}
export type TableSnapshot = {available:false}|{available:true;editable:boolean;objectId:Id128;rowId:Id128;columnId:Id128;rows:{id:Id128;size:number}[];columns:{id:Id128;size:number}[];cells:{text:string;fill:Color;format:TextFormat;mixedFormat:boolean}[];grid:{version:1;color:Color;width:number};generation:Revision;};
export interface TableAPI {
 create(options?:{rows?:number;columns?:number;x?:number;y?:number;generation?:Revision}):Promise<CommandResult&{objectId:Id128;generation:Revision}>;
 navigate(direction:1|-1):Promise<CommandResult>;
 read(objectId?:Id128):TableSnapshot;select(target:TableTarget):Promise<CommandResult>;edit(target:TableTarget):Promise<TextSnapshot>;
 insertRow(options:{objectId:Id128;index:number;generation?:Revision}):Promise<CommandResult>;insertColumn(options:{objectId:Id128;index:number;generation?:Revision}):Promise<CommandResult>;
 removeRow(options:{objectId:Id128;rowId:Id128;generation?:Revision}):Promise<CommandResult>;removeColumn(options:{objectId:Id128;columnId:Id128;generation?:Revision}):Promise<CommandResult>;
 setRowHeight(options:{objectId:Id128;rowId:Id128;size:number;generation?:Revision}):Promise<CommandResult>;setColumnWidth(options:{objectId:Id128;columnId:Id128;size:number;generation?:Revision}):Promise<CommandResult>;
 setCellStyle(options:TableTarget&{fill?:Color;format?:Pick<TextFormat,'color'|'fontSize'|'font'|'bold'|'alignment'>}):Promise<CommandResult>;
 setGridStyle(options:{objectId:Id128;color?:Color;width?:number;generation?:Revision}):Promise<CommandResult>;
}
export type VectorPathCommand = {op:'move'|'line';point:[number,number]}|{op:'quad';control:[number,number];point:[number,number]}|{op:'cubic';control1:[number,number];control2:[number,number];point:[number,number]}|{op:'close'};
export interface VectorPathOptions {objectId?:Id128;commands:VectorPathCommand[];fillRule?:'nonzero'|'evenodd';x?:number;y?:number;}
export interface VectorPathSnapshot {available:boolean;objectId:Id128;commands:VectorPathCommand[];fillRule:'nonzero'|'evenodd';transform:[number,number,number,number,number,number];appearance:AppearanceValues;generation:Revision;}
interface ControlMethods {
  createVectorPath(options:VectorPathOptions):Promise<{objectId:Id128;generation:Revision}>;
  readVectorPath(objectId?:Id128):VectorPathSnapshot;
  table:TableAPI;
  appearance:AppearanceAPI;
  readonly version:'0.3.11';
  readBuildInfo():{ok:true;apiVersion:1;features:{shellAxis:1;resourceCustody:1;documentBackground:1;imageExport:1;table:1;sketchStyle:1;patternFill:1;appearancePreview:1;sketchExtended:1;vectorPath:1;roundedCorners:1;fontFallback:2;inkV2:1;compactStroke:1;materialInk:1;simulatedPressure:1;devicePressure:1;laserTransient:1};sdkVersion:string;sourceRevision:string;dirty:boolean;platform:string};
  readState():RuntimeState;
  readBackground():DocumentBackground;
  /** Read-only committed content; owned bytes. Default complete visible content, 16px padding. */
  exportImage(options?:ImageExportOptions):Promise<ImageExportResult>;
  setBackground(value:DocumentBackground):Promise<CommandResult>;
  subscribeOperations(listener:(packet:OperationPacket)=>void):()=>void;
  subscribeState(listener:(state:RuntimeState)=>void):()=>void;
  dispatchEvents():Promise<void>;
  data:{attach(options?:{capacity?:number;byteCapacity?:number}):DataStatus;status():DataStatus;
    readNext(after?:OperationMetadata|null):OperationPacket|null;
    release(through:OperationMetadata):CommandResult;
    reportStatus(packet:OperationMetadata,localDurable:DataProgress,cloudSynced:DataProgress):CommandResult;};
  readSelection():{count:number;roots:Id128[];primary:Id128;status:number;reason:number;groupScope:Id128|null;bounds:[number,number,number,number]|null;actions:{available:boolean;state:number;reason:number}[];generation:Revision;selectionRevision:Revision;kind:number;width:number;height:number;transform:[number,number,number,number,number,number]|null;appearance:{fill:Color;stroke:Color;strokeWidth:number;opacity:number;fillEnabled:boolean;strokeEnabled:boolean}|null};
  selectTool(name:'ink'|'select'|'objectEraser'|'partialEraser'|'hand'|'text'):Promise<CommandResult>;
  selectShape(name:'rectangle'|'ellipse'|'triangle'|'line'|'arrow'|'sticky'):Promise<CommandResult>;
  setSelectionMode(name:'point'|'rectangle'|'lasso'):Promise<CommandResult>;
  setInk(options:{size?:number;opacity?:number;color?:[number,number,number,number]}):Promise<CommandResult>;
  setEraser(options:{mode:1|2;diameter?:number}):Promise<CommandResult>;
  brushPresets():{profileId:string;revision:number}[];selectBrush(profileId:string,revision:number):Promise<CommandResult>;
  setBrushConfig(value:InkBrushConfig):Promise<CommandResult>;readBrushConfig():Required<InkBrushConfig>|null;
  setAppearance(options:{selected?:boolean;fill?:Color;stroke?:Color;fillEnabled?:boolean;strokeEnabled?:boolean;strokeWidth?:number;opacity?:number}):Promise<CommandResult>;
  orderSelection(front:boolean):Promise<CommandResult>;
  undo():Promise<CommandResult>;redo():Promise<CommandResult>;panBy(x:number,y:number):Promise<CommandResult>;zoomAt(x:number,y:number,scale:number):Promise<CommandResult>;
  fitContent():Promise<CommandResult>;fitSelection():Promise<CommandResult>;fitObject(id:Id128):Promise<CommandResult>;
  resizeSelection(width:number,height:number):Promise<CommandResult>;transformSelection(matrix:[number,number,number,number,number,number]):Promise<CommandResult>;
  deleteSelection():Promise<CommandResult>;lockSelection(value:boolean):Promise<CommandResult>;setSelectionVisible(value:boolean):Promise<CommandResult>;unlockSelection():Promise<CommandResult>;
  createSection(options?:SectionOptions):Promise<CommandResult>;wrapInSection(options:SectionOptions):Promise<CommandResult>;releaseSection(id:Id128):Promise<CommandResult>;reparent(options:SectionOptions):Promise<CommandResult>;resizeSection(id:Id128,boundary:[number,number,number,number]):Promise<CommandResult>;renameSection(id:Id128,title:string):Promise<CommandResult>;
  readSection(id?:Id128):{available:boolean;objectId:Id128;children:Id128[];roots:Id128[];groupScope:Id128|null;title:string;boundary:[number,number,number,number];generation:Revision};
  selectSection(id:Id128):Promise<CommandResult>;selectSectionChildren(id:Id128):Promise<CommandResult>;
  group():Promise<CommandResult>;ungroup(id?:Id128):Promise<CommandResult>;enterGroup(id?:Id128):Promise<CommandResult>;leaveGroup():Promise<CommandResult>;
  text:{read():TextSnapshot;fonts():FontCatalog;fontStatus(id:Id128):FontEntry|null;
    create(x:number,y:number,width?:number):Promise<TextSnapshot>;edit(id:Id128):Promise<TextSnapshot>;editAtPoint(x:number,y:number):Promise<TextSnapshot>;
    replace(text:string,range?:[number,number]):Promise<CommandResult>;select(start:number,length:number):Promise<CommandResult>;
    selectAll():Promise<TextSnapshot>;backspace():Promise<TextSnapshot>;delete():Promise<TextSnapshot>;moveLeft():Promise<TextSnapshot>;moveRight():Promise<TextSnapshot>;
    escape():Promise<TextSnapshot>;insertParagraph():Promise<TextSnapshot>;setWidth(width:number):Promise<TextSnapshot>;resync():Promise<TextSnapshot>;retry():Promise<TextSnapshot>;
    format(patch:TextFormat):Promise<void>;finish():Promise<boolean>;completeComposition():Promise<boolean>};
  multi:{read():MultiProjection;readValues(field:number,stamp:Stamp,eligible:Id128[]):{objectId:Id128;value:PropertyValue}[];setPermission(mode:'unconfigured'|'localSingleUser'|'contextUnavailable'):Promise<CommandResult>;setProperty(field:number,value:PropertyValue,stamp:Stamp,eligible:Id128[]):Promise<CommandResult>;setStrokeWidth(width:number,stamp:Stamp,eligible:Id128[]):Promise<CommandResult>};
  resources:{list():ResourceCatalog;readBytes(token:ResourceToken):Uint8Array;token(id:Id128):ResourceToken|null;receipt(id:Id128):{token:ResourceToken;state:number;byteLength:number;changed:boolean}|null;publish(token:ResourceToken,bytes:Uint8Array):Promise<CommandResult>};
  importImage(bytes:Uint8Array,mime:'image/png'|'image/jpeg'|'image/webp'):Promise<{objectId:Id128}>;
  replaceImage(bytes:Uint8Array,mime:'image/png'|'image/jpeg'|'image/webp'):Promise<{objectId:Id128}>;
  readSelectedImage():{available:boolean;editable:boolean;objectId:Id128;content:ImageContent};readCrop():{active:boolean;token:Revision;objectId:Id128;scale:number;content:ImageContent};
  resizeImage(id:Id128,width:number,height:number,freeStretch?:boolean):Promise<CommandResult>;
  beginCrop(id:Id128):Promise<{cropToken:Revision}>;updateCrop(options:{token:Revision;sourceRect:[number,number,number,number];width:number;height:number;scale?:number}):Promise<CommandResult>;
  commitCrop(token:Revision):Promise<CommandResult>;cancelCrop(token:Revision):Promise<CommandResult>;
  createConnector(options:{objectId?:Id128;start:Endpoint;end:Endpoint;routing?:1|2}):Promise<{objectId:Id128}>;
  setConnectorRouting(options:{objectId?:Id128;routing:1|2;generation?:Revision}):Promise<CommandResult>;
  readConnector(id?:Id128):{available:boolean;objectId:Id128;start:Endpoint;end:Endpoint;routing:1|2;routeStatus:number;route:[number,number][];generation:Revision};
  createSticky(options:{objectId?:Id128;size:number;x?:number;y?:number}):Promise<{objectId:Id128}>;readSelectedSticky():{available:boolean;objectId:Id128;width:number;height:number};
  captureSnapshot(metadata:{generation:Revision;documentRevision:Revision;recoveryFrontier:number[]}):Promise<{bytes:Uint8Array;generation:Revision;checkpointDigest:number[];throughCommit:CommitStamp}>;
  applyRemoteOperations(packets:{operationId:Id128;bytes:Uint8Array}[]):Promise<{ok:boolean;code?:string;disposition?:number;operationId?:Id128;commit?:CommitStamp}[]>;
  applyReplayOperation(bytes:Uint8Array):Promise<CommandResult>;
  applyReplayOperations(packets:{operationId:Id128;bytes:Uint8Array}[]):Promise<{ok:boolean;code?:string;disposition?:number;source?:4;operationId?:Id128;commit?:CommitStamp}[]>;
  applyRemoteOperation(bytes:Uint8Array):Promise<CommandResult>;close():Promise<void>;
}
/** All owner-dependent calls cross an asynchronous platform boundary. */
type AsyncMethods<T> = {readonly [K in keyof T]: T[K] extends (...args:infer A)=>infer R
 ? K extends 'subscribeState'|'subscribeOperations' ? T[K] : (...args:A)=>Promise<Awaited<R>>
 : T[K] extends object ? AsyncMethods<T[K]> : T[K]};
export type Controller = AsyncMethods<ControlMethods>;
export interface ControllerOptions {
 documentId:Id128; builtinFonts?:boolean; initialResources?:InitialResources;
 permission?:'unconfigured'|'localSingleUser'|'contextUnavailable';
 restore?:{bytes:Uint8Array;metadata:{documentRevision:Revision;recoveryFrontier:number[];checkpointDigest:number[];resourceBinding:Revision}};
 onError?:(error:AxiomError)=>void;
}
