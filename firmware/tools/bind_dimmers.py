#!/usr/bin/env python3
"""Bind the dimmers to their lights in zigbee2mqtt, via z2m groups.

For every entry in the bindings file the script
  1. creates the z2m group if it does not exist yet,
  2. adds the lights to the group,
  3. binds the dimmer endpoint to the group (on/off, level, color temperature),
so the dimmer controls its lights directly over Zigbee -- no Home Assistant
automation needed, and it keeps working while HA/z2m restart.

Everything is checked against z2m first (devices, model, endpoints, lights);
if anything is wrong nothing is changed. Re-running is safe: existing groups,
members and bindings are skipped.

  pip install paho-mqtt
  MQTT_PASSWORD=secret python tools/bind_dimmers.py tools/dimmer_bindings.json \\
      --host homeassistant.local --user mqtt

Add --dry-run to only print what would be done. See
tools/dimmer_bindings.example.json for the file format.
"""
import argparse
import json
import os
import sys
import threading
import time

try:
    import paho.mqtt.client as mqtt
except ImportError:
    mqtt = None

DIMMER_MODEL = "ESP_DIMMER_1"

# Controller endpoints of the dimmer and the clusters they send, see z2m/ESP_DIMMER_1.js
ENDPOINTS = {
    "controller": 1,
    "ext_encoder": 3,
    "ext_switch": 4,
}
CLUSTERS = {
    "controller": ["genOnOff", "genLevelCtrl", "lightingColorCtrl"],
    "ext_encoder": ["genOnOff", "genLevelCtrl", "lightingColorCtrl"],
    "ext_switch": ["genOnOff"],
}

RESPONSE_TIMEOUT = 15  # seconds


class Z2M:
    """Minimal request/response client for the zigbee2mqtt bridge API."""

    def __init__(self, host, port, user, password, base_topic):
        self.base = base_topic
        self.retained = {}
        self.responses = {}
        self.cond = threading.Condition()
        self.transaction = 0

        if hasattr(mqtt, "CallbackAPIVersion"):  # paho-mqtt >= 2.0
            self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        else:
            self.client = mqtt.Client()
        if user:
            self.client.username_pw_set(user, password)
        self.client.on_message = self._on_message
        self.client.connect(host, port)
        self.client.subscribe(f"{self.base}/bridge/info")
        self.client.subscribe(f"{self.base}/bridge/devices")
        self.client.subscribe(f"{self.base}/bridge/groups")
        self.client.subscribe(f"{self.base}/bridge/response/#")
        self.client.loop_start()

    def _on_message(self, client, userdata, msg):
        try:
            payload = json.loads(msg.payload)
        except ValueError:
            return
        with self.cond:
            if msg.topic.startswith(f"{self.base}/bridge/response/"):
                if isinstance(payload, dict) and "transaction" in payload:
                    self.responses[payload["transaction"]] = payload
            else:
                self.retained[msg.topic.rsplit("/", 1)[-1]] = payload
            self.cond.notify_all()

    def wait_for_state(self):
        """Wait for the retained bridge topics (info, devices, groups)."""
        deadline = time.time() + RESPONSE_TIMEOUT
        with self.cond:
            while not all(k in self.retained for k in ("info", "devices", "groups")):
                if not self.cond.wait(timeout=max(0, deadline - time.time())):
                    missing = {"info", "devices", "groups"} - set(self.retained)
                    sys.exit(f"No answer from zigbee2mqtt (missing bridge/{', bridge/'.join(sorted(missing))}). "
                             f"Check host, credentials and --base-topic.")
            return dict(self.retained)

    def request(self, command, payload):
        """Send a bridge request and return the response data, exit on error."""
        self.transaction += 1
        tid = f"bind_dimmers_{self.transaction}"
        payload = dict(payload, transaction=tid)
        self.client.publish(f"{self.base}/bridge/request/{command}", json.dumps(payload))
        deadline = time.time() + RESPONSE_TIMEOUT
        with self.cond:
            while tid not in self.responses:
                if not self.cond.wait(timeout=max(0, deadline - time.time())):
                    sys.exit(f"Timeout waiting for the response to {command} {json.dumps(payload)}")
            response = self.responses.pop(tid)
        if response.get("status") != "ok":
            sys.exit(f"{command} failed: {response.get('error')} (request: {json.dumps(payload)})")
        return response.get("data", {})

    def close(self):
        self.client.loop_stop()
        self.client.disconnect()


def load_bindings(path):
    with open(path, encoding="utf-8") as f:
        bindings = json.load(f)["bindings"]
    for i, b in enumerate(bindings):
        b.setdefault("endpoint", "controller")
        b.setdefault("lights", [])
        for key in ("dimmer", "group"):
            if not b.get(key):
                sys.exit(f"{path}: entry {i + 1} has no '{key}'")
    return bindings


