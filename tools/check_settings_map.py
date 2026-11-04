#!/usr/bin/env python3
"""Checks include/mergeship/SettingsMap.def, the settings SoH shares with 2Ship in mergeship.exe.

For every row: the SoH CVar is used somewhere in Shipwright's code and the 2Ship CVar somewhere in 2ship2harkinian's
(spelled literally, through a name macro such as CVAR_VSYNC_ENABLED / MS_CVAR_TERMINA_DOOR, a prefix macro such as
CVAR_ENHANCEMENT("X") / CVAR_PREFIX_ADVANCED_RESOLUTION ".X", or a Cosmetics Editor COSMETIC_OPTION("id")), and the
defaults the code passes to CVarGetInteger / CVarGetFloat agree with the table's.

    python tools/check_settings_map.py            # errors (a name no code uses) and default mismatches
    python tools/check_settings_map.py -v         # every row, with where each name was found and its defaults
    python tools/check_settings_map.py --strict   # default mismatches are errors too

Exit code: 0 when every name exists (and, with --strict, every default agrees), 1 otherwise.
"""
import argparse
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
TABLE = os.path.join(ROOT, 'include', 'mergeship', 'SettingsMap.def')
SHARED_INCLUDE = os.path.join(ROOT, 'include', 'mergeship')

ENGINES = {
    'soh': {
        'label': 'SoH',
        'roots': ['Shipwright/soh/soh', 'Shipwright/soh/src', 'Shipwright/soh/include'],
        # First definition wins (CMake: the game's own file is read before libultraship's, and caches are not reset).
        'cmake': ['Shipwright/CMake/soh-cvars.cmake', 'Shipwright/CMake/lus-cvars.cmake',
                  'Shipwright/libultraship/cmake/cvars.cmake'],
        'cosmetic': ('gCosmetics', ('Value', 'Changed', 'Rainbow', 'Locked')),
    },
    '2ship': {
        'label': '2Ship',
        'roots': ['2ship2harkinian/mm/2s2h', '2ship2harkinian/mm/src', '2ship2harkinian/mm/include'],
        'cmake': ['2ship2harkinian/CMake/2ship-cvars.cmake', '2ship2harkinian/CMake/lus-cvars.cmake',
                  '2ship2harkinian/libultraship/cmake/cvars.cmake'],
        'cosmetic': ('gCosmetic', ('Color', 'Changed', 'Rainbow', 'Locked')),
    },
}
SOURCE_EXT = ('.c', '.cpp', '.h', '.hpp', '.inc')
# Files that mention names without using them (old-name migrations, this table's consumers).
SKIP_FILES = {'ConfigUpdaters.cpp', 'MergeShipSettingsSync.cpp', 'SettingsSync.h', 'SettingsMap.def'}

ROW = re.compile(r'^\s*MS_SETTING\(\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*(\w+)\s*,\s*(\w+)\s*,\s*([^,]+?)\s*,\s*([^,]+?)\s*,'
                 r'\s*(\w+)\s*,\s*([\w|\s]+?)\s*\)\s*$')
CMAKE_SET = re.compile(r'set\(\s*(\w+)\s+"([^"]*)"')
STRING_DEFINE = re.compile(r'#\s*define\s+(\w+)\s+"([^"]*)"\s*$', re.M)
PREFIX_FUNC = re.compile(r'#\s*define\s+(\w+)\(\s*(\w+)\s*\)\s+(\w+)\s+"\."\s+\2\b')
COSMETIC = re.compile(r'COSMETIC_OPTION\(\s*"([^"]+)"')


def parse_table():
    rows = []
    with open(TABLE, encoding='utf-8') as f:
        for number, line in enumerate(f, 1):
            if not line.lstrip().startswith('MS_SETTING('):
                continue
            m = ROW.match(line)
            if not m:
                sys.exit(f'{TABLE}:{number}: cannot parse: {line.strip()}')
            soh, ship, soh_type, ship_type, soh_default, ship_default, xform, flags = m.groups()
            rows.append({
                'line': number, 'soh': soh, 'ship': ship, 'soh_type': soh_type, 'ship_type': ship_type,
                'soh_default': float(soh_default), 'ship_default': float(ship_default), 'xform': xform,
                'flags': {f.strip() for f in flags.split('|')},
            })
    return rows


