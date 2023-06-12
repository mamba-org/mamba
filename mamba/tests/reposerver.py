import argparse
import base64
import glob
import os
import re
import shutil
import sys
from http.server import HTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
from typing import Dict, List

try:
    import conda_content_trust.authentication as cct_authentication
    import conda_content_trust.common as cct_common
    import conda_content_trust.metadata_construction as cct_metadata_construction
    import conda_content_trust.root_signing as cct_root_signing
    import conda_content_trust.signing as cct_signing

    conda_content_trust_available = True
except ImportError:
    conda_content_trust_available = False


def fatal_error(message: str) -> None:
    """Print error and exit."""
    print(message, file=sys.stderr)
    exit(1)


def get_fingerprint(gpg_output: str) -> str:
    lines = gpg_output.splitlines()
    fpline = lines[1].strip()
    fpline = fpline.replace(" ", "")
    return fpline


KeySet = Dict[str, List[Dict[str, str]]]


def normalize_keys(keys: KeySet) -> KeySet:
    out = {}
    for ik, iv in keys.items():
        out[ik] = []
        for el in iv:
            if isinstance(el, str):
                el = el.lower()
                keyval = cct_root_signing.fetch_keyval_from_gpg(el)
                res = {"fingerprint": el, "public": keyval}
            elif isinstance(el, dict):
                res = {
                    "private": el["private"].lower(),
                    "public": el["public"].lower(),
                }
            out[ik].append(res)

    return out


class RepoSigner:
    keys = {
        "root": [],
        "key_mgr": [
            {
                "private": "c9c2060d7e0d93616c2654840b4983d00221d8b6b69c850107da74b42168f937",
                "public": "013ddd714962866d12ba5bae273f14d48c89cf0773dee2dbf6d4561e521c83f7",
            },
        ],
        "pkg_mgr": [
            {
                "private": "f3cdab14740066fb277651ec4f96b9f6c3e3eb3f812269797b9656074cd52133",
                "public": "f46b5a7caa43640744186564c098955147daa8bac4443887bc64d8bfee3d3569",
            }
        ],
    }

    def __init__(self, in_folder: str) -> None:
        self.in_folder = Path(in_folder).resolve()
        self.folder = self.in_folder.parent / (str(self.in_folder.name) + "_signed")
        self.keys["root"] = [
            get_fingerprint(os.environ["KEY1"]),
            get_fingerprint(os.environ["KEY2"]),
        ]
        self.keys = normalize_keys(self.keys)

    def make_signed_repo(self) -> Path:
        print("[reposigner] Using keys:", self.keys)
        print("[reposigner] Using folder:", self.folder)

        self.folder.mkdir(exist_ok=True)
        self.create_root(self.keys)
        self.create_key_mgr(self.keys)
        for f in glob.glob(str(self.in_folder / "**" / "repodata.json")):
            self.sign_repodata(Path(f), self.keys)
        return self.folder

    def create_root(self, keys):
        root_keys = keys["root"]

        root_pubkeys = [k["public"] for k in root_keys]
        key_mgr_pubkeys = [k["public"] for k in keys["key_mgr"]]

        root_version = 1

        root_md = cct_metadata_construction.build_root_metadata(
            root_pubkeys=root_pubkeys[0:1],
            root_threshold=1,
            root_version=root_version,
            key_mgr_pubkeys=key_mgr_pubkeys,
            key_mgr_threshold=1,
        )

        # Wrap the metadata in a signing envelope.
        root_md = cct_signing.wrap_as_signable(root_md)

        root_md_serialized_unsigned = cct_common.canonserialize(root_md)

        root_filepath = self.folder / f"{root_version}.root.json"
        print("Writing out: ", root_filepath)
        # Write unsigned sample root metadata.
        with open(root_filepath, "wb") as fout:
            fout.write(root_md_serialized_unsigned)

        # This overwrites the file with a signed version of the file.
        cct_root_signing.sign_root_metadata_via_gpg(
            root_filepath, root_keys[0]["fingerprint"]
        )

        # Load untrusted signed root metadata.
        signed_root_md = cct_common.load_metadata_from_file(root_filepath)

        cct_authentication.verify_signable(signed_root_md, root_pubkeys, 1, gpg=True)

        print("[reposigner] Root metadata signed & verified!")

    def create_key_mgr(self, keys):
        private_key_key_mgr = cct_common.PrivateKey.from_hex(
            keys["key_mgr"][0]["private"]
        )
        pkg_mgr_pub_keys = [k["public"] for k in keys["pkg_mgr"]]
        key_mgr = cct_metadata_construction.build_delegating_metadata(
            metadata_type="key_mgr",  # 'root' or 'key_mgr'
            delegations={"pkg_mgr": {"pubkeys": pkg_mgr_pub_keys, "threshold": 1}},
            version=1,
            # timestamp   default: now
            # expiration  default: now plus root expiration default duration
        )

        key_mgr = cct_signing.wrap_as_signable(key_mgr)

        # sign dictionary in place
        cct_signing.sign_signable(key_mgr, private_key_key_mgr)

        key_mgr_serialized = cct_common.canonserialize(key_mgr)
        with open(self.folder / "key_mgr.json", "wb") as fobj:
            fobj.write(key_mgr_serialized)

        # let's run a verification
        root_metadata = cct_common.load_metadata_from_file(self.folder / "1.root.json")
        key_mgr_metadata = cct_common.load_metadata_from_file(
            self.folder / "key_mgr.json"
        )

        cct_common.checkformat_signable(root_metadata)

        if "delegations" not in root_metadata["signed"]:
            raise ValueError('Expected "delegations" entry in root metadata.')

        root_delegations = root_metadata["signed"]["delegations"]  # for brevity
        cct_common.checkformat_delegations(root_delegations)
        if "key_mgr" not in root_delegations:
            raise ValueError(
                'Missing expected delegation to "key_mgr" in root metadata.'
            )
        cct_common.checkformat_delegation(root_delegations["key_mgr"])

        # Doing delegation processing.
        cct_authentication.verify_delegation("key_mgr", key_mgr_metadata, root_metadata)

        print("[reposigner] success: key mgr metadata verified based on root metadata.")

        return key_mgr

    def sign_repodata(self, repodata_fn, keys):
        target_folder = self.folder / repodata_fn.parent.name
        if not target_folder.exists():
            target_folder.mkdir()

        final_fn = target_folder / repodata_fn.name
        print("copy", repodata_fn, final_fn)
        shutil.copyfile(repodata_fn, final_fn)

        pkg_mgr_key = keys["pkg_mgr"][0]["private"]
        cct_signing.sign_all_in_repodata(str(final_fn), pkg_mgr_key)
        print(f"[reposigner] Signed {final_fn}")


