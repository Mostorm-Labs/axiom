"""Independent predicates. No production imports, planner or store mutations.

Math uses exact rational intermediates rounded to binary64 after EACH elementary
operation. Canonical comparisons remain bit-exact; quality is a separate check.
"""
from fractions import Fraction
import math
import struct


def f64(value):
    value = float(value)
    if not math.isfinite(value):
        raise ValueError('NumericRange')
    return value


def elementary(a, b, op):
    a, b = Fraction.from_float(f64(a)), Fraction.from_float(f64(b))
    value = {'+':lambda:a+b, '-':lambda:a-b, '*':lambda:a*b, '/':lambda:a/b}[op]()
    return f64(value)


def add(a,b): return elementary(a,b,'+')
def sub(a,b): return elementary(a,b,'-')
def mul(a,b): return elementary(a,b,'*')
def div(a,b): return elementary(a,b,'/')
def canonical(x): return 0.0 if x == 0 else f64(x)
def bits(x): return struct.pack('>d', canonical(x)).hex()


def compose(p, q):
    a,b,c,d,x,y = p
    A,B,C,D,X,Y = q
    return [canonical(v) for v in [add(mul(a,A),mul(c,B)),add(mul(b,A),mul(d,B)),
        add(mul(a,C),mul(c,D)),add(mul(b,C),mul(d,D)),
        add(add(mul(a,X),mul(c,Y)),x),add(add(mul(b,X),mul(d,Y)),y)]]


def inverse(t):
    a,b,c,d,x,y = map(f64,t)
    det = sub(mul(a,d),mul(b,c))
    if det == 0:
        raise ValueError('UnsupportedPose: singular inverse')
    return [canonical(v) for v in [div(d,det),div(-b,det),div(-c,det),div(a,det),
        div(sub(mul(c,y),mul(d,x)),det),div(sub(mul(b,x),mul(a,y)),det)]]


def point(t,p):
    a,b,c,d,x,y = t
    return [canonical(add(add(mul(a,p[0]),mul(c,p[1])),x)),
            canonical(add(add(mul(b,p[0]),mul(d,p[1])),y))]


def quality(before, after):
    """Exact inequality over already-rounded world components, P12 section6.2."""
    a,b = Fraction.from_float(f64(before)), Fraction.from_float(f64(after))
    bound = Fraction(1,2**32) + Fraction(1,2**47)*max(Fraction(1),abs(a),abs(b))
    return abs(a-b) <= bound


def worlds(graph):
    out, active = {},set()
    def visit(key):
        if key in out: return out[key]
        if key in active: raise ValueError('Cycle')
        active.add(key)
        node = graph[key]
        parent = node.get('parent')
        if parent is not None and parent not in graph: raise ValueError('MissingTarget')
        local = node.get('local',[1,0,0,1,node.get('tx',0),0])
        out[key] = local if parent is None else compose(visit(parent),local)
        active.remove(key)
        return out[key]
    for key in graph: visit(key)
    return out


def valid_forest(graph):
    worlds(graph)
    sibling_keys = {}
    children = {}
    for key,node in graph.items():
        parent = node.get('parent')
        children.setdefault(parent,[]).append(key)
        if parent is not None:
            kind = graph[parent]['kind']
            if kind not in ('Group','Section','Sticky'): raise ValueError('InvalidApplicability')
            if kind == 'Sticky' and node['kind'] != 'RichText': raise ValueError('InvalidApplicability')
        if 'order' in node:
            pair = (parent,node['order'])
            if pair in sibling_keys: raise ValueError('OrderKeyCollision')
            sibling_keys[pair] = key
    for parent,keys in children.items():
        if parent is not None and graph[parent]['kind'] == 'Sticky' and len(keys)>1:
            raise ValueError('StickyCardinality')
    return True


def release_frontier(graph, removed):
    return sorted(k for k,v in graph.items() if k not in removed and v.get('parent') in removed)


def on_segment(p,a,b):
    return ((b[0]-a[0])*(p[1]-a[1]) == (b[1]-a[1])*(p[0]-a[0]) and
            min(a[0],b[0])<=p[0]<=max(a[0],b[0]) and min(a[1],b[1])<=p[1]<=max(a[1],b[1]))


def inside(p, polygon):
    result = False
    for a,b in zip(polygon,polygon[1:]+polygon[:1]):
        if on_segment(p,a,b): return True
        if (a[1]>p[1]) != (b[1]>p[1]):
            x = Fraction(b[0]-a[0])*Fraction(p[1]-a[1],b[1]-a[1])+a[0]
            if p[0] < x: result = not result
    return result


