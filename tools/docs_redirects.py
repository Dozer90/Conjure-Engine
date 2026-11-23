"""Read the small redirect contract used to preserve moved handbook URLs."""

import json
import re


def metadata(text):
    front = re.match(r"\A---\n(.*?)\n---(?:\n|$)", text, re.DOTALL)
    if not front:
        return None
    target = re.search(r"(?m)^redirect_to:\s*(\S+)\s*$", front.group(1))
    if not target:
        return None
    fragments = re.search(r"(?m)^redirect_fragments:[ \t]*(.+)$", front.group(1))
    routes = json.loads(fragments.group(1)) if fragments else {}
    if not isinstance(routes, dict) or any(not isinstance(k, str) or not isinstance(v, str)
                                           for k, v in routes.items()):
        raise ValueError("redirect_fragments must map section anchors to relative page links")
    return target.group(1), routes
