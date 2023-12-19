import argparse
import base64
import os
import re
import sys
import http.server


class ChannelHandler(http.server.SimpleHTTPRequestHandler):
    url_pattern = re.compile(r"^/(?:t/[^/]+/)?([^/]+)")

    def do_GET(self) -> None:
        # First extract channel name
        channel_name = None
        if tuple(channels.keys()) != (None,):
            match = self.url_pattern.match(self.path)
            if match:
                channel_name = match.group(1)
                # Strip channel for file server
                start, end = match.span(1)
                self.path = self.path[:start] + self.path[end:]

        # Then dispatch to appropriate auth method
        if channel_name in channels:
            channel = channels[channel_name]
            self.directory = channel["directory"]
            auth = channel["auth"]
            if auth == "none":
                return super().do_GET()
            elif auth == "basic":
                server_key = base64.b64encode(
                    bytes(f"{channel['user']}:{channel['password']}", "utf-8")
                ).decode("ascii")
                return self.basic_do_GET(server_key=server_key)
            elif auth == "bearer":
                return self.bearer_do_GET(server_key=channel["bearer"])
            elif auth == "token":
                return self.token_do_GET(server_token=channel["token"])

        self.send_response(404)

    def basic_do_HEAD(self) -> None:
        self.send_response(200)
        self.send_header("Content-type", "text/html")
        self.end_headers()

    def basic_do_AUTHHEAD(self) -> None:
        self.send_response(401)
        self.send_header("WWW-Authenticate", 'Basic realm="Test"')
        self.send_header("Content-type", "text/html")
        self.end_headers()

    def bearer_do_GET(self, server_key: str) -> None:
        auth_header = self.headers.get("Authorization", "")
        print(auth_header)
        print(f"Bearer {server_key}")
        if not auth_header or auth_header != f"Bearer {server_key}":
            self.send_response(403)
            self.send_header("Content-type", "text/html")
            self.end_headers()
            self.wfile.write(b"no valid api key received")
        else:
            super().do_GET()

    def basic_do_GET(self, server_key: str) -> None:
        """Present frontpage with basic user authentication."""
        auth_header = self.headers.get("Authorization", "")

        if not auth_header:
            self.basic_do_AUTHHEAD()
            self.wfile.write(b"no auth header received")
        elif auth_header == "Basic " + server_key:
            super().do_GET()
        else:
            self.basic_do_AUTHHEAD()
            self.wfile.write(auth_header.encode("ascii"))
            self.wfile.write(b"not authenticated")

    token_pattern = re.compile("^/t/([^/]+?)/")

    def token_do_GET(self, server_token: str) -> None:
        """Present frontpage with user authentication."""
        match = self.token_pattern.search(self.path)
        if match:
            prefix_length = len(match.group(0)) - 1
            new_path = self.path[prefix_length:]
            found_token = match.group(1)
            if found_token == server_token:
                self.path = new_path
                return super().do_GET()

        self.send_response(403)
        self.send_header("Content-type", "text/html")
        self.end_headers()
        self.wfile.write(b"no valid api key received")


global_parser = argparse.ArgumentParser(description="Start a multi-channel conda package server.")
global_parser.add_argument("-p", "--port", type=int, default=8000, help="Port to use.")

channel_parser = argparse.ArgumentParser(description="Start a simple conda package server.")
channel_parser.add_argument(
    "-d",
    "--directory",
    type=str,
    default=os.getcwd(),
    help="Root directory for serving.",
)
channel_parser.add_argument(
    "-n",
    "--name",
    type=str,
    default=None,
    help="Unique name of the channel used in URL",
)
channel_parser.add_argument(
    "-a",
    "--auth",
    default=None,
    type=str,
    help="auth method (none, basic, token, or bearer)",
)
channel_parser.add_argument(
    "--token",
    type=str,
    default=None,
    help="Use token as API Key",
)
channel_parser.add_argument(
    "--bearer",
    type=str,
    default=None,
    help="Use bearer token as API Key",
)
channel_parser.add_argument(
    "--user",
    type=str,
    default=None,
    help="Use token as API Key",
)
channel_parser.add_argument(
    "--password",
    type=str,
    default=None,
    help="Use token as API Key",
)


# Gobal args can be given anywhere with the first set of args for backward compatibility.
args, argv_remaining = global_parser.parse_known_args()
PORT = args.port

# Iteratively parse arguments in sets.
# Each argument set, separated by -- in the CLI is for a channel.
# Credits: @hpaulj on SO https://stackoverflow.com/a/26271421
channels = {}
while argv_remaining:
    args, argv_remaining = channel_parser.parse_known_args(argv_remaining)
    # Drop leading -- to move to next argument set
    argv_remaining = argv_remaining[1:]
    # Consolidation
    if not args.auth:
        if args.user and args.password:
            args.auth = "basic"
        elif args.token:
            args.auth = "token"
        elif args.bearer:
            args.auth = "bearer"
        else:
            args.auth = "none"

    # name = args.name if args.name else Path(args.directory).name
    # args.name = name
    channels[args.name] = vars(args)

print(channels)

# Unamed channel in multi-channel case would clash URLs but we want to allow
# a single unamed channel for backward compatibility.
if (len(channels) > 1) and (None in channels):
    print("Cannot use empty channel name when using multiple channels", file=sys.stderr)
    exit(1)

server = http.server.HTTPServer(("", PORT), ChannelHandler)
print("Server started at localhost:" + str(PORT))
try:
    server.serve_forever()
except Exception:
    # Catch all sorts of interrupts
    print("Shutting server down")
    server.shutdown()
    print("Server shut down")
