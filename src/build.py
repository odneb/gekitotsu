js="\n".join(open(f).read() for f in ['sim.js','render.js','io.js','net.js','main.js'])
js=js.replace("if (typeof module !== 'undefined') module.exports = SIM;","")
open('index.html','w').write(open('shell.html').read().replace('/*__JS__*/',js))
print(len(js))
