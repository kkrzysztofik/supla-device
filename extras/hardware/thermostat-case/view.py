# /// script
# requires-python = ">=3.10,<3.14"
# dependencies = ["build123d>=0.9", "trimesh", "numpy"]
# ///
# Copyright (C) Krzysztof Krzysztofik
"""Interactive 3D viewer: base, lid, tabs and board envelopes -> out/viewer.html (one file, three.js from unpkg).

uv run view.py                     build once
uv run view.py --serve 8080        build, serve out/ on the LAN, rebuild when params.py/parts.py change
"""
import argparse
import base64
import functools
import http.server
import importlib
import os
import sys
import tempfile
import threading
import time

import trimesh
from build123d import export_stl

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "out")
WATCH = [os.path.join(HERE, f) for f in ("params.py", "parts.py")]


def build():
    """Build out/viewer.html: the meshes embedded as one GLB, three.js loaded from unpkg."""
    import params
    import parts
    importlib.reload(params)
    importlib.reload(parts)
    shapes = {"base": parts.base(), "lid": parts.lid(), **parts.fixings_placed(), "tab_clamp": parts.clamp_placed(), **parts.envelopes()}
    scene = trimesh.Scene()
    with tempfile.TemporaryDirectory() as tmp:
        for n, s in shapes.items():
            f = os.path.join(tmp, n + ".stl")
            export_stl(s, f, tolerance=0.05, angular_tolerance=0.3)
            scene.add_geometry(trimesh.load(f), node_name=n, geom_name=n)   # colours are set in the page by name
    glb = base64.b64encode(scene.export(file_type="glb")).decode()
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, "viewer.html"), "w") as f:
        f.write(PAGE.replace("__GLB__", glb))
    print(time.strftime("%H:%M:%S"), "wrote out/viewer.html,", len(shapes), "meshes", flush=True)


def main():
    """Build once; with --serve PORT also serve out/ and rebuild when the sources change."""
    ap = argparse.ArgumentParser()
    ap.add_argument("--serve", type=int, metavar="PORT", help="serve out/ on 0.0.0.0 and rebuild on change")
    a = ap.parse_args()
    sys.path.insert(0, HERE)
    build()
    if not a.serve:
        return
    handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=OUT)
    srv = http.server.ThreadingHTTPServer(("0.0.0.0", a.serve), handler)   # noqa: S104 (LAN-only, opt-in --serve)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    print(f"serving out/ on http://0.0.0.0:{a.serve}/viewer.html, reload the page after a rebuild", flush=True)
    seen = [os.path.getmtime(p) for p in WATCH]
    while True:
        time.sleep(1)
        now = [os.path.getmtime(p) for p in WATCH]
        if now != seen:
            seen = now
            try:
                build()
            except Exception as e:   # noqa: BLE001 (a half-saved params.py shouldn't kill the server)
                print("rebuild failed:", e, flush=True)


