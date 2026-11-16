"""Load attributed, content-bound whole-group cheat libraries for TeoPCE."""
import hashlib
import json
from pathlib import Path
import re
from urllib.parse import urlsplit

import build_cheat_groups as codec
import library_media as media

MAX_DATABASE = 16 * 1024 * 1024


class Database:
    def __init__(self, paths, preferred=None):
        self.entries = {}
        self.sources = []
        for path in paths:
            raw = media.read_file(path, MAX_DATABASE)
            document = json.loads(raw)
            codec.require(type(document) is dict and document.get("version") == 1,
                          "Expected cheat library version 1")
            entries = document.get("entries")
            codec.require(type(entries) is list and len(entries) <= 1200,
                          "Cheat library needs an entries list of at most 1200 games")
            for item in entries:
                self.add(item)
            provenance = {"file": Path(path).name, "sha256": hashlib.sha256(raw).hexdigest(),
                          "entries": len(entries)}
            for field in ("data_license", "attribution"):
                if field in document:
                    provenance[field] = document[field]
            self.sources.append(provenance)
        if preferred is not None:
            custom = Database([preferred])
            self.entries.update(custom.entries)
            self.sources.extend(custom.sources)

    def add(self, item):
        fields = {"title", "kind", "path", "content_identity", "size", "crc32",
                  "requires_path", "sources", "groups"}
        codec.require(type(item) is dict and set(item) == fields, "Cheat library entry schema")
        codec.require(type(item["kind"]) is int and item["kind"] in (1, 2), "Unsupported cheat media")
        codec.require(type(item["title"]) is str and 0 < len(item["title"]) <= 256, "Cheat library title")
        media.safe_relative(item["path"])
        codec.require(type(item["requires_path"]) is bool, "Cheat path policy must be boolean")
        codec.require(type(item["content_identity"]) is str and
                      re.fullmatch(r"[a-f0-9]{64}", item["content_identity"]), "Cheat game SHA-256 identity")
        sources = item["sources"]
        codec.require(type(sources) is list and 1 <= len(sources) <= 32, "Cheat source attribution")
        codec.require(all(type(s) is str and len(s) <= 2048 and urlsplit(s).scheme == "https"
                          and urlsplit(s).netloc for s in sources), "Cheat sources must be HTTPS links")
        codec.require(type(item["groups"]) is list, "Cheat groups must be a list")
        groups = []
        for group in item["groups"]:
            codec.require(type(group) is dict and set(group) == {"id", "name", "codes", "reason"}, "Cheat group schema")
            codec.require(type(group["codes"]) is list and all(type(c) is list for c in group["codes"]), "Cheat code list")
            groups.append({**group, "codes": [tuple(code) for code in group["codes"]]})
        # The production encoder owns code-domain, count and identity validation.
        key = 0
        encoded = codec.encode_package(key, item["size"], item["crc32"], groups)
        codec.decode_package(encoded, key, item["size"], item["crc32"])
        entry = {**item, "groups": groups}
        identity = item["kind"], item["content_identity"]
        prior = self.entries.get(identity)
        codec.require(prior is None or prior == entry, "Conflicting cheat libraries for the same game content")
        self.entries[identity] = entry

    def package(self, row, game):
        entry = self.entries.get((game["kind"], game["content_identity"]))
        if entry is None:
            return None, "No exact-content cheat match", []
        fp = game["fingerprint"]
        codec.require((fp["size"], fp["crc32"]) == (entry["size"], entry["crc32"]),
                      "Cheat library launch identity mismatch")
        if entry["requires_path"] and game["path"] != entry["path"]:
            return None, "CD startup policy requires path " + entry["path"], entry["sources"]
        data = codec.encode_package(row["key"], fp["size"], fp["crc32"], entry["groups"])
        return data, "Matched bundled/imported cheat library by full content", entry["sources"]
