"""Tests for the Design Studio data layer (tools/apps/web_runner/studio_store.py).

Run: python3 -m unittest tests/test_studio_store.py
"""
import json
import os
import shutil
import sys
import tempfile
import time
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools", "apps", "web_runner"))
import studio_store as st  # noqa: E402

REAL = {n: st.read(n) for n in st.FILES}


class Surgical(unittest.TestCase):
    def check(self, text, ops):
        out = st.apply_ops(text, ops)
        data = json.loads(text)
        for op in ops:
            found, _ = st.get(data, op["path"][:-1])
            if op.get("delete"):
                parent = st.get(data, op["path"][:-1])[1]
                del parent[op["path"][-1]]
            else:
                parent = st.get(data, op["path"][:-1])[1]
                parent[op["path"][-1]] = op["value"]
        self.assertEqual(json.loads(out), data)
        return out

    def changed_lines(self, a, b):
        la, lb = a.split("\n"), b.split("\n")
        return [x for x in lb if x not in la]

    def test_leaf_edit_touches_one_line(self):
        t = REAL["prototype"]
        out = self.check(t, [{"path": ["screens", "psbt_overview", "actions", 0, "label"], "value": "Check details"}])
        self.assertEqual(len(out.split("\n")), len(t.split("\n")))
        self.assertEqual(len(self.changed_lines(t, out)), 1)
        self.assertIn('"label": "Check details"', out)

    def test_token_edit(self):
        t = REAL["tokens"]
        out = self.check(t, [{"path": ["metric", "rowH"], "value": 52}])
        self.assertEqual(self.changed_lines(t, out), ['    "rowH": 52,'])
        out = self.check(t, [{"path": ["type", "hero", "size"], "value": 27}])
        self.assertIn('"hero":    { "size": 27, "lh": 30 }', out)

    def test_replace_whole_screen_keeps_untouched_text(self):
        t = REAL["prototype"]
        scr = json.loads(t)["screens"]["seeds"]
        scr["navbar"]["title"] = "Your seeds"
        out = self.check(t, [{"path": ["screens", "seeds"], "value": scr}])
        self.assertEqual(len(self.changed_lines(t, out)), 1)

    def test_add_and_delete_keys(self):
        t = REAL["prototype"]
        out = self.check(t, [{"path": ["screens", "tools", "body", 0, "muted"], "delete": True}])
        self.assertIn('{ "type": "list_item", "icon": "derivation", "label": "Address explorer" }', out)
        out = self.check(out, [{"path": ["screens", "tools", "body", 0, "go"], "value": "seeds"}])
        self.assertIn('"label": "Address explorer", "go": "seeds" }', out)
        out = self.check(t, [{"path": ["flows"], "value": [{"id": "a", "title": "A", "description": "", "screens": ["home"]}]}])
        self.assertIn('"flows": [', out)

    def test_expect(self):
        t = REAL["tokens"]
        st.apply_ops(t, [{"path": ["metric", "rowH"], "expect": 48, "value": 50}])
        with self.assertRaises(st.EditError):
            st.apply_ops(t, [{"path": ["metric", "rowH"], "expect": 52, "value": 50}])
        with self.assertRaises(st.EditError):
            st.apply_ops(t, [{"path": ["metric", "rowH"], "expect_missing": True, "delete": True}])

    def test_house_style_for_new_blocks(self):
        v = {"type": "group", "children": [{"type": "text", "text": "a"}, {"type": "text", "text": "b"}]}
        self.assertEqual(st.house(v, 8), '{ "type": "group", "children": [\n'
                         '          { "type": "text", "text": "a" },\n'
                         '          { "type": "text", "text": "b" }\n        ]}')


class Stores(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.saved = (dict(st.FILES), st.STORE, st.HISTORY, st.COMMENTS, st.SNAPS)
        for n, text in REAL.items():
            p = os.path.join(self.tmp, n + ".json")
            st.write_atomic(p, text)
            st.FILES[n] = p
        st.STORE = os.path.join(self.tmp, ".studio")
        st.HISTORY = os.path.join(st.STORE, "history")
        st.COMMENTS = os.path.join(st.STORE, "comments.json")
        st.SNAPS = os.path.join(st.STORE, "comments")

    def tearDown(self):
        st.FILES.clear()
        st.FILES.update(self.saved[0])
        st.STORE, st.HISTORY, st.COMMENTS, st.SNAPS = self.saved[1:]
        shutil.rmtree(self.tmp)

    def test_edit_on_a_file_changed_outside_keeps_both(self):
        st.record("outside", "Starting point")
        other = st.read("tokens").replace('"rowH": 48', '"rowH": 50')
        st.write_atomic(st.FILES["tokens"], other)       # an agent's edit
        st.edit("tokens", [{"path": ["metric", "tap"], "expect": 44, "value": 46}], "Changed tap")
        d = json.loads(st.read("tokens"))
        self.assertEqual((d["metric"]["rowH"], d["metric"]["tap"]), (50, 46))

    def test_history_restore_and_name(self):
        first = st.record("outside", "Starting point")
        st.edit("tokens", [{"path": ["metric", "rowH"], "value": 52}], "rowH 52")
        self.assertEqual([v["who"] for v in st.history()], ["outside", "you"])
        st.restore(first["id"])
        self.assertEqual(st.read("tokens"), REAL["tokens"])
        self.assertEqual(st.history()[-1]["title"], "Restored “Starting point”")
        st.name_latest("Before the row test")
        self.assertEqual(st.history()[-1]["name"], "Before the row test")
        self.assertIsNone(st.record("you", "nothing changed"))

    def test_comments_and_agent_badge(self):
        st.record("outside", "Starting point")
        db = st.comment_action({"action": "create", "screen": "home", "path": None, "part": "tile",
                                "x": 10, "y": 20, "size": "320x480", "text": "Bigger Scan button"})
        cid = db["comments"][0]["id"]
        st.comment_action({"action": "send", "snapshots": {}})
        self.assertEqual(st.comments()["comments"][0]["status"], "sent")
        time.sleep(0.01)
        st.write_atomic(st.FILES["tokens"], REAL["tokens"].replace('"tileH": 84', '"tileH": 90'))
        st.record("outside", "Changed outside the studio")
        st.agent_answer(cid, "Raised metric.tileH to 90.", resolve=True)
        self.assertEqual(st.comments()["comments"][0]["status"], "resolved")
        self.assertEqual(st.history()[-1]["who"], "agent")
        self.assertEqual(st.history()[0]["who"], "outside")      # older outside edits stay unclaimed
        st.comment_action({"action": "reply", "id": cid, "text": "One more step"})
        self.assertEqual(st.comments()["comments"][0]["status"], "draft")


if __name__ == "__main__":
    unittest.main()
