// SPDX-License-Identifier: GPL-3.0-only
// SMK-37 port: Copyright (C) 2026 M. Geisler (genkigenki/makkuro)
// The web installer's SMK-37 rules (web/install/fm1pkg.js, fm1ota.js) in node: the interleave count only when the
// files' CRCs check out with it, exactly the package's model on install and resume (nothing written without the
// package's identity), the MIDI port filter. A fake WebMIDI device answers the handshake.
//   node tests/smk_web_test.mjs [build/ketch-smk37.fwsc]
import { readFileSync } from "node:fs";
import { logicalImage, blockCount, productOf } from "../web/install/fm1pkg.js";
import { Updater, sameModel, pack7 } from "../web/install/fm1ota.js";

let bad = 0;
const check = (what, ok) => { console.log(`${what.padEnd(72)} ${ok ? "ok" : "FAIL"}`); bad += !ok; };
const BLK = 0x30, KEEP = 0x2F;

function interleave(img, product, blocks) {
  const out = new Uint8Array(img.length + blocks);
  for (let i = 0; i < blocks; i++) {
    out.set(img.subarray(i * KEEP, (i + 1) * KEEP), i * BLK);
    out[i * BLK + KEEP] = i < product.length ? (product.charCodeAt(i) + i + 1) & 0xFF : 0x7D;
  }
  out.set(img.subarray(blocks * KEEP), blocks * BLK);
  return out;
}

function fakeAccess(name, identity) {                // one port pair that answers the handshake with identity
  const sent = [];
  const input = { name, state: "connected", onmidimessage: null, open: async () => {} };
  const output = {
    name, state: "connected", open: async () => {},
    send(bytes) {
      sent.push(bytes);
      if (bytes[0] === 0xF0 && bytes[1] === 0x00 && bytes[2] === 0x32) {   // HS_QUERY
        const t = [...identity].map((c) => c.charCodeAt(0));
        const body = [0, 0x59, 0x11, 0, 0, 0, ...t, ...new Array(28 - t.length).fill(0)];
        setTimeout(() => input.onmidimessage && input.onmidimessage({ data: [0xF0, ...pack7(body), 0xF7] }), 1);
      }
    },
  };
  return { access: { inputs: new Map([[1, input]]), outputs: new Map([[1, output]]) }, sent };
}

const path = process.argv[2] || "build/ketch-smk37.fwsc";
const pkg = new Uint8Array(readFileSync(path));
const img = logicalImage(pkg);
check(`${path.split("/").pop()}: ${productOf(pkg)}, 20 blocks`, blockCount(pkg) === 20 && img.length === pkg.length - 20);
const p36 = interleave(img, "SMK-37 Pro_016", 36);
check("36 interleave blocks (the stock SMK v016 layout): detected, the files check out",
  blockCount(p36) === 36 && Buffer.from(logicalImage(p36)).equals(Buffer.from(img)));
const chance = interleave(img, productOf(pkg), 20);
chance[20 * BLK + KEEP] = 0x7D;                      // 0x7D after block 20 by chance
const want20 = new Uint8Array(chance.length - 20);
for (let i = 0; i < 20; i++) want20.set(chance.subarray(i * BLK, i * BLK + KEEP), i * KEEP);
want20.set(chance.subarray(20 * BLK), 20 * KEEP);
check("0x7D after block 20 by chance: the files fail with the detected count, 20 taken",
  blockCount(chance) > 20 && Buffer.from(logicalImage(chance)).equals(Buffer.from(want20)));

check("sameModel: exactly the model", sameModel("SMK-37 Pro", "ota-SMK-37 Pro") && !sameModel("SMK-37 Pro", "SMK-37 Elite") &&
  !sameModel("FM-1", "SMK-37 Pro") && !sameModel("MKE-P37", "SMK-37 Pro"));

for (const [name, probed] of [["SMK-37 Pro", true], ["MKE-P37", true], ["SMK-37 Elite", true], ["STARRYKEY-37 PLAY", true],
  ["Felucca", true], ["FM-1", true], ["Drum Machine", false], ["Audio Interface", false], ["Synth Module", false]]) {
  const { access, sent } = fakeAccess(name, "SMK-37 Pro_950");
  const dev = await new Updater(access).find();
  check(`MIDI port ${JSON.stringify(name)}: ${probed ? "probed" : "never sent anything"}`, !!dev === probed && (sent.length > 0) === probed);
  if (dev) dev.link.close();
}

async function resume(loader, product) {
  const { access, sent } = fakeAccess("Felucca Update", loader);
  try { await new Updater(access).resume(img, null, product); return { code: "", sent }; } catch (e) { return { code: e.code, sent }; }
}
for (const [loader, product, what] of [["ota-SMK-37 Pro_900", "FM-1_951", "an FM-1 package to an SMK-37 loader"],
  ["ota-SMK-37 Pro_900", "SMK-37 Elite_951", "an Elite package to a Pro loader"], ["ota-FM-1_900", "SMK-37 Pro_951", "an SMK package to an FM-1 loader"],
  ["ota-SMK-37 Pro_900", "", "no package identity"]]) {
  const r = await resume(loader, product);
  check(`resume: ${what} -> refused, only the handshake sent`, r.code === "model" && r.sent.length === 1);
}
console.log(bad ? "SMK WEB TEST FAILED" : "smk web test passed");
process.exit(bad ? 1 : 0);
