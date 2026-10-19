#!/usr/bin/env python3
"""Design Studio comments, for agents. No server needed; the studio picks up changes live.

  python3 tools/apps/web_runner/studio_comments.py list            comments sent to you, as Markdown
  python3 tools/apps/web_runner/studio_comments.py reply 3 "…"     ask the designer something (not resolved)
  python3 tools/apps/web_runner/studio_comments.py resolve 3 "…"   say what you changed (resolved)

Workflow (AGENTS.md › "Address the Design Studio comments"): list, make the changes in
tokens.json / prototype.json (checking context/design-system.md), check them in the studio,
then reply or resolve each one. Resolving marks your edits in the studio's version history
with the Agent badge.
"""
import json
import re
import sys
import time

import studio_store as st


def part_json(screen, path):
    """The part's JSON from prototype.json, from an inspector path like home.body[0][1]."""
    proto = json.loads(st.read("prototype"))
    scr = proto.get("screens", {}).get(screen)
    if scr is None:
        return None
    m = re.match(r"^[a-z0-9_]+\.(navbar|body|actions)((?:\[\d+\])*)$", path or "")
    if not m:
        return scr
    node = scr.get(m.group(1))
    for i, n in enumerate(int(x) for x in re.findall(r"\d+", m.group(2))):
        node = node[n] if i == 0 else node.get("children", [])[n]
    return node


def cmd_list():
    db = st.comments()
    todo = [c for c in db["comments"] if c["status"] == "sent"]
    if not todo:
        print("No Design Studio comments are waiting.")
        return
    print("# Design Studio comments (%d waiting)\n" % len(todo))
    for c in todo:
        when = time.strftime("%b %d %H:%M", time.localtime(c.get("sent_at", c["created"])))
        print("## #%d · screen `%s` · %s" % (c["id"], c["screen"], c.get("part") or "the screen"))
        print("- Sent %s, pinned on the %s at (%s, %s) pt" % (when, c.get("size") or "320x480",
                                                             round(c.get("x") or 0), round(c.get("y") or 0)))
        if c.get("path"):
            print("- Part: `%s` (inspector path)" % c["path"])
        if c.get("tokens"):
            print("- Tokens: " + ", ".join("%s `%s`" % kv for kv in c["tokens"].items()))
        if c.get("snapshot"):
            print("- Snapshot: `%s`" % c["snapshot"])
        print()
        for m in c["thread"]:
            print("> **%s:** %s" % ("Designer" if m["who"] == "you" else "Agent", m["text"]))
        node = part_json(c["screen"], c.get("path"))
        if node is not None:
            print("\n```json\n" + json.dumps(node, indent=2, ensure_ascii=False) + "\n```")
        print()


def main(argv):
    if len(argv) < 2 or argv[1] not in ("list", "reply", "resolve"):
        print(__doc__)
        return 2
    if argv[1] == "list":
        cmd_list()
        return 0
    if len(argv) < 4:
        print("Usage: studio_comments.py %s <id> \"text\"" % argv[1])
        return 2
    try:
        c = st.agent_answer(int(argv[2].lstrip("#")), argv[3], resolve=argv[1] == "resolve")
    except (st.EditError, ValueError) as e:
        print(e)
        return 1
    print("#%d %s." % (c["id"], "resolved" if c["status"] == "resolved" else "answered; waiting for the designer"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