def rect_contained(rect, polygon):
    x,y,w,h = rect
    corners = [(x,y),(x+w,y),(x+w,y+h),(x,y+h)]
    if not all(inside(p,polygon) for p in corners): return False
    # A notch entering the rectangle disproves containment even if corners hit.
    for a,b in zip(polygon,polygon[1:]+polygon[:1]):
        for p in [a,b,((a[0]+b[0])/2,(a[1]+b[1])/2)]:
            if x<p[0]<x+w and y<p[1]<y+h: return False
    # Split rectangle edges at every polygon crossing and test each interval.
    for a,b in zip(corners,corners[1:]+corners[:1]):
        cuts = {Fraction(0),Fraction(1)}
        rx,ry = b[0]-a[0],b[1]-a[1]
        for c,d in zip(polygon,polygon[1:]+polygon[:1]):
            sx,sy = d[0]-c[0],d[1]-c[1]
            den = rx*sy-ry*sx
            if den:
                t = Fraction((c[0]-a[0])*sy-(c[1]-a[1])*sx,den)
                u = Fraction((c[0]-a[0])*ry-(c[1]-a[1])*rx,den)
                if 0<=t<=1 and 0<=u<=1: cuts.add(t)
        cuts = sorted(cuts)
        for lo,hi in zip(cuts,cuts[1:]):
            t = (lo+hi)/2
            if not inside((a[0]+rx*t,a[1]+ry*t),polygon): return False
    return True


def fail(index,reason):
    return {'first_failure':index,'reason':reason}


def c01(trace):
    completed = False
    for index,event in enumerate(trace):
        if event.get('unavailable'):
            if completed: return fail(index,'unavailable after declared coherent completion')
            continue
        if event.get('presented_x') != 20:
            return fail(index,'wrong intermediate presented position')
        if event.get('scene') in ('g+1','g+2'):
            if event.get('preview') != 'retired': return fail(index,'committed scene with old preview')
            completed = True
        elif event.get('scene') == 'g' and completed:
            return fail(index,'stale scene published after recovery')
    return None if completed else fail(len(trace),'positive coherent completion missing')


def c02(trace):
    current = None
    completed = False
    for index,event in enumerate(trace):
        if event.get('event') in ('wrap_committed','user_selection'):
            current = event['selected']
        elif event.get('selected') != current:
            return fail(index,'late callback overwrote current selection')
        completed |= event.get('event') == 'later_current_read'
    return None if completed and current == ['T'] else fail(len(trace),'positive current-selection completion missing')


def exact(expected, actual):
    """JSON with explicit f64 bit strings; never approximate canonical equality."""
    if type(expected) is not type(actual): return False
    if isinstance(expected,dict):
        return expected.keys() == actual.keys() and all(exact(v,actual[k]) for k,v in expected.items())
    if isinstance(expected,list):
        return len(expected) == len(actual) and all(exact(a,b) for a,b in zip(expected,actual))
    if isinstance(expected,float): return struct.pack('>d',expected) == struct.pack('>d',actual)
    return expected == actual


def capabilities(actual, expected):
    return actual == sorted(set(actual),key=lambda s:s.encode('ascii')) and actual == expected


def at(value,path):
    for part in path.split('.'):
        value = value[int(part)] if isinstance(value,list) else value[part]
    return value


def evaluate(predicates, observation):
    """Small executable assertion language over real observations, not an SUT."""
    for index,p in enumerate(predicates):
        op = p['op']
        try:
            if op == 'equal': ok = exact(p['expected'],at(observation,p['path']))
            elif op == 'unchanged': ok = exact(at(observation,p['before']),at(observation,p['after']))
            elif op == 'c01': ok = c01(at(observation,p['path'])) is None
            elif op == 'c02': ok = c02(at(observation,p['path'])) is None
            elif op == 'forest': ok = valid_forest(at(observation,p['path']))
            elif op == 'capabilities': ok = capabilities(at(observation,p['path']),p['expected'])
            elif op == 'quality': ok = quality(at(observation,p['before']),at(observation,p['after']))
            elif op == 'size': ok = len(bytes.fromhex(at(observation,p['path']))) <= p['limit']
            else: return fail(index,'unimplemented predicate '+op)
        except (KeyError,IndexError,TypeError,ValueError,ZeroDivisionError,OverflowError) as error:
            return fail(index,str(error))
        if not ok: return fail(index,'predicate mismatch: '+op)
    return None
