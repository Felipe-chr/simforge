#!/usr/bin/env python3
"""Valida os contratos do SimForge: schemas, exemplos e links relativos.

Uso:
    python tools/validate_contracts.py

Requisitos: Python 3.9+ e `jsonschema` (`pip install jsonschema`).
Sai com código 1 em qualquer falha, para uso em CI.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

try:
    from jsonschema import Draft202012Validator, FormatChecker
except ImportError:  # pragma: no cover
    print("ERRO: instale jsonschema (pip install jsonschema)")
    sys.exit(2)

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"
EXAMPLES = ROOT / "examples"

SCHEMAS = {
    "sforge": DOCS / "formatos" / "sforge-v1.schema.json",
    "sfpkg": DOCS / "formatos" / "sfpkg-v1.schema.json",
    "api": DOCS / "protocolos" / "api-v1.schema.json",
    "plugin": DOCS / "protocolos" / "plugin-v1.schema.json",
    "world": DOCS / "protocolos" / "world-v1.schema.json",
}

failures: list[str] = []


def fail(msg: str) -> None:
    failures.append(msg)
    print(f"  FALHA  {msg}")


def ok(msg: str) -> None:
    print(f"  ok     {msg}")


def json_blocks(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8")
    return re.findall(r"```json\n(.*?)```", text, re.S)


def load_schemas() -> dict[str, dict]:
    schemas: dict[str, dict] = {}
    for name, path in SCHEMAS.items():
        schema = json.loads(path.read_text(encoding="utf-8"))
        Draft202012Validator.check_schema(schema)
        schemas[name] = schema
        ok(f"schema {name} é JSON Schema 2020-12 válido")
    return schemas


def validate(label: str, schema: dict, instances: list[object]) -> None:
    validator = Draft202012Validator(schema, format_checker=FormatChecker())
    for index, instance in enumerate(instances):
        errors = sorted(validator.iter_errors(instance), key=lambda e: list(e.path))
        if errors:
            first = errors[0]
            where = "/".join(str(p) for p in first.path) or "<raiz>"
            fail(f"{label}[{index}] em /{where}: {first.message[:200]}")
        else:
            ok(f"{label}[{index}]")


def check_examples(schemas: dict[str, dict]) -> None:
    # Documento .sforge completo em 03-plataforma.md
    full = [
        json.loads(b)
        for b in json_blocks(DOCS / "03-plataforma.md")
        if '"format": "simforge.project"' in b
    ]
    validate("sforge.exemplo", schemas["sforge"], full)

    # Arquivos .sforge distribuídos em examples/
    shipped = sorted(EXAMPLES.glob("*.sforge")) if EXAMPLES.is_dir() else []
    if not shipped:
        fail("examples/: nenhum arquivo .sforge encontrado")
    for path in shipped:
        try:
            document = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            fail(f"examples/{path.name}: JSON inválido ({exc})")
            continue
        validate(f"sforge.{path.name}", schemas["sforge"], [document])

    # Manifestos de plugin (dois documentos)
    manifests = [
        json.loads(b)
        for b in json_blocks(DOCS / "protocolos" / "plugin-abi-v1.md")
        if '"format": "simforge.plugin"' in b
    ]
    manifests += [
        json.loads(b)
        for b in json_blocks(DOCS / "03-plataforma.md")
        if '"format": "simforge.plugin"' in b
    ]
    validate("plugin.manifesto", schemas["plugin"], manifests)

    # Manifesto de pacote
    package = [
        json.loads(b)
        for b in json_blocks(DOCS / "formatos" / "sfpkg-v1.md")
        if '"format": "simforge.package"' in b
    ]
    validate("sfpkg.manifesto", schemas["sfpkg"], package)

    # Envelopes da API (request/response/error/event)
    api_blocks = [json.loads(b) for b in json_blocks(DOCS / "protocolos" / "api-v1.md")]
    envelopes = [
        o
        for o in api_blocks
        if isinstance(o, dict)
        and (
            {"id", "method"} <= o.keys()
            or {"id", "ok"} <= o.keys()
            or {"seq", "t_ns", "type", "payload"} <= o.keys()
        )
    ]
    validate("api.envelope", schemas["api"], envelopes)

    # Mensagens world.*
    world_blocks = [json.loads(b) for b in json_blocks(DOCS / "protocolos" / "world-v1.md")]
    messages = [
        o
        for o in world_blocks
        if isinstance(o, dict) and str(o.get("type", "")).startswith("world.")
    ]
    validate("world.mensagem", schemas["world"], messages)


def check_all_json_blocks_parse() -> None:
    count = 0
    for path in markdown_files(DOCS):
        for index, block in enumerate(json_blocks(path)):
            count += 1
            try:
                json.loads(block)
            except json.JSONDecodeError as exc:
                fail(f"{path.name} bloco JSON #{index}: {exc}")
    ok(f"{count} blocos JSON parseiam")


LINK_RE = re.compile(r"\]\(([^)]+)\)")
EXTERNAL = ("http://", "https://", "mailto:", "#")
# Diretórios gerados ou de terceiros: não fazem parte dos contratos do projeto.
SKIP_DIRS = {"node_modules", "dist", "build", ".vite", ".git", "__pycache__"}


def markdown_files(root: Path) -> list[Path]:
    return sorted(
        path
        for path in root.rglob("*.md")
        if not SKIP_DIRS.intersection(path.relative_to(root).parts)
    )


def check_links() -> None:
    count = 0
    for path in markdown_files(ROOT):
        for match in LINK_RE.finditer(path.read_text(encoding="utf-8")):
            target = match.group(1)
            if target.startswith(EXTERNAL):
                continue
            relative = target.split("#", 1)[0]
            if not relative.strip():
                continue
            count += 1
            if not (path.parent / relative).exists():
                fail(f"{path.name}: link quebrado -> {target}")
    ok(f"{count} links relativos resolvem")


def main() -> int:
    print("SimForge — validação de contratos")
    schemas = load_schemas()
    print("\nExemplos contra schemas")
    check_examples(schemas)
    print("\nBlocos JSON")
    check_all_json_blocks_parse()
    print("\nLinks relativos")
    check_links()

    print()
    if failures:
        print(f"{len(failures)} falha(s)")
        return 1
    print("Todos os contratos válidos.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
