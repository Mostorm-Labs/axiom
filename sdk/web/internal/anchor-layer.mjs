import {AxiomError} from '../../api/axiom.mjs';

function anchorId(value) {
  if (typeof value !== 'string' || !/^[0-9a-f]{32}$/.test(value) || /^0+$/.test(value)) {
    throw new AxiomError('INVALID_ARGUMENT', 'Expected a nonzero anchor Id128');
  }
  return value;
}

function copyRect(rect) {
  if (Array.isArray(rect) && rect.length === 4) {
    const [x, y, width, height] = rect;
    if ([x, y, width, height].every(Number.isFinite) &&
        width > 0 && height > 0 &&
        Number.isFinite(x + width) && Number.isFinite(y + height) &&
        x + width > x && y + height > y) {
      return [x, y, width, height];
    }
  }
  throw new AxiomError('INVALID_ARGUMENT', 'Expected [x, y, width, height] in world units with positive size');
}

function setRect(element, [x, y, width, height]) {
  Object.assign(element.style, {
    left: `${x}px`, top: `${y}px`, width: `${width}px`, height: `${height}px`,
  });
}

export function attachAnchorLayer(mount, initialViewport, detach, newId) {
  const doc = mount.ownerDocument;
  const clip = doc.createElement('div');
  clip.className = 'axiom-anchor-layer';
  Object.assign(clip.style, {
    position: 'absolute', left: '0', top: '0', zIndex: '3',
    overflow: 'hidden', pointerEvents: 'none',
  });
  const world = doc.createElement('div');
  Object.assign(world.style, {
    position: 'absolute', left: '0', top: '0', transformOrigin: '0 0',
  });
  clip.append(world);
  const anchors = new Map();
  let disposed = false;
  let viewport = null;

  function setViewport(value) {
    if (disposed) return;
    viewport = value;
    clip.style.display = value ? '' : 'none';
    if (value) {
      clip.style.width = `${value.width}px`;
      clip.style.height = `${value.height}px`;
      world.style.transform = `matrix(${value.transform.join(',')})`;
    }
  }

  function ensureOpen() {
    if (disposed) throw new AxiomError('CLOSED');
  }

  function snapshot({id, rect}) {
    return {id, rect: [...rect]};
  }

  const api = Object.freeze({
    create(options) {
      ensureOpen();
      if (!options || typeof options !== 'object' || Array.isArray(options) ||
          Object.keys(options).some(key => key !== 'id' && key !== 'rect')) {
        throw new AxiomError('INVALID_ARGUMENT', 'Expected anchor options with rect and optional id');
      }
      const rect = copyRect(options.rect);
      const id = anchorId(options.id === undefined ? newId() : options.id);
      if (anchors.has(id)) throw new AxiomError('INVALID_ARGUMENT', 'Anchor ID already exists');
      const element = doc.createElement('div');
      element.dataset.anchorId = id;
      Object.assign(element.style, {position: 'absolute', boxSizing: 'border-box'});
      setRect(element, rect);
      world.append(element);
      const anchor = {id, rect, element};
      anchors.set(id, anchor);
      return snapshot(anchor);
    },
    get(id) {
      ensureOpen();
      const anchor = anchors.get(anchorId(id));
      return anchor ? snapshot(anchor) : null;
    },
    getElement(id) {
      ensureOpen();
      return anchors.get(anchorId(id))?.element ?? null;
    },
    list() {
      ensureOpen();
      return Array.from(anchors.values(), snapshot);
    },
    setRect(id, value) {
      ensureOpen();
      const anchor = anchors.get(anchorId(id));
      if (!anchor) throw new AxiomError('INVALID_ARGUMENT', 'Unknown anchor ID');
      const rect = copyRect(value);
      setRect(anchor.element, rect);
      anchor.rect = rect;
      return snapshot(anchor);
    },
    readTransform() {
      ensureOpen();
      return viewport ? [...viewport.transform] : null;
    },
    remove(id) {
      ensureOpen();
      const anchor = anchors.get(anchorId(id));
      if (!anchor) return false;
      anchors.delete(id);
      anchor.element.remove();
      return true;
    },
    dispose() {
      if (disposed) return;
      disposed = true;
      detach();
      anchors.clear();
      world.replaceChildren();
      clip.remove();
    },
  });
  setViewport(initialViewport);
  mount.append(clip);
  return {api, setViewport};
}