def load_sources(roots):
    texts = []
    for root in roots:
        for dirpath, dirnames, filenames in os.walk(os.path.join(ROOT, root)):
            dirnames[:] = [d for d in dirnames if d not in ('assets', 'build', '.git')]
            for name in filenames:
                if name.endswith(SOURCE_EXT) and name not in SKIP_FILES:
                    path = os.path.join(dirpath, name)
                    try:
                        with open(path, encoding='utf-8', errors='replace') as f:
                            texts.append((os.path.relpath(path, ROOT), f.read()))
                    except OSError:
                        pass
    return texts


def cmake_macros(files):
    values = {}
    for rel in files:
        path = os.path.join(ROOT, rel)
        if not os.path.exists(path):
            continue
        with open(path, encoding='utf-8') as f:
            for name, value in CMAKE_SET.findall(f.read()):
                value = re.sub(r'\$\{(\w+)\}', lambda m: values.get(m.group(1), m.group(0)), value)
                values.setdefault(name, value)
    return values


class Engine:
    def __init__(self, key):
        spec = ENGINES[key]
        self.label = spec['label']
        self.texts = load_sources(spec['roots'])
        shared = load_sources([os.path.relpath(SHARED_INCLUDE, ROOT)])
        self.macros = cmake_macros(spec['cmake'])            # name -> string value
        for _, text in self.texts + shared:
            for name, value in STRING_DEFINE.findall(text):
                self.macros.setdefault(name, value)
        self.prefix_funcs = {}                               # CVAR_ENHANCEMENT -> "gEnhancements"
        for _, text in self.texts:
            for func, _, prefix in PREFIX_FUNC.findall(text):
                if prefix in self.macros:
                    self.prefix_funcs.setdefault(func, self.macros[prefix])
        self.cosmetic_prefix, self.cosmetic_fields = spec['cosmetic']
        self.cosmetic_ids = set()
        for _, text in self.texts:
            self.cosmetic_ids.update(COSMETIC.findall(text))

    def spellings(self, name):
        """(needle, regex) pairs matching a use of the CVar `name` in this codebase's source; the needle is a plain
        substring every match contains (a cheap filter before the regex)."""
        out = [(f'"{name}"', re.escape(f'"{name}"'))]
        for macro, value in self.macros.items():
            if value == name:
                out.append((macro, r'\b' + re.escape(macro) + r'\b(?!\s*")'))
            elif name.startswith(value + '.') and value:
                rest = name[len(value):]
                out.append((f'"{rest}"', r'\b' + re.escape(macro) + r'\s*' + re.escape(f'"{rest}"')))
        for func, prefix in self.prefix_funcs.items():
            if name.startswith(prefix + '.'):
                rest = name[len(prefix) + 1:]
                out.append((f'"{rest}"', r'\b' + re.escape(func) + r'\(\s*' + re.escape(f'"{rest}"') + r'\s*\)'))
        return out

    def cosmetic(self, name):
        parts = name.split('.')
        if len(parts) >= 3 and parts[0] == self.cosmetic_prefix and parts[-1] in self.cosmetic_fields:
            ident = '.'.join(parts[1:-1])
            return ident in self.cosmetic_ids
        return False

    def find(self, name):
        """(places, defaults): files that use `name`, and the default arguments of its CVarGet calls."""
        patterns = [(needle, re.compile(p)) for needle, p in self.spellings(name)]
        places, defaults = [], []
        for rel, text in self.texts:
            hit = False
            for needle, pattern in patterns:
                if needle not in text:
                    continue
                for m in pattern.finditer(text):
                    hit = True
                    default = call_default(text, m)
                    if default is not None:
                        defaults.append(default)
            if hit:
                places.append(rel)
        if not places and self.cosmetic(name):
            places.append('COSMETIC_OPTION')
        return places, defaults