class ChannelHandler(SimpleHTTPRequestHandler):
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
                return SimpleHTTPRequestHandler.do_GET(self)
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
            SimpleHTTPRequestHandler.do_GET(self)

    def basic_do_GET(self, server_key: str) -> None:
        """Present frontpage with basic user authentication."""
        auth_header = self.headers.get("Authorization", "")

        if not auth_header:
            self.basic_do_AUTHHEAD()
            self.wfile.write(b"no auth header received")
        elif auth_header == "Basic " + server_key:
            SimpleHTTPRequestHandler.do_GET(self)
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
                return SimpleHTTPRequestHandler.do_GET(self)

        self.send_response(403)
        self.send_header("Content-type", "text/html")
        self.end_headers()
        self.wfile.write(b"no valid api key received")


global_parser = argparse.ArgumentParser(
    description="Start a multi-channel conda package server."
)
global_parser.add_argument("-p", "--port", type=int, default=8000, help="Port to use.")

channel_parser = argparse.ArgumentParser(
    description="Start a simple conda package server."
)
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
    "--sign",
    action="store_true",
    help="Sign repodata (note: run generate_gpg_keys.sh before)",
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
    if args.sign:
        if not conda_content_trust_available:
            fatal_error("Conda content trust not installed!")
        args.directory = RepoSigner(args.directory).make_signed_repo()

    # name = args.name if args.name else Path(args.directory).name
    # args.name = name
    channels[args.name] = vars(args)

print(channels)

# Unamed channel in multi-channel case would clash URLs but we want to allow
# a single unamed channel for backward compatibility.
if (len(channels) > 1) and (None in channels):
    fatal_error("Cannot use empty channel name when using multiple channels")

server = HTTPServer(("", PORT), ChannelHandler)
print("Server started at localhost:" + str(PORT))
try:
    server.serve_forever()
except:
    # Catch all sorts of interrupts
    print("Shutting server down")
    server.shutdown()
    print("Server shut down")
