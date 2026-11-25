#!/usr/bin/env python3
"""Print the license string for itachid. Usage: make_license.py <secret-hex> > itachi.lic"""
import hmac, hashlib, sys
NAME, DEVICE = "joditachi", "duchamp"   # keep in sync with src/auth.h
secret = bytes.fromhex(sys.argv[1]) if len(sys.argv) > 1 else bytes.fromhex("6974616368692d6465762d6b65792d6368616e67652d6d65")
print(hmac.new(secret, f"{NAME}|{DEVICE}".encode(), hashlib.sha256).hexdigest())
