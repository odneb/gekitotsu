# Runs when the editor opens this project.
#   job.py   builds the project's content (materials, the arena). It runs when it has changed since it last ran.
#   test.py  drives the game for testing. It runs whenever it changes while the editor is open.
# Everything is logged to Saved/job.log.
import unreal, os, time, traceback
_P = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
_JOB = os.path.join(_P, 'Content', 'Python', 'job.py'); _TEST = os.path.join(_P, 'Content', 'Python', 'test.py')
_LOG = os.path.join(_P, 'Saved', 'job.log'); _STAMP = os.path.join(_P, 'Saved', 'job.stamp')
_S = {'t0': time.time(), 'n': 0, 'later': [], 'test': None, 'job': None}
def _mtime(p):
    try: return os.path.getmtime(p)
    except OSError: return 0
def _log(*a):
    try:
        os.makedirs(os.path.dirname(_LOG), exist_ok=True)
        with open(_LOG, 'a') as f: f.write(' '.join(str(x) for x in a) + '\n')
    except Exception: pass
def _later(sec, fn): _S['later'].append((time.time() + sec, fn))
def _run(path, name):
    _log('--- run', name, time.ctime())
    try:
        exec(compile(open(path).read(), path, 'exec'), {'__name__': '__job__', 'unreal': unreal, 'log': _log, 'later': _later, 'PROJECT': _P})
        _log(name, 'OK'); return True
    except Exception: _log(traceback.format_exc()); _log(name, 'FAIL'); return False
try: _S['job'] = float(open(_STAMP).read().strip())
except Exception: _S['job'] = 0.0
_S['test'] = _mtime(_TEST)
def _tick(dt):
    _S['n'] += 1; now = time.time()
    for it in list(_S['later']):
        if now >= it[0]:
            _S['later'].remove(it)
            try: it[1]()
            except Exception: _log(traceback.format_exc())
    if _S['n'] % 20 or now - _S['t0'] < 12: return
    m = _mtime(_JOB)
    if m and abs(m - _S['job']) > 0.5:
        _S['job'] = m
        if _run(_JOB, 'job'):
            try: open(_STAMP, 'w').write(repr(m))
            except Exception: pass
        return
    m = _mtime(_TEST)
    if m and m != _S['test']: _S['test'] = m; _run(_TEST, 'test')
unreal.register_slate_post_tick_callback(_tick)
_log('=== editor started', time.ctime(), unreal.SystemLibrary.get_engine_version())
