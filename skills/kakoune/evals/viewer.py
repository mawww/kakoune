#!/usr/bin/env python3
"""Render a Kakoune benchmark run into a self-contained HTML report.

Reads a benchmark run's `results.json` (a JSON array of
`[idx, kind, result, detail, block]` tuples) and, optionally, the `evals.json`
that produced it (to attach each task's instruction and ground truth). Emits one
standalone HTML file you can open directly in a browser.

Usage:
    python3 viewer.py results.json [evals.json] [-o report.html]

Exit codes:
    0  report written
    1  bad input (missing file, malformed JSON)
"""

import argparse
import json
import sys
from datetime import datetime
from html import escape


STATUS_STYLE = {
    "PASS": "pass",
    "FAIL": "fail",
    "TIMEOUT": "timeout",
    "ERROR": "error",
}


def load_results(path):
    """Load results.json and confirm it is a JSON array of result rows.

    Raises ValueError if the file is valid JSON but not a list, so callers get
    one clear message rather than an odd IndexError or dict lookup downstream.
    """
    with open(path, encoding="utf-8") as fh:
        data = json.load(fh)
    if not isinstance(data, list):
        raise ValueError("results.json must be a JSON array")
    return data


def load_evals(path):
    """Return {idx: {"instruction": ..., "ground_truth": ..., "note": ...}}."""
    with open(path, encoding="utf-8") as fh:
        entries = json.load(fh)
    return {idx: entry for idx, entry in enumerate(entries)}


def normalize_row(row):
    """Coerce a results.json tuple into a dict, tolerating short rows."""
    idx, kind, result = row[0], row[1], row[2]
    detail = row[3] if len(row) > 3 else ""
    block = row[4] if len(row) > 4 else ""
    return {
        "idx": idx,
        "kind": kind,
        "result": result,
        "detail": detail,
        "block": block,
    }


def score(rows):
    total = len(rows)
    passed = sum(1 for r in rows if r["result"] == "PASS")
    failed = sum(1 for r in rows if r["result"] in ("FAIL", "ERROR"))
    timed_out = sum(1 for r in rows if r["result"] == "TIMEOUT")
    # Guard the percentage against divide-by-zero on an empty run.
    pct = round(100 * passed / total, 1) if total else 0.0
    return {
        "total": total,
        "passed": passed,
        "failed": failed,
        "timed_out": timed_out,
        "score": pct,
    }


def render(rows, evals):
    """Build the standalone HTML report from normalized rows.

    `evals` maps idx -> {instruction, ground_truth, note}; anything missing
    from it (or lacking an instruction) renders as empty/muted rather than
    erroring, so a results.json produced without a paired evals file still works.
    """
    s = score(rows)
    cards = [
        ("Tasks", s["total"], ""),
        ("Passed", s["passed"], "pass"),
        ("Failed", s["failed"], "fail"),
        ("Timed out", s["timed_out"], "timeout"),
        ("Score", f"{s['score']}%", ""),
    ]

    card_html = "".join(
        f'<div class="card {cls}"><div class="card-num">{val}</div>'
        f'<div class="card-label">{label}</div></div>'
        for label, val, cls in cards
    )

    table_rows = []
    for r in rows:
        kind = r["kind"]
        status = r["result"]
        # Unknown result values fall back to a neutral style instead of crashing.
        cls = STATUS_STYLE.get(status, "unknown")
        inst = evals.get(r["idx"], {}).get("instruction", "")
        gt = evals.get(r["idx"], {}).get("ground_truth", "")
        note = evals.get(r["idx"], {}).get("note", "")

        detail_html = (
            f'<pre class="detail">{escape(r["detail"])}</pre>'
            if r["detail"].strip()
            else '<span class="muted">—</span>'
        )

        block_html = ""
        if r["block"].strip():
            block_html = (
                '<details class="block"><summary>model output '
                f'({len(r["block"])} chars)</summary>'
                f'<pre class="code">{escape(r["block"])}</pre></details>'
            )

        gt_html = ""
        if gt.strip():
            gt_html = (
                '<details class="gt"><summary>ground truth</summary>'
                f'<pre class="code">{escape(gt)}</pre>'
                + (f'<p class="note">{escape(note)}</p>' if note else "")
                + "</details>"
            )

        table_rows.append(
            f"""<tr class="row-{cls}">
            <td class="idx">{r['idx']}</td>
            <td class="status"><span class="badge {cls}">{status}</span>
              <span class="kind">{escape(kind)}</span></td>
            <td class="task">{escape(inst) or '<span class="muted">(no instruction)</span>'}</td>
            <td class="detail-cell">{detail_html}</td>
            <td class="cell-actions">{gt_html}{block_html}</td>
          </tr>"""
        )

    return f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Kakoune benchmark report</title>
