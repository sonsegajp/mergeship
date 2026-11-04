"""Compare an exported namespace to a stock O2R, including embedded paths and Gfx hashes.

This verifies serialized data, not rendering or game-specific scene/audio semantics.

--rebase checks the other direction, the one the games use at run time: the stock archive re-namespaced
exactly as include/mergeship/AssetArchive.h's AssetArchiveMode::Rebase does it as each resource loads
(2Ship mounts SoH's oot.o2r that way) must equal the namespaced export byte for byte.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import struct
import zipfile

MASK = (1 << 64) - 1
TABLE = []
for value in range(256):
    value <<= 56
    for _ in range(8):
        value = ((value << 1) ^ (0x42F0E1EBA9EA3693 if value >> 63 else 0)) & MASK
    TABLE.append(value)

def crc64(path):
    value = MASK
    for byte in path.encode():
        value = TABLE[(value >> 56) ^ byte] ^ ((value << 8) & MASK)
    return value

def normalize(data, prefix, hashes, counts):
    if len(data) < 64:
        return data
    endian = "<" if data[0] == 0 else ">"
    resource_type = struct.unpack_from(endian + "I", data, 4)[0]
    if resource_type == 0x4F444C54:
        data = bytearray(data)
        offset = 72
        while offset + 8 <= len(data):
            w0, w1 = struct.unpack_from(endian + "II", data, offset)
            opcode = w0 >> 24
            offset += 8
            if opcode in {0x20, 0x31, 0x32, 0x33, 0x35, 0x36, 0x42}:
                if offset + 8 > len(data):
                    raise ValueError("Truncated extended Gfx instruction")
                high, low = struct.unpack_from(endian + "II", data, offset)
                old_hash = hashes.get((high << 32) | low)
                if old_hash is not None:
                    struct.pack_into(endian + "II", data, offset, old_hash >> 32, old_hash & 0xFFFFFFFF)
                    counts["gfx_hashes"] += 1
                offset += 8
        return bytes(data)
    # Only normalize real length-prefixed strings. Leaf textures, vertices,
    # samples and animation data must remain byte-for-byte identical.
    edits = []
    pattern = rb"(?:__OTR__)?" + re.escape(prefix.encode()) + rb"/"
    for match in re.finditer(pattern, data):
        start = match.start()
        if start < 68:
            continue
        length = struct.unpack_from(endian + "I", data, start - 4)[0]
        if length < len(match.group()) or start + length > len(data):
            continue
        original = data[start:start + length]
        try:
            decoded = original.decode("ascii")
        except UnicodeDecodeError:
            continue
        if not re.fullmatch(r"[ -~]+", decoded):
            continue
        new = original.replace((prefix + "/").encode(), b"", 1)
        edits.append((start - 4, start + length, struct.pack(endian + "I", len(new)) + new))
    for start, end, replacement in reversed(edits):
        data = data[:start] + replacement + data[end:]
        counts["string_paths"] += 1
    return data

def verify(stock_path, foreign_path, namespace):
    counts = Counter()
    metadata = {"version", "portVersion", "manifest.json"}
    failures = []
    with zipfile.ZipFile(stock_path) as stock, zipfile.ZipFile(foreign_path) as foreign:
        native_names = set(stock.namelist()) - metadata
        foreign_names = set(foreign.namelist()) - metadata
        expected = {namespace + "/" + name for name in native_names}
        missing = sorted(expected - foreign_names)
        extra = sorted(foreign_names - expected)
        hashes = {crc64(namespace + "/" + name): crc64(name) for name in native_names}
        for name in sorted(native_names):
            path = namespace + "/" + name
            if path not in foreign_names:
                continue
            baseline = stock.read(name)
            normalized = normalize(foreign.read(path), namespace, hashes, counts)
            if normalized != baseline:
                failures.append(name)
            counts["resources"] += 1
    return {"stock": str(stock_path), "foreign": str(foreign_path), "namespace": namespace,
            "counts": dict(counts), "missing": missing, "extra": extra,
            "different_after_normalization": failures,
            "passed": not (missing or extra or failures)}

GFX_HASH_OPCODES = {0x20, 0x31, 0x32, 0x33, 0x35, 0x36, 0x42}
LEAF_TYPES = {0x4F544558, 0x4F415252, 0x4F534D50, 0x4F565458}  # OTEX, OARR, OSMP, OVTX
PRINTABLE_RUN = re.compile(rb"[ -~]{3,}")

def rebase(data, prefix, visible, hashes):
    """AssetArchive::Rebase, line for line: `visible` maps namespaced path -> stock path."""
    if len(data) < 64 or data[:1] == b"<":
        return data
    endian = "<" if data[0] == 0 else ">"
    resource_type = struct.unpack_from(endian + "I", data, 4)[0]
    if resource_type == 0x4F444C54:
        data = bytearray(data)
        offset = 72
        while offset + 8 <= len(data):
            w0 = struct.unpack_from(endian + "I", data, offset)[0]
            offset += 8
            if (w0 >> 24) in GFX_HASH_OPCODES:
                if offset + 8 > len(data):
                    break
                high, low = struct.unpack_from(endian + "II", data, offset)
                new_hash = hashes.get((high << 32) | low)
                if new_hash is not None:
                    struct.pack_into(endian + "II", data, offset, new_hash >> 32, new_hash & 0xFFFFFFFF)
                offset += 8
        return bytes(data)
    if resource_type in LEAF_TYPES:
        return data
    edits = []
    for run in PRINTABLE_RUN.finditer(data):
        for start in range(run.start(), min(run.start() + 4, run.end())):
            if start < 68:
                continue
            length = struct.unpack_from(endian + "I", data, start - 4)[0]
            if length < 3 or start + length > run.end():
                continue
            text = data[start:start + length].decode("ascii")
            marked = text.startswith("__OTR__")
            path = text[7:] if marked else text
            if visible.get(prefix + path) == path:
                new = ("__OTR__" if marked else "") + prefix + path
                edits.append((start - 4, start + length, struct.pack(endian + "I", len(new)) + new.encode()))
                break
    for start, end, replacement in reversed(edits):
        data = data[:start] + replacement + data[end:]
    return data

def verify_rebase(stock_path, foreign_path, namespace):
    metadata = {"version", "portVersion", "manifest.json"}
    prefix = namespace + "/"
    different = []
    counts = Counter()
    with zipfile.ZipFile(stock_path) as stock, zipfile.ZipFile(foreign_path) as foreign:
        names = [name for name in stock.namelist() if name not in metadata]
        visible = {prefix + name: name for name in names}
        hashes = {crc64(name): crc64(prefix + name) for name in names}
        foreign_names = set(foreign.namelist()) - metadata
        missing = sorted(set(visible) - foreign_names)
        extra = sorted(foreign_names - set(visible))
        for name in names:
            if prefix + name not in foreign_names:
                continue
            original = stock.read(name)
            rebased = rebase(original, prefix, visible, hashes)
            counts["rewritten" if rebased != original else "unchanged"] += 1
            if rebased != foreign.read(prefix + name):
                different.append(name)
            counts["resources"] += 1
    return {"stock": str(stock_path), "foreign": str(foreign_path), "namespace": namespace, "mode": "rebase",
            "counts": dict(counts), "missing": missing, "extra": extra,
            "different_after_rebase": different, "passed": not (missing or extra or different)}

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stock", type=Path)
    parser.add_argument("foreign", type=Path)
    parser.add_argument("--namespace", default="oot")
    parser.add_argument("--report", type=Path)
    parser.add_argument("--rebase", action="store_true",
                        help="check AssetArchive's run-time re-namespacing of the stock archive instead")
    args = parser.parse_args()
    result = (verify_rebase if args.rebase else verify)(args.stock, args.foreign, args.namespace)
    output = json.dumps(result, indent=2)
    if args.report:
        args.report.write_text(output + "\n", encoding="utf-8")
    print(output)
    raise SystemExit(0 if result["passed"] else 1)
