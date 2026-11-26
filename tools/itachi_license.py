#!/usr/bin/env python3
"""ITACHI license tool - (c) @yooitachiijod.  Pure python 3, no dependencies.

  keygen                      create itachi_private.key + itachi_public.key  (KEEP THE PRIVATE KEY SECRET, BACK IT UP)
  sign  --name @you --device duchamp [--serial SERIAL] [--expires YYYY-MM-DD] [--priv itachi_private.key] [-o license.lic]
  seal  [--license license.lic] [--pub itachi_public.key]    writes hal/itachi_sealed.h  (then build the HAL)
  verify [--license license.lic] [--pub itachi_public.key]

A sealed HAL only works with the exact license.lic it was sealed with (its signature derives the key that unmasks the
HAL's service names / interface hashes). Ed25519 is deterministic: same license text -> same signature -> same key."""
import argparse, hashlib, os, sys, time, datetime

# ---------------- Ed25519 (RFC 8032 reference algorithm) ----------------
p = 2**255 - 19
q = 2**252 + 27742317777372353535851937790883648493
def sha512(s): return hashlib.sha512(s).digest()
def modp_inv(x): return pow(x, p - 2, p)
d = -121665 * modp_inv(121666) % p
def point_add(P, Q):
    A, B = (P[1]-P[0]) * (Q[1]-Q[0]) % p, (P[1]+P[0]) * (Q[1]+Q[0]) % p
    C, D = 2 * P[3] * Q[3] * d % p, 2 * P[2] * Q[2] % p
    E, F, G, H = B-A, D-C, D+C, B+A
    return (E*F % p, G*H % p, F*G % p, E*H % p)
def point_mul(s, P):
    Q = (0, 1, 1, 0)
    while s > 0:
        if s & 1: Q = point_add(Q, P)
        P = point_add(P, P); s >>= 1
    return Q
def point_equal(P, Q):
    return (P[0]*Q[2]-Q[0]*P[2]) % p == 0 and (P[1]*Q[2]-Q[1]*P[2]) % p == 0