def call_default(text, match):
    """The second argument of the CVarGetInteger/Float call whose first argument is `match`, if it is one."""
    before = text[max(0, match.start() - 40):match.start()]
    if not re.search(r'CVarGet(Integer|Float)\(\s*$', before):
        return None
    i = match.end()
    while i < len(text) and text[i] in ' \t\r\n':
        i += 1
    if i >= len(text) or text[i] != ',':
        return None
    depth, start = 0, i + 1
    for j in range(start, min(len(text), start + 200)):
        c = text[j]
        if c == '(':
            depth += 1
        elif c == ')':
            if depth == 0:
                return text[start:j].strip()
            depth -= 1
    return None


def numeric(expr, macros):
    expr = expr.strip()
    if expr in macros:
        expr = macros[expr]
    expr = {'true': '1', 'false': '0'}.get(expr, expr)
    try:
        return float(int(expr, 0))
    except ValueError:
        pass
    try:
        return float(expr.rstrip('fF'))
    except ValueError:
        return None


# Constants whose value the checker cannot see through the preprocessor (enumerators, button bits).
KNOWN = {'BTN_CUSTOM_MODIFIER1': '64', 'BTN_CUSTOM_MODIFIER2': '128', 'FILTER_THREE_POINT': '0',
         'MIRRORED_WORLD_OFF': '0', 'DAMAGE_VANILLA': '0', 'Colors::LightBlue': '5', 'LightBlue': '5'}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('--strict', action='store_true', help='default mismatches are errors')
    args = parser.parse_args()

    rows = parse_table()
    engines = {'soh': Engine('soh'), '2ship': Engine('2ship')}
    errors, warnings = [], []
    ship_names = {}
    for row in rows:
        if row['ship'] in ship_names:
            errors.append(f"line {row['line']}: 2Ship CVar {row['ship']} already mapped on line {ship_names[row['ship']]}")
        ship_names[row['ship']] = row['line']
        if 'MS_F_UNSET' in row['flags'] and (row['xform'] not in ('MS_X_SAME', 'MS_X_FORM_TUNIC', 'MS_X_FD_TUNIC') or
                                             row['soh_type'] != row['ship_type']):
            errors.append(f"line {row['line']}: MS_F_UNSET needs the same type and MS_X_SAME")
        if (row['soh_type'] == 'MS_T_COLOR') != (row['ship_type'] == 'MS_T_COLOR'):
            errors.append(f"line {row['line']}: a colour maps only to a colour")
        report = []
        for key, name, default, kind in (('soh', row['soh'], row['soh_default'], row['soh_type']),
                                         ('2ship', row['ship'], row['ship_default'], row['ship_type'])):
            engine = engines[key]
            places, defaults = engine.find(name)
            if not places:
                errors.append(f"line {row['line']}: {engine.label} CVar {name} is not used anywhere in {engine.label}")
                report.append(f'  {engine.label:5} {name}: NOT FOUND')
                continue
            seen = sorted(set(defaults))
            if kind != 'MS_T_COLOR':
                macros = dict(engine.macros)
                macros.update(KNOWN)
                values = {d: numeric(d, macros) for d in seen}
                wrong = [d for d, v in values.items() if v is not None and abs(v - default) > 1e-6]
                if wrong:
                    warnings.append(f"line {row['line']}: {engine.label} {name}: table default {default:g}, code uses "
                                    f"{', '.join(wrong)}")
            report.append(f"  {engine.label:5} {name}: {len(places)} file(s), e.g. {places[0]}; "
                          f"defaults {', '.join(seen) if seen else '-'}")
        if args.verbose:
            print(f"line {row['line']}: {row['xform']} {' | '.join(sorted(row['flags']))}")
            print('\n'.join(report))

    print(f'{len(rows)} mappings ({len({r["soh"] for r in rows})} SoH CVars -> {len(ship_names)} 2Ship CVars), '
          f'{len(errors)} error(s), {len(warnings)} default warning(s)')
    for e in errors:
        print('ERROR:', e)
    for w in warnings:
        print('warning:', w)
    failed = errors or (args.strict and warnings)
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
