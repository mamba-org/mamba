import argparse
import base64
import http.server
import re
import socketserver

parser = argparse.ArgumentParser(description="Start a simple conda package server.")
parser.add_argument("-p", "--port", type=int, default=8000, help="Port to use.")
parser.add_argument(
    "-a",
    "--auth",
    default="none",
    type=str,
    help="auth method (none, basic, or token)",
)
args = parser.parse_args()

key = "user:test"
# key = base64.b64encode(key)
key = base64.b64encode(bytes(key, "utf-8")).decode("ascii")

api_key = "xy-12345678-1234-1234-1234-123456789012"


class BasicAuthHandler(http.server.SimpleHTTPRequestHandler):
    """ Main class to present webpages and authentication. """

    def do_HEAD(self):
        print("send header")
        self.send_response(200)
        self.send_header("Content-type", "text/html")
        self.end_headers()

    def do_AUTHHEAD(self):
        print("send header")
        self.send_response(401)
        self.send_header("WWW-Authenticate", 'Basic realm="Test"')
        self.send_header("Content-type", "text/html")
        self.end_headers()

    def do_GET(self):
        global key
        """ Present frontpage with user authentication. """
        auth_header = self.headers.get("Authorization", "")
        print("Auth header: ", auth_header)
        print(key)
        if not auth_header:
            self.do_AUTHHEAD()
            self.wfile.write(b"no auth header received")
            pass
        elif auth_header == "Basic " + key:
            http.server.SimpleHTTPRequestHandler.do_GET(self)
            pass
        else:
            self.do_AUTHHEAD()
            self.wfile.write(auth_header)
            self.wfile.write(b"not authenticated")
            pass


class CondaTokenHandler(http.server.SimpleHTTPRequestHandler):
    """ Main class to present webpages and authentication. """

    token_pattern = re.compile("^/t/([^/]+?)/")

    def do_GET(self):
        """ Present frontpage with user authentication. """
        global api_key
        match = self.token_pattern.search(self.path)
        if match:
            prefix_length = len(match.group(0)) - 1
            new_path = self.path[prefix_length:]
            found_api_key = match.group(1)
            print("API Key: ", found_api_key)
            print("New Path: ", new_path)
            if found_api_key == api_key:
                self.path = new_path
                return http.server.SimpleHTTPRequestHandler.do_GET(self)

        self.send_response(403)
        self.wfile.write(b"no auth header received")


if not args.auth or args.auth == "none":
    handler = http.server.SimpleHTTPRequestHandler
elif not args.auth or args.auth == "basic":
    handler = BasicAuthHandler
elif not args.auth or args.auth == "token":
    handler = CondaTokenHandler

PORT = args.port

server = socketserver.TCPServer(("", PORT), handler)
print("Server started at localhost:" + str(PORT))
try:
    server.serve_forever()
except KeyboardInterrupt:
    server.shutdown()
