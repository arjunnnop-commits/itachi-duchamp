#!/usr/bin/env python3
"""ITACHI: turn SELinux denials into CIL allow rules.  (c) @yooitachiijod
Usage: logcat -d | grep 'avc:' | python3 tools/avc2cil.py [domain]   (default domain: itachi_hal)"""
import re, sys, collections
dom = sys.argv[1] if len(sys.argv) > 1 else "itachi_hal"
rx = re.compile(r"avc:\s+denied\s+\{([^}]*)\}.*?scontext=u:r:(\w+):s0.*?tcontext=u:(?:object_r|r):(\w+):s0\s+tclass=(\w+)")
rules = collections.defaultdict(set)
for line in sys.stdin:
    m = rx.search(line)
    if m and m.group(2) == dom:
        for p in m.group(1).split():
            rules[(m.group(3), m.group(4))].add(p)
for (t, c), perms in sorted(rules.items()):
    print(f"(allow {dom} {t} ({c} ({' '.join(sorted(perms))})))")
