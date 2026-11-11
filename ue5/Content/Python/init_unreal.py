# Runs when the editor opens this project. It builds the project's content from Content/Python/job.py, and
# re-runs that file whenever it changes, so the scene can be rebuilt without restarting the editor.
# Everything it does is logged to Saved/job.log.
import unreal, os, time, traceback
_P = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
_JOB = os.path.join(_P, 'Content', 'Python', 'job.py'); _LOG = os.path.join(_P, 'Saved', 'job.log')
_S = {'m': 0, 't0': time.time(), 'n': 0, 'later': []}
def _log(*a):
    try:
        os.makedirs(os.path.dirname(_LOG), exist_ok=True)
        with open(_LOG, 'a') as f: f.write(' '.join(str(x) for x in a) + '\n')
    except Exception: pass
def _later(sec, fn): _S['later'].append((time.time() + sec, fn))
def _tick(dt):
    _S['n'] += 1; now = time.time()
    for it in list(_S['later']):
        if now >= it[0]:
            _S['later'].remove(it)
            try: it[1]()
            except Exception: _log(traceback.format_exc())
    if _S['n'] % 20 or now - _S['t0'] < 12: return
    try: m = os.path.getmtime(_JOB)
    except OSError: return
    if m == _S['m']: return
    _S['m'] = m; _log('--- run', time.ctime())
    try:
        exec(compile(open(_JOB).read(), _JOB, 'exec'), {'__name__': '__job__', 'unreal': unreal, 'log': _log, 'later': _later, 'PROJECT': _P})
        _log('JOB OK')
    except Exception: _log(traceback.format_exc()); _log('JOB FAIL')
unreal.register_slate_post_tick_callback(_tick)
_log('=== editor started', time.ctime(), unreal.SystemLibrary.get_engine_version())