modp_sqrt_m1 = pow(2, (p-1)//4, p)
def recover_x(y, sign):
    if y >= p: return None
    x2 = (y*y-1) * modp_inv(d*y*y+1)
    if x2 == 0: return None if sign else 0
    x = pow(x2, (p+3)//8, p)
    if (x*x - x2) % p != 0: x = x * modp_sqrt_m1 % p
    if (x*x - x2) % p != 0: return None
    if (x & 1) != sign: x = p - x
    return x
g_y = 4 * modp_inv(5) % p
g_x = recover_x(g_y, 0)
G = (g_x, g_y, 1, g_x * g_y % p)
def point_compress(P):
    zinv = modp_inv(P[2]); x, y = P[0]*zinv % p, P[1]*zinv % p
    return int.to_bytes(y | ((x & 1) << 255), 32, "little")
def point_decompress(s):
    y = int.from_bytes(s, "little"); sign = y >> 255; y &= (1 << 255) - 1
    x = recover_x(y, sign)
    return None if x is None else (x, y, 1, x*y % p)
def secret_expand(secret):
    h = sha512(secret); a = int.from_bytes(h[:32], "little")
    a &= (1 << 254) - 8; a |= (1 << 254)
    return a, h[32:]
def secret_to_public(secret):
    a, _ = secret_expand(secret); return point_compress(point_mul(a, G))
def sha512_modq(s): return int.from_bytes(sha512(s), "little") % q
def ed_sign(secret, msg):
    a, prefix = secret_expand(secret); A = point_compress(point_mul(a, G))
    r = sha512_modq(prefix + msg); R = point_compress(point_mul(r, G))
    h = sha512_modq(R + A + msg); s = (r + h*a) % q
    return R + int.to_bytes(s, 32, "little")
def ed_verify(public, msg, sig):
    if len(public) != 32 or len(sig) != 64: return False
    A = point_decompress(public); R = point_decompress(sig[:32])
    if A is None or R is None: return False
    s = int.from_bytes(sig[32:], "little")
    if s >= q: return False
    h = sha512_modq(sig[:32] + public + msg)
    return point_equal(point_mul(s, G), point_add(R, point_mul(h, A)))

# ---------------- ITACHI license ----------------
DOMAIN = b"itachi-v1"
def derive_key(sig): return sha512(sig + DOMAIN)
def mask(sig, tag, data):
    key = derive_key(sig); out = bytearray(); b = 0
    while len(out) < len(data):
        out += sha512(key + bytes([tag, b])); b += 1
    return bytes(x ^ y for x, y in zip(data, out))
# sealed blobs: tag -> plaintext (same order as C header)
BLOBS = [
    (1, b"vendor.oplus.hardware.touch.IOplusTouch"),
    (2, b"vendor.oplus.hardware.displaypanelfeature.IDisplayPanelFeature"),
    (3, b"ef41c5fab372bb5ad6a417f606845c87cf0e9b17"),
    (4, b"39afca22ac77253421ad681eeb2bae0c6ff62c13"),
]
def serial_id(serial): return hashlib.sha512(serial.encode()).hexdigest()[:16]

def read_license(path):
    raw = open(path, "rb").read()
    i = raw.rfind(b"sig=")
    if i < 0: sys.exit("license has no sig= line")
    body, sigline = raw[:i], raw[i:].decode().strip()
    sig = bytes.fromhex(sigline[4:])
    return body, sig
def hexs(b): return ", ".join("0x%02x" % x for x in b)

def main():
    ap = argparse.ArgumentParser(); sp = ap.add_subparsers(dest="cmd", required=True)
    sp.add_parser("keygen")
    s = sp.add_parser("sign"); s.add_argument("--name", required=True); s.add_argument("--device", default="duchamp")
    s.add_argument("--serial"); s.add_argument("--expires"); s.add_argument("--priv", default="itachi_private.key"); s.add_argument("-o", default="license.lic")
    for n in ("seal", "verify"):
        t = sp.add_parser(n); t.add_argument("--license", default="license.lic"); t.add_argument("--pub", default="itachi_public.key")
        if n == "seal": t.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "hal", "itachi_sealed.h"))
    a = ap.parse_args()
    if a.cmd == "keygen":
        if os.path.exists("itachi_private.key"): sys.exit("itachi_private.key already exists - refusing to overwrite")
        seed = os.urandom(32)
        fd = os.open("itachi_private.key", os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600); os.write(fd, seed.hex().encode()); os.close(fd)
        open("itachi_public.key", "w").write(secret_to_public(seed).hex())
        print("created itachi_private.key (SECRET, back it up offline) and itachi_public.key"); return
    if a.cmd == "sign":
        seed = bytes.fromhex(open(a.priv).read().strip())
        exp = 0
        if a.expires: exp = int(datetime.datetime.strptime(a.expires, "%Y-%m-%d").replace(tzinfo=datetime.timezone.utc).timestamp())
        body = f"name={a.name}\ndevice={a.device}\nid={serial_id(a.serial) if a.serial else '*'}\nexpires={exp}\n"
        sig = ed_sign(seed, body.encode()); open(a.o, "w").write(body + "sig=" + sig.hex() + "\n")
        print("wrote", a.o); print(body, end=""); return
    pub = bytes.fromhex(open(a.pub).read().strip()); body, sig = read_license(a.license)
    ok = ed_verify(pub, body, sig)
    if a.cmd == "verify": print("VALID" if ok else "INVALID"); sys.exit(0 if ok else 1)
    if not ok: sys.exit("license does not verify against that public key")
    out = ["/* generated by tools/itachi_license.py seal - do not edit. (c) @yooitachiijod */",
           "#ifndef ITACHI_SEALED_H\n#define ITACHI_SEALED_H",
           "static const unsigned char ITACHI_PUBKEY[32] = { %s };" % hexs(pub)]
    names = ["TOUCH_DESC", "DPF_DESC", "TOUCH_HASH", "DPF_HASH"]
    for (tag, plain), nm in zip(BLOBS, names):
        m = mask(sig, tag, plain)
        out.append("static const unsigned char SEALED_%s[%d] = { %s };" % (nm, len(m), hexs(m)))
        out.append("#define SEALED_%s_TAG %d" % (nm, tag))
    out.append("#endif\n"); open(a.out, "w").write("\n".join(out)); print("wrote", os.path.normpath(a.out))
main()
