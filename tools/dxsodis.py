#!/usr/bin/env python3
"""Minimal D3D9 shader bytecode (DXSO) disassembler: declarations + instruction list.

usage: dxsodis.py file.bin [--decl-only]
"""
import struct
import sys

OPS = {0: "nop", 1: "mov", 2: "add", 3: "sub", 4: "mad", 5: "mul", 6: "rcp", 7: "rsq", 8: "dp3", 9: "dp4",
       10: "min", 11: "max", 12: "slt", 13: "sge", 14: "exp", 15: "log", 16: "lit", 17: "dst", 18: "lrp",
       19: "frc", 20: "m4x4", 21: "m4x3", 22: "m3x4", 23: "m3x3", 24: "m3x2", 25: "call", 26: "callnz",
       27: "loop", 28: "ret", 29: "endloop", 30: "label", 31: "dcl", 32: "pow", 33: "crs", 34: "sgn",
       35: "abs", 36: "nrm", 37: "sincos", 38: "rep", 39: "endrep", 40: "if", 41: "ifc", 42: "else",
       43: "endif", 44: "break", 45: "breakc", 46: "mova", 47: "defb", 48: "defi", 64: "texcoord",
       65: "texkill", 66: "tex", 80: "cnd", 81: "def", 88: "cmp", 90: "dp2add", 91: "dsx", 92: "dsy",
       93: "texldd", 95: "texldl"}
REG = {0: "r", 1: "v", 2: "c", 3: "a/t", 4: "oRast", 5: "oD", 6: "o/oT", 7: "i", 8: "oC", 9: "oDepth",
       10: "s", 14: "b", 15: "aL", 17: "vMisc", 19: "p"}
USAGE = {0: "position", 1: "blendweight", 2: "blendindices", 3: "normal", 4: "psize", 5: "texcoord",
         6: "tangent", 7: "binormal", 8: "tessfactor", 9: "positiont", 10: "color", 11: "fog", 12: "depth",
         13: "sample"}
NOSRC_DST = {38, 40, 41, 44, 45, 27, 25, 26, 30}  # flow control: sources only


def reg(tok):
    t = ((tok & 0x1800) >> 8) | ((tok & 0x70000000) >> 28)
    return f"{REG.get(t, f'type{t}')}{tok & 0x7ff}"


def main():
    data = open(sys.argv[1], "rb").read()
    decl_only = "--decl-only" in sys.argv
    toks = struct.unpack(f"<{len(data) // 4}I", data[: len(data) // 4 * 4])
    ver = toks[0]
    kind = "vs" if ver >> 16 == 0xFFFE else "ps"
    major, minor = (ver >> 8) & 0xFF, ver & 0xFF
    print(f"{kind}_{major}_{minor}")
    i = 1
    while i < len(toks):
        tok = toks[i]
        if tok == 0x0000FFFF:
            break
        op = tok & 0xFFFF
        if op == 0xFFFE:
            i += 1 + ((tok >> 16) & 0x7FFF)
            continue
        ln = (tok >> 24) & 0x0F if major >= 2 else None
        if ln is None:  # SM1: guess length by scanning to the next token with bit31 clear
            j = i + 1
            while j < len(toks) and toks[j] & 0x80000000:
                j += 1
            ln = j - i - 1
        params = toks[i + 1: i + 1 + ln]
        name = OPS.get(op, f"op{op}")
        ctl = (tok >> 16) & 0xFF
        if op == 31:
            d, r = params[0], params[1]
            if ((r & 0x1800) >> 8 | (r & 0x70000000) >> 28) == 10:
                print(f"  dcl_sampler type{(d >> 27) & 0xF} {reg(r)}")
            else:
                print(f"  dcl_{USAGE.get(d & 0x1F, d & 0x1F)}{(d >> 16) & 0xF} {reg(r)} mask={(r >> 16) & 0xF:x}")
        elif not decl_only:
            if op in NOSRC_DST:
                ops = [reg(p) for p in params]
            else:
                ops = [reg(params[0]) + f".{(params[0] >> 16) & 0xF:x}"] + [reg(p) for p in params[1:]] if params else []
            if op in (81, 48, 47):
                ops = [reg(params[0])] + [hex(p) for p in params[1:]]
            print(f"  {name}{'_c' + str(ctl) if op in (41, 45) else ''} " + ", ".join(ops))
        i += 1 + ln


if __name__ == "__main__":
    main()
