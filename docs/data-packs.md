# Data packs

A data pack is a folder of JSON files and optional images that adds clubs, players, kits and badges to Nutmeg. The game ships with a fictional default set; community packs are layered on top of it.

This is format version **1**. The JSON Schema files in [`data/schema/`](../data/schema) are the authoritative definition; this page explains them. A complete working example lives in [`data/packs/sample_fictional/`](../data/packs/sample_fictional).

## Content policy

- The default set shipped with Nutmeg is fictional. Every name in it is invented.
- Community packs may contain real names (clubs, players, stadiums) and real badges. The pack author is responsible for any trademark, publicity or other rights in them. The Nutmeg project does not clear, endorse or host such content.
- A pack that uses real names must say so (`contains_real_names: true`) and must carry a `rights_statement` in its manifest. The same applies to real crests (`contains_real_badges: true`). The schema enforces this.
- Data and art in a pack must be licensed **CC0-1.0** or **CC-BY-SA-4.0**, declared in the manifest and covering every file in the pack. Do not include anything you cannot license that way, except where the rights statement says the author takes responsibility for real-world marks.
- Nutmeg code is GPL-3.0. Packs are data and are licensed separately, as above.
- Tools that list or install packs should show the real-names flag and the rights statement before a pack is enabled.

## Pack layout

```
my_pack/
  manifest.json     required
  clubs.json        optional
  players.json      optional
  kits.json         optional
  badges.json       optional
  assets/           optional, badge images (png or svg)
```

Every file except `manifest.json` is optional, so a pack can be only kits, or only a player update. Each file starts with an optional `"$schema"` key pointing at the matching schema, which gives editors validation and completion.

| File | Schema | Top-level key |
| --- | --- | --- |
| `manifest.json` | `manifest.schema.json` | (the object itself) |
| `clubs.json` | `clubs.schema.json` | `clubs` |
| `players.json` | `players.schema.json` | `players` |
| `kits.json` | `kits.schema.json` | `kits` |
| `badges.json` | `badges.schema.json` | `badges` |

## Manifest

| Field | Required | Notes |
| --- | --- | --- |
| `format` | yes | Always `1` for this version. |
| `id` | yes | Lowercase slug: `a-z`, `0-9`, `_`, `-`, up to 48 characters. Unique across installed packs. |
| `name` | yes | Display name. |
| `author` | yes | Person or group. |
| `licence` | yes | `CC0-1.0` or `CC-BY-SA-4.0`. |
| `version` | yes | Semantic version, `major.minor.patch`. |
| `contains_real_names` | yes | `true` if any name or likeness is real. |
| `contains_real_badges` | no | `true` if any badge reproduces a real crest or logo. |
| `rights_statement` | when either flag is true | Who takes responsibility for the real-world content. |
| `requires` | no | Ids of packs that must load first. |
| `description`, `homepage` | no | |

## Records

All records have an `id` slug, unique within its file inside the pack. Fields marked with a default may be omitted.

### Club

`id`, `name`, `short_name` (2 to 4 characters) are required. Optional: `city`, `stadium`, `division` (default 1), `formation` (`442`, `433`, `352`, `4231`, `532`, `343`; default `442`), `mentality` (`attacking`, `balanced`, `defensive`; default `balanced`), `style`, `keeping` and `set_pieces` (1 to 100), `rivals` (club refs), `badge` (badge ref), `kits` (`home`, `away`, `third`, `goalkeeper`, each a kit ref).

### Player

`id`, `name`, `position`, `age` and `attributes` are required. Optional: `club` (club ref; omit for a free agent), `number`, `potential`, `handling` (goalkeepers), `trait`.

- `position` is one of `GK`, `DF`, `MF`, `FW`.
- `attributes` holds integers from 1 to 100. The six core attributes `vision`, `passing`, `dribbling`, `finishing`, `tackling` and `pace` are required. Attributes added by later revisions of the format (see NUT-25) are optional and take engine defaults when absent, so a pack written today stays valid. Unknown keys are accepted if they are integer ratings.
- `trait` is one of `clinical`, `playmaker`, `pacey`, `enforcer`, `dribbler`.

### Kit

