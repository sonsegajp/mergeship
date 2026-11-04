"""A local stand-in for the Anchor server (anchor.hm64.org), for MergeShip2's Anchor tests.

Anchor (SoH soh/Network/Anchor, 2Ship 2s2h/Network/Anchor) talks NUL-delimited JSON over TCP. This server does what
the clients need from the real one, as far as the clients' code shows it:

  HANDSHAKE            joins the room (creating it with the handshake's roomState, owned by that client), answers
                       UPDATE_ROOM_STATE, and sends ALL_CLIENT_STATE (with "self") to everyone in the room
  UPDATE_CLIENT_STATE  stored, and passed on to the room
  UPDATE_ROOM_STATE    stored, and passed on to the room
  targetClientId       passed on to that client only
  targetTeamId         passed on to the team's other online members; with "addToQueue", kept in the team's queue for
                       members who are offline
  REQUEST_TEAM_STATE   passed on to the team's other online members; with none online, answered with the stored
                       UPDATE_TEAM_STATE and the queue
  UPDATE_TEAM_STATE    stored as the team's state (its queue emptied), passed on to the team
  anything else        passed on to the room
Every packet passed on carries the sender's "clientId".

For tests it also has a bot, "MockBot" (a room member that is always online, MergeShip2's client version), and a
control packet a test script sends through its client:

  {"type": "MOCK_ECHO", "delayMs": 8000, "packet": {...}, "world": "oot"}
      after delayMs, `packet` is sent to the sender as if MockBot had sent it (e.g. a GIVE_ITEM found in Hyrule that
      reaches the player while he is in Termina).

Everything is logged (stdout and --log): CONNECT / HANDSHAKE / DISCONNECT lines and one line per packet type.

    python tools/anchor_mock_server.py [--port 43383] [--log anchor_server.log]
"""

import argparse
import json
import socket
import threading
import time

CLIENT_VERSION = "MergeShip2-Anchor-2"  # include/mergeship/AnchorNet.h MS_ANCHOR_CLIENT_VERSION
BOT_ID = 9000


