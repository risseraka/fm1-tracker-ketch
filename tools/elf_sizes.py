"""Rank a device build's symbols by size, per section: what RAM, the pool and the flash go to.
  python -I fw/tools/elf_sizes.py fw/build/dinghy.elf [top]      (top: symbols per section, default 25)
No toolchain needed (pure Python, ELF32 little-endian). Run it before and after a change (fw/OPTIMIZE.md)."""
import collections, struct, sys

path = sys.argv[1]
top = int(sys.argv[2]) if len(sys.argv) > 2 else 25
d = open(path, "rb").read()
assert d[:4] == b"\x7fELF" and d[4] == 1 and d[5] == 1, "ELF32 little-endian expected"
shoff = struct.unpack_from("<I", d, 0x20)[0]
shentsize, shnum, shstrndx = struct.unpack_from("<HHH", d, 0x2E)
sh = [struct.unpack_from("<IIIIIIIIII", d, shoff + i * shentsize) for i in range(shnum)]   # name type flags addr off size ...


def cstr(tab, off):
    start = tab[4] + off
    return d[start:d.index(b"\0", start)].decode()


names = [cstr(sh[shstrndx], s[0]) for s in sh]
print("sections (bytes):")
for n, s in zip(names, sh):
    if s[5] and n in (".text", ".ram_text", ".data", ".bss", ".pool", ".noinit"):
        print(f"  {n:10s} at {s[3]:#010x}  {s[5]:8d}")
symtab = next(s for s in sh if s[1] == 2)
strtab = sh[symtab[6]]
per = collections.defaultdict(list)
for i in range(symtab[5] // 16):
    st_name, _, st_size, _, _, st_shndx = struct.unpack_from("<IIIBBH", d, symtab[4] + i * 16)
    if st_size and st_shndx < shnum:
        per[names[st_shndx]].append((st_size, cstr(strtab, st_name)))
for sec in (".pool", ".bss", ".data", ".text"):
    rows = sorted(per.get(sec, []), reverse=True)
    print(f"\n{sec}: {len(rows)} sized symbols, {sum(r[0] for r in rows)} B")
    for size, name in rows[:top]:
        print(f"  {size:7d} {name}")
