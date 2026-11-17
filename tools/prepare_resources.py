#!/usr/bin/env python3
# Подготовка ресурсов Genny для macOS-бандла.
#
# В Windows-версии картинки и пресеты вкомпилированы в DLL как ресурсы (.rc/.res).
# VSTGUI на macOS ищет картинки в бандле по имени bmp%05d.png, где число — ID ресурса.
# Поэтому скрипт разбирает resource.h и resource.rc и раскладывает файлы с нужными именами.

import argparse
import os
import re
import shutil
import sys

DEFINE_RE = re.compile(r"^#define\s+([A-Za-z_0-9]+)\s+([0-9]+)\s*$")
# NAME  TYPE  "путь\\к\\файлу"
ENTRY_RE = re.compile(r'^([A-Za-z_0-9]+)\s+([A-Z0-9_]+)\s+"([^"]+)"\s*$')


def read_text(path):
    with open(path, "rb") as f:
        raw = f.read()
    if raw[:2] == b"\xff\xfe" or raw[:2] == b"\xfe\xff":
        return raw.decode("utf-16")
    try:
        return raw.decode("utf-8")
    except UnicodeDecodeError:
        return raw.decode("cp1252", errors="replace")


def parse_defines(path):
    names = {}
    for line in read_text(path).splitlines():
        m = DEFINE_RE.match(line.strip().replace("\r", ""))
        if m:
            names[m.group(1)] = int(m.group(2))
    return names


def parse_rc(path):
    entries = []
    for line in read_text(path).splitlines():
        m = ENTRY_RE.match(line.strip().replace("\r", ""))
        if m:
            ident, kind, rel = m.group(1), m.group(2), m.group(3).replace("\\\\", "/").replace("\\", "/")
            entries.append((ident, kind, rel))
    return entries


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", required=True, help="каталог репозитория Genny (с resource.h и resource.rc)")
    ap.add_argument("--out", required=True, help="каталог, куда положить готовые ресурсы")
    args = ap.parse_args()

    defines = parse_defines(os.path.join(args.source, "resource.h"))
    entries = parse_rc(os.path.join(args.source, "resource.rc"))

    os.makedirs(args.out, exist_ok=True)

    by_id = {}
    copied = 0
    for ident, kind, rel in entries:
        if ident not in defines:
            print("пропуск (нет в resource.h): %s" % ident, file=sys.stderr)
            continue
        res_id = defines[ident]
        if res_id in by_id:
            print("конфликт ID %d: %s и %s" % (res_id, by_id[res_id], ident), file=sys.stderr)
            continue
        by_id[res_id] = (ident, kind, rel)

        src = os.path.join(args.source, rel)
        if not os.path.exists(src):
            print("нет файла: %s" % src, file=sys.stderr)
            continue

        ext = os.path.splitext(rel)[1].lower().lstrip(".")
        if kind == "PNG":
            # именно так VSTGUI/macOS ищет целочисленные ресурсы
            dst = os.path.join(args.out, "bmp%05d.png" % res_id)
        else:
            dst = os.path.join(args.out, "%05d.%s" % (res_id, ext))
        shutil.copyfile(src, dst)
        copied += 1

    # Служебный файл: карта «имя -> id», нужна загрузчику пресетов
    with open(os.path.join(args.out, "resource_ids.h"), "w", encoding="utf-8") as f:
        f.write("// Сгенерировано tools/prepare_resources.py — не редактировать вручную.\n")
        f.write("#pragma once\n")
        for name, value in sorted(defines.items(), key=lambda kv: kv[1]):
            f.write("#define %s %d\n" % (name, value))

    print("скопировано ресурсов: %d, известных ID: %d" % (copied, len(defines)))


if __name__ == "__main__":
    main()