class Server:
    def __init__(self, log_path):
        self.lock = threading.RLock()
        self.log_file = open(log_path, "a", encoding="utf-8") if log_path else None
        self.next_id = 1
        self.next_conn = 1
        self.clients = {}  # clientId -> Client
        self.rooms = {}  # roomId -> {"state": {...}, "clients": set(clientIds), "teams": {teamId: {...}}}

    def log(self, text):
        line = time.strftime("%H:%M:%S") + " " + text
        print(line, flush=True)
        if self.log_file:
            self.log_file.write(line + "\n")
            self.log_file.flush()

    # ---- sending ------------------------------------------------------------------------------------------------

    def send(self, client, packet):
        if client is None or client.sock is None:
            return
        try:
            client.sock.sendall(json.dumps(packet).encode("utf-8") + b"\0")
        except OSError as e:
            self.log(f"send to {client.id} failed: {e}")

    def room_members(self, room_id):
        room = self.rooms.get(room_id)
        return [self.clients[c] for c in room["clients"] if c in self.clients] if room else []

    def client_state(self, client, recipient):
        state = dict(client.state)
        state["clientId"] = client.id
        state["online"] = client.sock is not None
        state["self"] = client is recipient
        return state

    def bot_state(self, world):
        return {
            "clientId": BOT_ID, "name": "MockBot", "color": {"r": 255, "g": 200, "b": 0},
            "clientVersion": CLIENT_VERSION, "teamId": "default", "online": True, "self": False, "seed": 0,
            "isSaveLoaded": True, "isGameComplete": False, "sceneNum": 85, "curRoomNum": 0, "entranceIndex": 0,
            "world": world, "sceneName": "Kokiri Forest" if world == "oot" else "Clock Town",
        }

    def send_all_client_state(self, room_id):
        members = self.room_members(room_id)
        for recipient in members:
            if recipient.sock is None:
                continue
            state = [self.client_state(c, recipient) for c in members] + [self.bot_state("oot")]
            self.send(recipient, {"type": "ALL_CLIENT_STATE", "state": state})

    # ---- receiving ----------------------------------------------------------------------------------------------

    def handle(self, client, packet):
        kind = packet.get("type", "?")
        if not packet.get("quiet"):
            self.log(f"conn#{client.conn} client {client.id}: {kind}")
            if kind.endswith("GIVE_ITEM"):  # the item itself, for item tests
                self.log("    " + json.dumps({k: v for k, v in packet.items() if k != "type"}))
        packet["clientId"] = client.id if kind != "HANDSHAKE" else packet.get("clientId", 0)

        if kind == "HANDSHAKE":
            self.handshake(client, packet)
            return
        if client.room is None:
            return
        room = self.rooms[client.room]

        if kind == "MOCK_ECHO":
            delay = packet.get("delayMs", 0) / 1000.0
            echo = dict(packet.get("packet", {}))
            echo["clientId"] = BOT_ID

            def later():
                time.sleep(delay)
                with self.lock:
                    self.log(f"MOCK_ECHO -> client {client.id}: {echo.get('type')}")
                    self.send(client, echo)

            threading.Thread(target=later, daemon=True).start()
            return

        if kind == "UPDATE_CLIENT_STATE":
            client.state = packet.get("state", client.state)
        elif kind == "UPDATE_ROOM_STATE":
            room["state"] = packet.get("state", room["state"])
            for member in self.room_members(client.room):
                self.send(member, packet)
            return
        elif kind == "UPDATE_TEAM_STATE" and "targetTeamId" in packet:
            team = room["teams"].setdefault(packet["targetTeamId"], {"state": None, "queue": []})
            team["state"] = packet.get("state")
            team["queue"] = []
        elif kind == "REQUEST_TEAM_STATE":
            team_id = packet.get("targetTeamId", "default")
            mates = [m for m in self.room_members(client.room)
                     if m is not client and m.sock is not None and m.state.get("teamId", "default") == team_id]
            if not mates:
                team = room["teams"].get(team_id, {"state": None, "queue": []})
                reply = {"type": "UPDATE_TEAM_STATE", "targetTeamId": team_id, "clientId": 0,
                         "queue": [json.dumps(p) for p in team["queue"]]}
                if team["state"] is not None:
                    reply["state"] = team["state"]
                self.send(client, reply)
                return

        if "targetClientId" in packet:
            target = self.clients.get(packet["targetClientId"])
            if target is not None and target.room == client.room:
                self.send(target, packet)
            return
        if "targetTeamId" in packet:
            team_id = packet["targetTeamId"]
            team = room["teams"].setdefault(team_id, {"state": None, "queue": []})
            offline = False
            for member in self.room_members(client.room):
                if member is client or member.state.get("teamId", "default") != team_id:
                    continue
                if member.sock is None:
                    offline = True
                else:
                    self.send(member, packet)
            if packet.get("addToQueue") and offline:
                team["queue"].append(packet)
            return
        for member in self.room_members(client.room):
            if member is not client:
                self.send(member, packet)

    def handshake(self, client, packet):
        room_id = packet.get("roomId", "")
        wanted = packet.get("clientId", 0)
        if wanted and wanted in self.clients and self.clients[wanted].sock is None:
            # The same player again (a reconnect): take back the old id.
            old = self.clients.pop(wanted)
            client.id = wanted
            self.clients[wanted] = client
            del old
        else:
            client.id = self.next_id
            self.next_id += 1
            self.clients[client.id] = client
        client.room = room_id
        client.state = packet.get("clientState", {})
        room = self.rooms.get(room_id)
        if room is None:
            state = dict(packet.get("roomState", {}))
            state["ownerClientId"] = client.id
            room = self.rooms[room_id] = {"state": state, "clients": set(), "teams": {}}
        room["clients"].add(client.id)
        self.log(f"HANDSHAKE conn#{client.conn}: client {client.id} in room '{room_id}' "
                 f"(world {client.state.get('world', 'oot')}, name {client.state.get('name')})")
        self.send(client, {"type": "UPDATE_ROOM_STATE", "state": room["state"]})
        self.send_all_client_state(room_id)

    def serve(self, sock, addr):
        with self.lock:
            client = Client(sock, self.next_conn)
            self.next_conn += 1
        self.log(f"CONNECT conn#{client.conn} from {addr[0]}:{addr[1]}")
        buffer = b""
        try:
            while True:
                data = sock.recv(65536)
                if not data:
                    break
                buffer += data
                while b"\0" in buffer:
                    raw, buffer = buffer.split(b"\0", 1)
                    if not raw:
                        continue
                    try:
                        packet = json.loads(raw.decode("utf-8"))
                    except ValueError:
                        self.log(f"conn#{client.conn}: bad JSON ({len(raw)} bytes)")
                        continue
                    with self.lock:
                        self.handle(client, packet)
        except OSError as e:
            self.log(f"conn#{client.conn}: {e}")
        with self.lock:
            client.sock = None
            self.log(f"DISCONNECT conn#{client.conn} (client {client.id})")
            if client.room is not None:
                self.send_all_client_state(client.room)
        sock.close()


class Client:
    def __init__(self, sock, conn):
        self.sock = sock
        self.conn = conn
        self.id = 0
        self.room = None
        self.state = {}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=43383)
    parser.add_argument("--log", default="")
    args = parser.parse_args()
    server = Server(args.log)
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", args.port))
    listener.listen()
    server.log(f"LISTEN 127.0.0.1:{args.port}")
    while True:
        sock, addr = listener.accept()
        threading.Thread(target=server.serve, args=(sock, addr), daemon=True).start()


if __name__ == "__main__":
    main()
