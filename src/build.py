import datetime
js="window.GK_BUILD='"+datetime.datetime.utcnow().strftime('%m%d-%H%M')+" UTC';\n"+"\n".join(open(f).read() for f in ['sim.js','mocap.js','render.js','io.js','net.js','main.js'])
js=js.replace("if (typeof module !== 'undefined') module.exports = SIM;","").replace("if (typeof module !== 'undefined') module.exports = MOCAP_RAW;","")
open('index.html','w').write(open('shell.html').read().replace('/*__JS__*/',js))
print(len(js))
