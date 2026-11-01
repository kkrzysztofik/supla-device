# /// script
# requires-python = ">=3.10,<3.14"
# dependencies = ["build123d>=0.9", "trimesh", "manifold3d", "numpy"]
# ///
# Copyright (C) Krzysztof Krzysztofik
"""Interactive 3D viewer: mount, HeatSync rack, original collar, Dremel envelope -> out/viewer.html
(one file, three.js from unpkg). Needs A1_HeatSync.3mf here for the rack and collar.

uv run view.py                     build once
uv run view.py --serve 8080        build, serve out/ on the LAN, rebuild when params.py/parts.py change
"""
import argparse
import base64
import contextlib
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
PYC = os.path.join(HERE, "__pycache__")
WATCH = [os.path.join(HERE, f) for f in ("params.py", "parts.py")]
WATCH_OPTIONAL = [os.path.join(HERE, "A1_HeatSync.3mf")]   # rack/collar mesh: downloading it rebuilds
sys.dont_write_bytecode = True   # params/parts are re-read every build: never cache them as .pyc


def drop_bytecode():
    """Remove params/parts bytecode; a same-second, same-size edit defeats .pyc freshness checks."""
    try:
        names = os.listdir(PYC)
    except FileNotFoundError:
        return
    for n in names:
        if n.startswith(("params.", "parts.")) and n.endswith(".pyc"):
            with contextlib.suppress(FileNotFoundError):   # a concurrent build removed it first
                os.unlink(os.path.join(PYC, n))


def mtimes():
    """Snapshot of what build() reads; None marks an input that is absent right now.

    Returns None overall while params.py/parts.py are mid-save (some editors unlink then
    rewrite), so the loop waits instead of rebuilding against a half-saved source.
    """
    now = []
    for p in WATCH:
        try:
            now.append(os.path.getmtime(p))
        except OSError:   # a transiently missing file must not take the server down
            return None
    for p in WATCH_OPTIONAL:
        try:
            now.append(os.path.getmtime(p))
        except OSError:   # absent, or being replaced right now: same thing for a rebuild
            now.append(None)
    return now


def build():
    drop_bytecode()
    import params
    import parts
    importlib.reload(params)
    importlib.reload(parts)
    scene = trimesh.Scene()
    with tempfile.TemporaryDirectory() as tmp:   # STLs are only a hand-off to trimesh
        for n, s in {"mount": parts.mount(), "dremel": parts.dremel_envelope()}.items():
            f = os.path.join(tmp, n + ".stl")
            export_stl(s, f, tolerance=0.05, angular_tolerance=0.3)
            scene.add_geometry(trimesh.load(f), node_name=n, geom_name=n)   # colours are set in the page by name
    if os.path.exists(parts.HEATSYNC_3MF):
        scene.add_geometry(parts.heatsync_mesh("object_74"), node_name="rack", geom_name="rack")
        scene.add_geometry(parts.heatsync_mesh("object_77"), node_name="collar", geom_name="collar")
    glb = base64.b64encode(scene.export(file_type="glb")).decode()
    os.makedirs(OUT, exist_ok=True)
    page = os.path.join(OUT, "viewer.html")
    fd, tmp = tempfile.mkstemp(dir=OUT, suffix=".html")   # unique: a concurrent build must not collide
    try:
        with os.fdopen(fd, "w") as f:   # a reader must never see a half-written page
            f.write(PAGE.replace("__GLB__", glb))
        os.replace(tmp, page)
    finally:
        if os.path.exists(tmp):
            os.unlink(tmp)
    print(time.strftime("%H:%M:%S"), "wrote out/viewer.html,", len(scene.geometry), "meshes", flush=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--serve", type=int, metavar="PORT", help="serve out/ on 0.0.0.0 and rebuild on change")
    a = ap.parse_args()
    sys.path.insert(0, HERE)
    seen = mtimes()   # before the first build: a save that lands during it is still picked up
    build()
    if not a.serve:
        return
    handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=OUT)
    srv = http.server.ThreadingHTTPServer(("0.0.0.0", a.serve), handler)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    print(f"serving out/ on http://0.0.0.0:{a.serve}/viewer.html, reload the page after a rebuild", flush=True)
    failed, retry_at = None, 0.0
    while True:
        time.sleep(1)
        now = mtimes()
        if now is None or now == seen:   # nothing new, or a save is mid-flight
            continue
        if now == failed and time.monotonic() < retry_at:
            continue                     # same broken files: back off instead of spinning
        try:
            build()
        except Exception as e:   # a half-saved params.py shouldn't kill the server
            print("rebuild failed:", e, flush=True)
            failed, retry_at = now, time.monotonic() + 5   # retry in case the failure was transient
        else:
            seen, failed = now, None