<style>
  :root {{
    --bg: #0f1115; --panel: #171a21; --panel-2: #1e222b; --border: #2a2f3a;
    --fg: #e6e9ee; --muted: #8b93a1; --accent: #6ea8fe;
    --pass: #3fb950; --fail: #f85149; --timeout: #d29922; --error: #f85149;
  }}
  * {{ box-sizing: border-box; }}
  body {{
    margin: 0; background: var(--bg); color: var(--fg);
    font: 14px/1.5 -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
  }}
  header {{
    padding: 24px 28px 20px; border-bottom: 1px solid var(--border);
    background: linear-gradient(180deg, #14171d, #0f1115);
  }}
  header h1 {{ margin: 0 0 4px; font-size: 20px; }}
  header .meta {{ color: var(--muted); font-size: 12px; }}
  .cards {{
    display: flex; gap: 12px; padding: 20px 28px; flex-wrap: wrap;
  }}
  .card {{
    background: var(--panel); border: 1px solid var(--border);
    border-radius: 10px; padding: 14px 18px; min-width: 110px;
  }}
  .card.pass .card-num {{ color: var(--pass); }}
  .card.fail .card-num {{ color: var(--fail); }}
  .card.timeout .card-num {{ color: var(--timeout); }}
  .card-num {{ font-size: 26px; font-weight: 650; }}
  .card-label {{ color: var(--muted); font-size: 12px; text-transform: uppercase; letter-spacing: .04em; }}
  .wrap {{ padding: 0 28px 40px; overflow-x: auto; }}
  table {{ width: 100%; border-collapse: collapse; margin-top: 4px; }}
  th, td {{ text-align: left; padding: 10px 12px; border-bottom: 1px solid var(--border); vertical-align: top; }}
  th {{ color: var(--muted); font-size: 11px; text-transform: uppercase; letter-spacing: .05em; position: sticky; top: 0; background: var(--bg); }}
  td.idx {{ color: var(--muted); font-variant-numeric: tabular-nums; }}
  td.status {{ white-space: nowrap; }}
  td.task {{ max-width: 420px; }}
  td.detail-cell {{ max-width: 360px; }}
  td.cell-actions {{ white-space: nowrap; }}
  .badge {{
    display: inline-block; padding: 2px 8px; border-radius: 999px;
    font-size: 11px; font-weight: 600; text-transform: uppercase; letter-spacing: .03em;
  }}
  .badge.pass {{ background: rgba(63,185,80,.15); color: var(--pass); border: 1px solid rgba(63,185,80,.4); }}
  .badge.fail {{ background: rgba(248,81,73,.15); color: var(--fail); border: 1px solid rgba(248,81,73,.4); }}
  .badge.timeout {{ background: rgba(210,153,34,.15); color: var(--timeout); border: 1px solid rgba(210,153,34,.4); }}
  .badge.error {{ background: rgba(248,81,73,.15); color: var(--error); border: 1px solid rgba(248,81,73,.4); }}
  .badge.unknown {{ background: rgba(139,147,161,.15); color: var(--muted); border: 1px solid rgba(139,147,161,.4); }}
  .kind {{ color: var(--muted); font-size: 11px; margin-left: 6px; }}
  pre {{
    margin: 0; padding: 10px 12px; background: var(--panel-2);
    border: 1px solid var(--border); border-radius: 8px; overflow-x: auto;
    font-family: "SF Mono", Menlo, Consolas, monospace; font-size: 12px;
  }}
  pre.detail {{ margin: 0; border-left: 3px solid var(--fail); }}
  pre.code {{ margin-top: 8px; }}
  .detail {{ border-left-color: var(--fail) !important; }}
  details {{ margin: 0; }}
  summary {{
    cursor: pointer; color: var(--accent); font-size: 12px;
    list-style: none; padding: 6px 0;
  }}
  summary::-webkit-details-marker {{ display: none; }}
  .muted {{ color: var(--muted); }}
  .note {{ color: var(--muted); font-size: 12px; margin: 8px 0 0; }}
  .cell-actions summary {{ margin-bottom: 4px; }}
  .gt {{ border-top: 1px dashed var(--border); padding-top: 4px; }}
</style>
</head>
<body>
<header>
  <h1>Kakoune benchmark report</h1>
  <div class="meta">Generated {escape(datetime.now().strftime('%Y-%m-%d %H:%M'))} &middot; {escape(escape('results.json'))}</div>
</header>
<div class="cards">{card_html}</div>
<div class="wrap">
  <table>
    <thead>
      <tr>
        <th>#</th><th>Result</th><th>Task</th><th>Detail</th><th></th>
      </tr>
    </thead>
    <tbody>
      """ + "\n".join(table_rows) + """
    </tbody>
  </table>
</div>
</body>
</html>"""


def main(argv=None):
    """CLI entry point: load results, render the report, write it out.

    Returns 0 on success or 1 on bad input (missing file, malformed JSON) with a
    single line to stderr. When no -o is given the HTML goes to stdout.
    """
    parser = argparse.ArgumentParser(description="Render a Kakoune benchmark run to HTML.")
    parser.add_argument("results", help="path to results.json")
    parser.add_argument("evals", nargs="?", help="optional path to evals.json (for instruction/ground truth)")
    parser.add_argument("-o", "--output", help="output HTML path (default: stdout)")
    args = parser.parse_args(argv)

    try:
        rows = [normalize_row(r) for r in load_results(args.results)]
    except FileNotFoundError:
        print(f"viewer: no such file: {args.results}", file=sys.stderr)
        return 1
    except (json.JSONDecodeError, ValueError) as exc:
        print(f"viewer: bad results.json: {exc}", file=sys.stderr)
        return 1

    evals = load_evals(args.evals) if args.evals else {}

    html = render(rows, evals)

    if args.output:
        with open(args.output, "w", encoding="utf-8") as fh:
            fh.write(html)
        print(f"viewer: wrote {args.output} ({len(rows)} tasks)", file=sys.stderr)
    else:
        sys.stdout.write(html)
    return 0


if __name__ == "__main__":
    sys.exit(main())
