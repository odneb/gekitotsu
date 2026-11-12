# Drives the game for testing: starts it in the editor, sends it commands, takes screenshots, stops it.
import unreal, traceback, time
log('recompile', time.ctime()); unreal.SystemLibrary.execute_console_command(None, 'Module Recompile Gekitotsu'); log('recompiled', time.ctime())
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

def match():
    try:
        w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        a = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.GkMatch) if w else []
        return a[0] if len(a) else None
    except Exception: log(traceback.format_exc()); return None

def cmd(c):
    m = match(); r = m.debug(c) if m else 'NO MATCH'; log('  >', c, '=>', r); return r

def shot(name, w=1600, h=900):
    unreal.AutomationLibrary.take_high_res_screenshot(w, h, 't_%s.png' % name); log('  shot', name)

def play(steps, total=None):
    """steps: (seconds since the previous step, 'command; command' or ('shot', name))"""
    try: LES.editor_request_begin_play(); log('play started')
    except Exception: log(traceback.format_exc()); return
    t = 3.0
    for d, a in steps:
        t += d
        if isinstance(a, tuple): later(t, lambda a=a: shot(a[1]))
        else: later(t, lambda a=a: [cmd(c.strip()) for c in a.split(';')])
    def stop():
        try: LES.editor_request_end_play(); log('play stopped')
        except Exception: log(traceback.format_exc())
    later(t + (total or 2.5), stop)

steps = [(1.0, 'cpu 0 0; cpu 1 0; set idle 9999; state'), (2.2, 'place 0 -105 0; place 1 105 0; set camx 400; set camy 0; set camzfix 108'), (1.5, ('shot', 'close')),
         (1.6, 'hud 0; pause 1; set sheetgap 150; set camx 560; sheet Idle S5L:6 S5M:9 S5H:13 S2H:12'), (1.6, ('shot', 'sheetA')),
         (1.6, 'sheet; unset camx; hud 1; pause 0; cpu 0 1; cpu 1 1; set idle 0; reset')]
for k in range(5): steps += [(2.6, ('shot', 'fight%d' % k)), (0.2, 'state')]
play(steps)
