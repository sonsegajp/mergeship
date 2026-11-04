#!/usr/bin/env python3
"""Import Majora's Mask asset headers into Ship of Harkinian, under the mm/ namespace.

In both ports an asset symbol *is* its resource path:

    #define dgLinkGoronSkel "__OTR__objects/object_link_goron/gLinkGoronSkel"
    static const ALIGN_ASSET(2) char gLinkGoronSkel[] = dgLinkGoronSkel;

2 Ship 2 Harkinian and SoH use the identical convention, so MM's tables and action functions
compile against MM's symbols unchanged -- provided the paths point into the mm/ namespace that
our patched OTRExporter writes into mm.o2r. This rewrites them to do that.

With --verify and a built mm.o2r, every rewritten path is checked to actually exist in the
archive. That check is the point: a header and an archive that disagree do not fail loudly,
they resolve to Ocarina of Time's identically named asset instead.
"""
import argparse, os, re, sys, zipfile

NS = "mm"
# Majora's Mask and Ocarina of Time give 2195 assets the same C identifier -- gPlayerAnim_link_
# normal_climb_upL exists in both, pointing at different resources. Including both games' headers
# in one translation unit is therefore a redefinition error. Prefixing MM's symbols uniformly at
# import time removes the whole class of problem, and keeps which game an asset came from visible
# at every use site. MM code ported into SoH gets the same mechanical rename.
SYM_PREFIX = "mm_"

# MM and OoT headers for the same-named object share an include guard (OBJECTS_OBJECT_LINK_BOY_H
# in both), so including OoT's first turns MM's into a no-op and its symbols silently vanish.
GUARD = re.compile(r'^(#ifndef |#define |#endif // )([A-Z0-9_]+_H)', re.M)

DEF = re.compile(r'^#define d(\w+) "', re.M)
DECL = re.compile(r'^static const ALIGN_ASSET\((\d+)\) char (\w+)\[\] = d(\w+);', re.M)
# "__OTR__<path>" but not one already namespaced.
REF = re.compile(r'"__OTR__(?!' + NS + r'/)([^"]+)"')


def rewrite(text):
    text = REF.sub(lambda m: '"__OTR__%s/%s"' % (NS, m.group(1)), text)
    text = GUARD.sub(lambda m: '%sMM_%s' % (m.group(1), m.group(2)), text)
    text = DEF.sub(lambda m: '#define d%s%s "' % (SYM_PREFIX, m.group(1)), text)
    text = DECL.sub(
        lambda m: 'static const ALIGN_ASSET(%s) char %s%s[] = d%s%s;'
        % (m.group(1), SYM_PREFIX, m.group(2), SYM_PREFIX, m.group(3)), text)
    return text


def collect_paths(text):
    return re.findall(r'"__OTR__(' + NS + r'/[^"]+)"', text)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True, help="2ship2harkinian/mm/assets")
    ap.add_argument("--dst", required=True, help="Shipwright/soh/assets/mm")
    ap.add_argument("--verify", help="path to the built mm.o2r")
    ap.add_argument("--only", nargs="*", help="substrings; import only matching headers")
    a = ap.parse_args()

    archive = set(zipfile.ZipFile(a.verify).namelist()) if a.verify else None

    imported = refs = 0
    missing = []
    for root, _, files in os.walk(a.src):
        for fn in files:
            if not fn.endswith(".h"):
                continue
            src = os.path.join(root, fn)
            rel = os.path.relpath(src, a.src)
            if a.only and not any(s in rel.replace("\\", "/") for s in a.only):
                continue
            with open(src, "r", encoding="utf-8", errors="replace") as f:
                text = f.read()
            out = rewrite(text)
            paths = collect_paths(out)
            refs += len(paths)
            if archive is not None:
                for p in paths:
                    if p not in archive:
                        missing.append((rel, p))
            dst = os.path.join(a.dst, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            with open(dst, "w", encoding="utf-8", newline="\n") as f:
                f.write(out)
            imported += 1

    print("imported %d headers, %d asset references" % (imported, refs))
    if archive is not None:
        if missing:
            print("\n%d references are NOT in %s -- these would silently resolve to "
                  "Ocarina of Time's assets:" % (len(missing), os.path.basename(a.verify)))
            for rel, p in missing[:25]:
                print("   %-50s %s" % (rel, p))
            if len(missing) > 25:
                print("   ... and %d more" % (len(missing) - 25))
            return 1
        print("verified: every reference resolves inside %s" % os.path.basename(a.verify))
    return 0


if __name__ == "__main__":
    sys.exit(main())