`id`, `pattern` (`plain`, `hoops`, `stripes`, `halves`, `sash`, `chevron`), and the colours `primary`, `secondary`, `shorts`, `socks` as `#rrggbb`. `name` is optional.

### Badge

`id` plus either an `image` (path under `assets/`, `.png` or `.svg`) or a generated placeholder (`shape` of `shield`, `round` or `square`, with `primary` and `secondary` colours). Set `real_world: true` on a badge that reproduces a real crest.

## References and layering

A reference is an id in the same pack (`harbour_badge`) or `pack_id/id` to point into a pack listed in `requires` (`sample_fictional/harbour_badge`). Cross-file references must resolve; a loader rejects a pack with a dangling reference.

Packs load in this order: the built-in fictional default set, then each enabled pack in the order the user arranges them, with every pack's `requires` loaded before it. Layering rules:

- A record whose `id` already exists in an earlier layer **replaces** that record whole. There is no field-level merge, so a replacement must be complete.
- A new `id` adds a record.
- Replacement is per type and per id: replacing a player does not touch the club it belonged to.
- There is no deletion. To hide default content, the user disables or reorders packs.
- Image paths resolve inside the pack that declares them. Paths with `..` segments or leading `/` are rejected.

Ids share one namespace per record type across all layers, so a later pack replaces an earlier record simply by reusing its id. Authors adding new content should pick distinctive ids to avoid replacing something by accident.

## Mapping to Pitch

The format maps onto the Pitch Laravel app (`Team` and `Player` models) so a pack can seed a Pitch world and Pitch can send lineups back to the runner.

| Pack field | Pitch column |
| --- | --- |
| club `name` | `teams.name` |
| club `style` | `teams.style` |
| club `division` | `teams.division` |
| club `formation`, `mentality` | `teams.formation`, `teams.mentality` (same ids as Pitch's `Formation` and `Mentality`) |
| club `keeping`, `set_pieces` | `teams.keeping`, `teams.set_pieces` |
| club `rivals` | `teams.is_derby` (set when the club has a rival in the same league) |
| player `name`, `position`, `age`, `potential` | `players.name`, `position` (`GK`/`DF`/`MF`/`FW`), `age`, `potential` |
| player `attributes.*` | `players.vision`, `passing`, `dribbling`, `finishing`, `tackling`, `pace` |
| player `handling` | `players.handling` |
| player `trait` | `players.trait` |

Pitch keeps career state on its own side and the pack does not carry it: `fitness`, `form`, injuries, cards, contracts, loans, `is_youth`, `training_focus`, and squad membership. Pitch derives a team's flat attribute profile from its players, so clubs in a pack carry no team-level `vision` and similar columns. Kits, badges, `number`, `city`, `stadium` and `short_name` are Nutmeg-side presentation and have no Pitch column yet.

## Versioning

- `format` changes only for breaking changes and loaders reject a `format` they do not know.
- Additive changes (new optional fields, new optional attributes, new enum values) keep `format` at 1 and add to the schema files; packs written against an older schema stay valid.
- The pack `version` is the author's own and follows semantic versioning.

## Validating a pack

Nutmeg's C++ core has no JSON Schema validator and no JSON parser dependency, so there is no Catch2 check for this; adding one would mean a new dependency. Validate with a standard JSON Schema (draft 2020-12) tool instead.

With Python:

```bash
pip install jsonschema
python -c "import json,sys,jsonschema as j; [print(n, [e.message for e in j.Draft202012Validator(json.load(open(f'data/schema/{n}.schema.json'))).iter_errors(json.load(open(f'{sys.argv[1]}/{n}.json')))] or 'ok') for n in ('manifest','clubs','players','kits','badges')]" data/packs/sample_fictional
```

With Node, one file at a time:

```bash
npx ajv-cli validate --spec=draft2020 -s data/schema/players.schema.json -d data/packs/sample_fictional/players.json
```

The schemas check shape and ranges. They cannot check that references resolve or that ids are unique within a file; the pack loader does that.

## Sample pack

`data/packs/sample_fictional` has two invented clubs, Harbour Rovers and Vale United, each with 18 players (two goalkeepers, six defenders, six midfielders, four forwards: an 11-player squad plus a full bench), kits and generated badges. It is CC0-1.0 and contains no real names.
