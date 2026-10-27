"""Independent native Tox peer for Android interoperability, or a disposable qTox profile.

Uses the actual shared library supplied by an installed Tox client; no transport simulation.
Public rendezvous information only is written to peer.json. Savedata stays in the ignored
test directory. Never point this script at a personal profile or run the identity twice.
"""
import argparse
import ctypes as c
import json
import os
from pathlib import Path
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", required=True)
    parser.add_argument("--directory", required=True)
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument("--seconds", type=int, default=600)
    args = parser.parse_args()
    library = Path(args.library).resolve()
    dll_dir = os.add_dll_directory(str(library.parent)) if os.name == "nt" else None
    lib = c.CDLL(str(library))
    ptr = c.c_void_p
    byteptr = c.POINTER(c.c_uint8)

    def fn(name, result, *params):
        f = getattr(lib, name); f.restype = result; f.argtypes = params
        return f

    options_new = fn("tox_options_new", ptr, c.POINTER(c.c_int))
    options_free = fn("tox_options_free", None, ptr)
    ipv6 = fn("tox_options_set_ipv6_enabled", None, ptr, c.c_bool)
    create = fn("tox_new", ptr, ptr, c.POINTER(c.c_int))
    kill = fn("tox_kill", None, ptr)
    name = fn("tox_self_set_name", c.c_bool, ptr, ptr, c.c_size_t, ptr)
    address = fn("tox_self_get_address", None, ptr, byteptr)
    dht_key = fn("tox_self_get_dht_id", None, ptr, byteptr)
    udp_port = fn("tox_self_get_udp_port", c.c_uint16, ptr, ptr)
    save_size = fn("tox_get_savedata_size", c.c_size_t, ptr)
    save = fn("tox_get_savedata", None, ptr, byteptr)
    iterate = fn("tox_iterate", None, ptr, ptr)
    status = fn("tox_self_get_connection_status", c.c_int, ptr)
    bootstrap = fn("tox_bootstrap", c.c_bool, ptr, c.c_char_p, c.c_uint16, ptr, ptr)
    relay = fn("tox_add_tcp_relay", c.c_bool, ptr, c.c_char_p, c.c_uint16, ptr, ptr)
    accept = fn("tox_friend_add_norequest", c.c_uint32, ptr, ptr, ptr)
    send = fn("tox_friend_send_message", c.c_uint32, ptr, c.c_uint32, c.c_int, ptr, c.c_size_t, ptr)
    request_cb = c.CFUNCTYPE(None, ptr, byteptr, byteptr, c.c_size_t, ptr)
    message_cb = c.CFUNCTYPE(None, ptr, c.c_uint32, c.c_int, byteptr, c.c_size_t, ptr)
    set_request = fn("tox_callback_friend_request", None, ptr, request_cb)
    set_message = fn("tox_callback_friend_message", None, ptr, message_cb)
    directory = Path(args.directory).resolve()
    directory.mkdir(parents=True, exist_ok=True)
    identity = directory / "qtox-interop.tox"
    if identity.exists():
        raise SystemExit("Refusing to replace test identity: choose a new directory")
    error = c.c_int()
    options = options_new(c.byref(error)); ipv6(options, False)
    tox = create(options, c.byref(error)); options_free(options)
    if not tox or error.value:
        raise RuntimeError("tox_new failed")
    try:
        display_name = b"qTox interop"
        name(tox, display_name, len(display_name), None)
        buf = (c.c_uint8 * save_size(tox))(); save(tox, buf)
        identity.write_bytes(bytes(buf)); c.memset(buf, 0, len(buf))
        addr = (c.c_uint8 * 38)(); address(tox, addr)
        key = (c.c_uint8 * 32)(); dht_key(tox, key)
        version = ".".join(str(fn("tox_version_" + part, c.c_uint32)()) for part in ("major", "minor", "patch"))
        public = {"address": bytes(addr).hex().upper(), "dhtKey": bytes(key).hex().upper(), "port": udp_port(tox, None), "version": version}
        (directory / "peer.json").write_text(json.dumps(public, indent=2), encoding="utf-8")
        print("native_peer_prepared; toxcore=" + version, flush=True)
        if args.prepare_only:
            return

        @request_cb
        def on_request(t, public_key, message, size, user):
            accept(t, public_key, None)
            print("friend_request_accepted", flush=True)

        @message_cb
        def on_message(t, friend, kind, text, size, user):
            # A fixed test response; never copy/log user message content.
            response = "qTox → Parley ✓".encode()
            e = c.c_int(); send(t, friend, 0, response, len(response), c.byref(e))
            print("message_received_and_reply_queued" if not e.value else "reply_failed", flush=True)

        set_request(tox, on_request); set_message(tox, on_message)
        seeds = [
            ("tox1.mf-net.eu", 33445, "B3E5FA80DC8EBD1149AD2AB35ED8B85BD546DEDE261CA593234C619249419506"),
            ("144.217.167.73", 33445, "7E5668E0EE09E19F320AD47902419331FFEE147BB3606769CFBE921A2A2FD34C"),
        ]
        deadline = time.monotonic() + args.seconds; next_bootstrap = 0; last = -1
        while time.monotonic() < deadline:
            now = time.monotonic()
            if now >= next_bootstrap:
                for host, port, pub in seeds:
                    k = bytes.fromhex(pub); bootstrap(tox, host.encode(), port, k, None); relay(tox, host.encode(), port, k, None)
                next_bootstrap = now + 30
            iterate(tox, None)
            connection = status(tox)
            if connection != last:
                print("native_peer_connection=" + str(connection), flush=True); last = connection
            time.sleep(.02)
    finally:
        kill(tox)
        if dll_dir:
            dll_dir.close()


if __name__ == "__main__":
    main()