PAGE = """<!doctype html><html><head><meta charset="utf-8"><title>Dremel mount — 3D</title>
<style>body{margin:0;overflow:hidden;font:13px system-ui;background:#1d2026;color:#ddd}
#ui{position:absolute;top:10px;left:10px;background:#000a;padding:10px 14px;border-radius:8px;line-height:1.9}
#ui input[type=range]{width:160px;vertical-align:middle}.k{display:inline-block;width:10px;height:10px;margin:0 4px 0 8px;border-radius:2px}</style>
<script type="importmap">{"imports":{"three":"https://unpkg.com/three@0.160.0/build/three.module.js","three/addons/":"https://unpkg.com/three@0.160.0/examples/jsm/"}}</script>
</head><body><div id="ui"><b>Dremel 3000 mount on HeatSync</b><br>
<label><input type="checkbox" id="mount" checked><span class="k" style="background:#2872e6"></span>mount</label>
<label><input type="checkbox" id="dremel" checked><span class="k" style="background:#f08c1e"></span>Dremel</label><br>
<label><input type="checkbox" id="rack" checked><span class="k" style="background:#8c8c96"></span>rack</label>
<label><input type="checkbox" id="collar"><span class="k" style="background:#3cc8e6"></span>original iron collar</label><br>
mount opacity <input type="range" id="op" min="10" max="100" value="100"><br>
Dremel opacity <input type="range" id="dop" min="0" max="100" value="35"><br>
<small>drag = orbit · right-drag = pan · wheel = zoom · Z up, the Dremel hangs at -Y</small></div>
<script type="module">
import * as THREE from 'three';
import {OrbitControls} from 'three/addons/controls/OrbitControls.js';
import {GLTFLoader} from 'three/addons/loaders/GLTFLoader.js';
THREE.Object3D.DEFAULT_UP.set(0,0,1);
const r=new THREE.WebGLRenderer({antialias:true});r.setSize(innerWidth,innerHeight);r.setPixelRatio(devicePixelRatio);document.body.appendChild(r.domElement);
const s=new THREE.Scene();s.background=new THREE.Color(0x1d2026);
const cam=new THREE.PerspectiveCamera(35,innerWidth/innerHeight,1,3000);
s.add(new THREE.HemisphereLight(0xffffff,0x444455,1.6));
const d=new THREE.DirectionalLight(0xffffff,1.8);d.position.set(-120,-160,200);s.add(d);
const d2=new THREE.DirectionalLight(0xffffff,0.6);d2.position.set(100,80,-80);s.add(d2);
const ctl=new OrbitControls(cam,r.domElement);
const bin=Uint8Array.from(atob("__GLB__"),c=>c.charCodeAt(0)).buffer;
new GLTFLoader().parse(bin,'',g=>{
  const root=g.scene;root.rotation.set(0,0,0);root.updateMatrixWorld();s.add(root);
  const C={mount:0x2872e6,dremel:0xf08c1e,rack:0x8c8c96,collar:0x3cc8e6},G={};
  root.traverse(o=>{if(o.isMesh){const n=(o.name||o.parent?.name||'');
    o.material=new THREE.MeshStandardMaterial({color:C[n]??0xcccccc,flatShading:true,roughness:.55,metalness:.05,transparent:true,side:THREE.DoubleSide});(G[n]=G[n]||[]).push(o);}});
  const box=new THREE.Box3().setFromObject(root),c=box.getCenter(new THREE.Vector3());
  const sz=box.getSize(new THREE.Vector3()).length();ctl.target.copy(c);cam.position.set(c.x-sz*.9,c.y-sz*1.3,c.z+sz*.5);ctl.update();
  const $=id=>document.getElementById(id);
  const op=(n,v)=>(G[n]||[]).forEach(o=>{o.material.opacity=v;o.material.depthWrite=v>.99});
  const upd=()=>{['mount','dremel','rack','collar'].forEach(n=>(G[n]||[]).forEach(o=>o.visible=$(n).checked));
    op('mount',$('op').value/100);op('dremel',$('dop').value/100);op('collar',.6);};
  ['mount','dremel','rack','collar','op','dop'].forEach(i=>$(i).oninput=upd);upd();
},e=>document.body.insertAdjacentHTML('beforeend','<pre style="color:#f66">'+e+'</pre>'));
addEventListener('resize',()=>{cam.aspect=innerWidth/innerHeight;cam.updateProjectionMatrix();r.setSize(innerWidth,innerHeight)});
(function a(){requestAnimationFrame(a);r.render(s,cam)})();
</script></body></html>
"""

if __name__ == "__main__":
    main()
