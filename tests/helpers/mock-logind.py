#!/usr/bin/env python3
"""Minimal mock of the systemd-logind D-Bus service.

This is intentionally small and is only intended for tests. It implements the
parts of org.freedesktop.login1.Manager that shutdown-menu will eventually use:

  - PowerOff(interactive)
  - Reboot(interactive)
  - CanPowerOff()
  - CanReboot()

Every power/reboot request is appended as one JSON object per line to the file
passed with --record. The helper with-mock-logind.sh starts a private D-Bus
session bus and points DBUS_SYSTEM_BUS_ADDRESS at it, so this mock can be used
without touching the real system bus or the real logind service.
"""

import argparse
import json
import os
import signal
import sys
import time

try:
    from gi.repository import Gio, GLib
except ImportError:
    print(
        "mock-logind: python3-gi is required (install python3-gi)",
        file=sys.stderr,
    )
    sys.exit(77)


BUS_NAME = "org.freedesktop.login1"
OBJECT_PATH = "/org/freedesktop/login1"
INTERFACE_NAME = "org.freedesktop.login1.Manager"

INTERFACE_XML = """
<node>
  <interface name="org.freedesktop.login1.Manager">
    <method name="PowerOff">
      <arg name="interactive" type="b" direction="in"/>
    </method>
    <method name="Reboot">
      <arg name="interactive" type="b" direction="in"/>
    </method>
    <method name="CanPowerOff">
      <arg name="result" type="s" direction="out"/>
    </method>
    <method name="CanReboot">
      <arg name="result" type="s" direction="out"/>
    </method>
  </interface>
</node>
"""


def parse_args():
    parser = argparse.ArgumentParser(
        description="Run a mock org.freedesktop.login1 service for tests.",
    )
    parser.add_argument(
        "--bus-address",
        help=(
            "D-Bus address to use. If omitted, connect to the system bus. "
            "Tests should always pass a private bus address."
        ),
    )
    parser.add_argument(
        "--record",
        help="Append received method calls to this JSON Lines file.",
    )
    parser.add_argument(
        "--ready-file",
        help="Write this file after the mock owns org.freedesktop.login1.",
    )
    parser.add_argument(
        "--can-result",
        choices=("yes", "no", "na", "challenge"),
        default="yes",
        help="Value returned by CanPowerOff and CanReboot (default: yes).",
    )
    parser.add_argument(
        "--exit-after",
        type=int,
        default=0,
        metavar="N",
        help="Stop after N PowerOff/Reboot calls (0 means never stop).",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    calls = []
    result = {"exit_code": 0}

    if args.bus_address:
        connection = Gio.DBusConnection.new_for_address_sync(
            args.bus_address,
            Gio.DBusConnectionFlags.AUTHENTICATION_CLIENT
            | Gio.DBusConnectionFlags.MESSAGE_BUS_CONNECTION,
            None,
            None,
        )
    else:
        connection = Gio.bus_get_sync(Gio.BusType.SYSTEM, None)

    node_info = Gio.DBusNodeInfo.new_for_xml(INTERFACE_XML)
    interface_info = node_info.lookup_interface(INTERFACE_NAME)
    loop = GLib.MainLoop()

    def record_call(method, interactive):
        event = {
            "method": method,
            "interactive": interactive,
            "time": time.time(),
        }
        calls.append(event)
        if args.record:
            with open(args.record, "a", encoding="utf-8") as record_file:
                record_file.write(json.dumps(event, sort_keys=True) + "\n")
        print(
            "mock-logind: " + json.dumps(event, sort_keys=True),
            file=sys.stderr,
            flush=True,
        )
        if args.exit_after and len(calls) >= args.exit_after:
            GLib.idle_add(loop.quit)

    def method_call(
        connection,
        sender,
        object_path,
        interface_name,
        method_name,
        parameters,
        invocation,
    ):
        del connection, sender, object_path, interface_name

        if method_name in ("PowerOff", "Reboot"):
            interactive = bool(parameters.unpack()[0])
            record_call(method_name, interactive)
            invocation.return_value(None)
            return

        if method_name in ("CanPowerOff", "CanReboot"):
            invocation.return_value(GLib.Variant("(s)", (args.can_result,)))
            return

        invocation.return_dbus_error(
            "org.freedesktop.DBus.Error.UnknownMethod",
            "mock-logind does not implement " + method_name,
        )

    connection.register_object(
        OBJECT_PATH,
        interface_info,
        method_call,
        None,
        None,
    )

    def name_acquired(connection, name):
        del connection
        print("mock-logind: acquired " + name, file=sys.stderr, flush=True)
        if args.ready_file:
            with open(args.ready_file, "w", encoding="utf-8") as ready_file:
                ready_file.write(name + "\n")

    def name_lost(connection, name):
        del connection
        print(
            "mock-logind: could not acquire " + name,
            file=sys.stderr,
            flush=True,
        )
        result["exit_code"] = 1
        loop.quit()

    Gio.bus_own_name_on_connection(
        connection,
        BUS_NAME,
        Gio.BusNameOwnerFlags.NONE,
        name_acquired,
        name_lost,
    )

    def stop(_signum, _frame):
        loop.quit()

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)

    try:
        loop.run()
    finally:
        if args.ready_file and os.path.exists(args.ready_file):
            os.unlink(args.ready_file)

    return result["exit_code"]


if __name__ == "__main__":
    sys.exit(main())