def check(bindings, devices, groups):
    """Return a list of problems; empty if every entry can be applied."""
    by_name = {d["friendly_name"]: d for d in devices}
    group_names = {g["friendly_name"] for g in groups}
    problems = []
    seen = set()
    for b in bindings:
        where = f"{b['dimmer']} / {b['endpoint']}"
        if (b["dimmer"], b["endpoint"]) in seen:
            problems.append(f"{where}: listed twice")
        seen.add((b["dimmer"], b["endpoint"]))

        dimmer = by_name.get(b["dimmer"])
        if dimmer is None:
            problems.append(f"{where}: no device '{b['dimmer']}' in zigbee2mqtt")
        elif (dimmer.get("definition") or {}).get("model") != DIMMER_MODEL:
            problems.append(f"{where}: '{b['dimmer']}' is not an {DIMMER_MODEL} "
                            f"(is the external converter loaded?)")
        if b["endpoint"] not in ENDPOINTS:
            problems.append(f"{where}: unknown endpoint, use one of {', '.join(ENDPOINTS)}")
        if b["group"] in by_name:
            problems.append(f"{where}: group name '{b['group']}' is already used by a device")
        if not b["lights"] and b["group"] not in group_names:
            problems.append(f"{where}: new group '{b['group']}' needs at least one light")
        for light in b["lights"]:
            if light not in by_name:
                problems.append(f"{where}: no light '{light}' in zigbee2mqtt")
    return problems


def bind_request(z2m_version, dimmer, endpoint, group, clusters):
    """Bind payload; z2m 2.x takes the endpoint separately, 1.x as 'device/endpoint'."""
    if int(str(z2m_version).split(".")[0]) >= 2:
        return {"from": dimmer, "from_endpoint": endpoint, "to": group, "clusters": clusters}
    return {"from": f"{dimmer}/{endpoint}", "to": group, "clusters": clusters}


def apply(z2m, bindings, state, dry_run):
    version = state["info"].get("version", "2")
    by_name = {d["friendly_name"]: d for d in state["devices"]}
    groups = {g["friendly_name"]: g for g in state["groups"]}

    def do(command, payload):
        print(f"  {command} {json.dumps(payload, ensure_ascii=False)}")
        return {} if dry_run else z2m.request(command, payload)

    for b in bindings:
        dimmer, endpoint, group = b["dimmer"], b["endpoint"], b["group"]
        print(f"{dimmer} / {endpoint} -> {group}")

        # 1. group
        if group in groups:
            group_id = groups[group]["id"]
            members = {m["ieee_address"] for m in groups[group].get("members", [])}
        else:
            data = do("group/add", {"friendly_name": group})
            group_id = data.get("id")
            members = set()
            groups[group] = {"id": group_id, "members": []}

        # 2. lights in the group
        for light in b["lights"]:
            if by_name[light]["ieee_address"] in members:
                print(f"  '{light}' already in group")
                continue
            do("group/members/add", {"group": group, "device": light})
            members.add(by_name[light]["ieee_address"])

        # 3. binding dimmer endpoint -> group
        ep = (by_name[dimmer].get("endpoints") or {}).get(str(ENDPOINTS[endpoint]), {})
        bound = {bd["cluster"] for bd in ep.get("bindings", [])
                 if bd.get("target", {}).get("type") == "group" and bd["target"].get("id") == group_id}
        missing = [c for c in CLUSTERS[endpoint] if c not in bound]
        if not missing:
            print("  already bound")
            continue
        do("device/bind", bind_request(version, dimmer, endpoint, group, missing))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("bindings", help="JSON file with the dimmer -> group -> lights mapping")
    ap.add_argument("--host", default="localhost", help="MQTT broker (default: localhost)")
    ap.add_argument("--port", type=int, default=1883)
    ap.add_argument("--user", help="MQTT user; the password is read from $MQTT_PASSWORD")
    ap.add_argument("--base-topic", default="zigbee2mqtt")
    ap.add_argument("--dry-run", action="store_true", help="only print the requests, change nothing")
    args = ap.parse_args()

    if mqtt is None:
        sys.exit("paho-mqtt is missing: pip install paho-mqtt")

    bindings = load_bindings(args.bindings)
    z2m = Z2M(args.host, args.port, args.user, os.environ.get("MQTT_PASSWORD"), args.base_topic)
    try:
        state = z2m.wait_for_state()
        print(f"zigbee2mqtt {state['info'].get('version', '?')}, "
              f"{len(state['devices'])} devices, {len(state['groups'])} groups")
        problems = check(bindings, state["devices"], state["groups"])
        if problems:
            print("Nothing changed, fix these first:")
            for p in problems:
                print(f"  - {p}")
            sys.exit(1)
        apply(z2m, bindings, state, args.dry_run)
        print("Dry run, nothing changed." if args.dry_run else "Done.")
    finally:
        z2m.close()


if __name__ == "__main__":
    main()