PAGE = """<!doctype html><html><head><meta charset="utf-8"><title>Thermostat case — 3D</title>
<style>body{margin:0;overflow:hidden;font:13px system-ui;background:#1d2026;color:#ddd}
#ui{position:absolute;top:10px;left:10px;background:#000a;padding:10px 14px;border-radius:8px;line-height:1.9}
#ui input[type=range]{width:160px;vertical-align:middle}.k{display:inline-block;width:10px;height:10px;margin:0 4px 0 8px;border-radius:2px}</style>
<script type="importmap">{"imports":{"three":"https://unpkg.com/three@0.160.0/build/three.module.js","three/addons/":"https://unpkg.com/three@0.160.0/examples/jsm/"}}</script>
</head><body><div id="ui"><b>Thermostat wall box</b><br>
<label><input type="checkbox" id="lid" checked> lid</label>
<label><input type="checkbox" id="base" checked> base</label>
<label><input type="checkbox" id="boards" checked> boards</label><br>
explode lid <input type="range" id="ex" min="0" max="60" value="35"><br>
lid opacity <input type="range" id="op" min="10" max="100" value="100"><br>
base opacity <input type="range" id="bop" min="10" max="100" value="100"><br>
<span class="k" style="background:#288c46"></span>PCBs<span class="k" style="background:#3c3c46"></span>XIAO<span class="k" style="background:#14288c"></span>relay<br>
<span class="k" style="background:#0a0a14"></span>OLED glass<span class="k" style="background:#f5c81e"></span>button cap<span class="k" style="background:#f0f0eb"></span>Grove plugs<span class="k" style="background:#e66e28"></span>tabs<br>
<small>drag = orbit · right-drag = pan · wheel = zoom · the front faces +Z</small></div>
<script type="module">
import * as THREE from 'three';
import {OrbitControls} from 'three/addons/controls/OrbitControls.js';
import {GLTFLoader} from 'three/addons/loaders/GLTFLoader.js';
const r=new THREE.WebGLRenderer({antialias:true});r.setSize(innerWidth,innerHeight);r.setPixelRatio(devicePixelRatio);document.body.appendChild(r.domElement);
const s=new THREE.Scene();s.background=new THREE.Color(0x1d2026);
const cam=new THREE.PerspectiveCamera(35,innerWidth/innerHeight,1,2000);
s.add(new THREE.HemisphereLight(0xffffff,0x444455,1.6));
const d=new THREE.DirectionalLight(0xffffff,1.8);d.position.set(80,120,160);s.add(d);
const d2=new THREE.DirectionalLight(0xffffff,0.6);d2.position.set(-100,-50,-80);s.add(d2);
const ctl=new OrbitControls(cam,r.domElement);
const bin=Uint8Array.from(atob("__GLB__"),c=>c.charCodeAt(0)).buffer;
new GLTFLoader().parse(bin,'',g=>{
  const root=g.scene;s.add(root);const ms=[];
  const C={base:0xc8c8cd,lid:0x78a0dc,shield:0x288c46,xiao:0x3c3c46,usb:0xbebebe,relay:0x2858c8,relay_body:0x14288c,oled:0x1e6e3c,oled_glass:0x0a0a14,button:0x288246,button_sw:0x333333,button_cap:0xf5c81e,tab:0xe66e28};
  root.traverse(o=>{if(o.isMesh){const n=(o.name||o.parent?.name||'');o.name=n;
    const col=C[n]??(/grove/.test(n)?0xf0f0eb:C[n.split('_')[0]]??0xcccccc);
    o.material=new THREE.MeshStandardMaterial({color:col,flatShading:true,roughness:.55,metalness:.05,transparent:true,side:THREE.DoubleSide});ms.push(o);}});
  const box=new THREE.Box3().setFromObject(root),c=box.getCenter(new THREE.Vector3());
  const sz=box.getSize(new THREE.Vector3()).length();ctl.target.copy(c);cam.position.set(c.x+sz*.9,c.y+sz*.6,c.z+sz*1.5);ctl.update();
  const nm=o=>(o.name||o.parent?.name||'');
  const lid=ms.filter(o=>nm(o).startsWith('lid')),base=ms.filter(o=>nm(o).startsWith('base'));
  const brd=ms.filter(o=>!lid.includes(o)&&!base.includes(o)),front=brd.filter(o=>/^(oled|button|washer)/.test(nm(o)));
  const $=id=>document.getElementById(id);
  const upd=()=>{const e=+$('ex').value;[...lid,...front].forEach(o=>o.position.z=e);
    lid.forEach(o=>{o.visible=$('lid').checked;o.material.opacity=$('op').value/100;o.material.depthWrite=$('op').value>99});
    base.forEach(o=>{o.visible=$('base').checked;o.material.opacity=$('bop').value/100;o.material.depthWrite=$('bop').value>99});
    brd.forEach(o=>o.visible=$('boards').checked);};
  ['lid','base','boards','ex','op','bop'].forEach(i=>$(i).oninput=upd);upd();
},e=>document.body.insertAdjacentHTML('beforeend','<pre style="color:#f66">'+e+'</pre>'));
addEventListener('resize',()=>{cam.aspect=innerWidth/innerHeight;cam.updateProjectionMatrix();r.setSize(innerWidth,innerHeight)});
(function a(){requestAnimationFrame(a);r.render(s,cam)})();
</script></body></html>
"""

if __name__ == "__main__":
    main()
