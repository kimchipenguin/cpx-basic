#!/usr/bin/env python3
"""readme.rtf from README.TXT: the same manual for a modern system.
Sans-serif; the top headline bold 18 pt, the other headlines bold 16 pt, the
headings inside "Controls" bold; no tables.

usage: rtf.py README.TXT readme.rtf"""
import sys

def esc(s): return s.replace('\\', '\\\\').replace('{', '\\{').replace('}', '\\}')

def blocks(lines):
    """-> (kind, text): h1, h2, sub, para, item, or pre (lines kept as they are)"""
    rule = lambda l: bool(l) and set(l) <= set('-=')
    out, i = [], 0
    while i < len(lines):
        l = lines[i]; nxt = lines[i + 1] if i + 1 < len(lines) else ''
        if not l or rule(l): i += 1
        elif rule(nxt): out.append(('h1' if nxt[0] == '=' else 'h2', l)); i += 2
        elif l.startswith('  '):
            j = i
            while j < len(lines) and (lines[j].startswith('  ') or (not lines[j] and lines[j + 1:j + 2] and lines[j + 1].startswith('  '))): j += 1
            out.append(('pre', lines[i:j])); i = j
        else:
            j = i + 1
            while j < len(lines) and lines[j] and not rule(lines[j + 1] if j + 1 < len(lines) else '') \
                    and not lines[j].startswith('- ') and (not lines[j].startswith('  ') or l.startswith('- ')): j += 1
            text = ' '.join(x.strip() for x in lines[i:j])
            single = j == i + 1 and not nxt and lines[i + 2:i + 3] and lines[i + 2].startswith('  ')
            out.append(('item', text[2:]) if l.startswith('- ') else ('sub', text) if single else ('para', text)); i = j
    return out

def rtf(lines):
    r = ['{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0\\fswiss Helvetica;}{\\f1\\fmodern Courier New;}}', '\\f0\\fs24']
    for kind, t in blocks(lines):
        if kind == 'h1': r.append('{\\pard\\sa240\\b\\fs36 %s\\par}' % esc(t))
        elif kind == 'h2': r.append('{\\pard\\sb360\\sa160\\b\\fs32 %s\\par}' % esc(t))
        elif kind == 'sub': r.append('{\\pard\\sb200\\sa80\\b %s\\par}' % esc(t))
        elif kind == 'para': r.append('{\\pard\\sa160 %s\\par}' % esc(t))
        elif kind == 'item': r.append('{\\pard\\li360\\fi-240\\sa80 \\bullet  %s\\par}' % esc(t))
        else:                                           # the lists of the controls: label, tab, what it does
            rows = []
            for l in t:
                s = l.strip()
                if not s: rows.append(''); continue
                k = l.find('  ', len(l) - len(l.lstrip()) + 1)
                if k < 0 or not l[k:].strip():
                    if l.startswith('     ') and rows and rows[-1]: rows[-1] += ' ' + s      # a continued line
                    else: rows.append(esc(s))
                else: rows.append(esc(l[:k].strip()) + '\\tab ' + esc(l[k:].strip()))
            for s in rows:
                r.append('{\\pard\\li360\\tx2200%s %s\\par}' % ('\\sa120' if not s else '', s) if s else '{\\pard\\fs12\\par}')
    return '\n'.join(r) + '\n}\n'

if __name__ == '__main__':
    lines = open(sys.argv[1], 'rb').read().decode('ascii').replace('\r\n', '\n').split('\n')
    open(sys.argv[2], 'w').write(rtf(lines))
