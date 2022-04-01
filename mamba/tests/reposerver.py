import argparse
import base64
import glob
import os
import re
import shutil
from http.server import HTTPServer, SimpleHTTPRequestHandler
from pathlib import Path

try:
    import conda_content_trust.authentication as cct_authentication
    import conda_content_trust.common as cct_common
    import conda_content_trust.metadata_construction as cct_metadata_construction
    import conda_content_trust.root_signing as cct_root_signing
    import conda_content_trust.signing as cct_signing
    conda_content_trust_available = True
except ImportError:
    conda_content_trust_available = False

import rich.console
console = rich.console.Console()

default_user, default_password = os.environ.get("TESTPWD", "user:pass").split(":")

parser = argparse.ArgumentParser(description="Start a simple conda package server.")
parser.add_argument("-p", "--port", type=int, default=8000, help="Port to use.")
parser.add_argument(
    "-d",
    "--directory",
    type=str,
    default=os.getcwd(),
    help="Root directory for serving.",
)
parser.add_argument(
    "-a",
    "--auth",
    default="none",
    type=str,
    help="auth method (none, basic, or token)",
)
parser.add_argument(
    "--sign",
    action="store_true",
    help="Sign repodata (note: run generate_gpg_keys.sh before)",
)
parser.add_argument(
    "--token",
    type=str,
    default="xy-12345678-1234-1234-1234-123456789012",
    help="Use token as API Key",
)
parser.add_argument(
    "--user",
    type=str,
    default=default_user,
    help="Use token as API Key",
)
parser.add_argument(
    "--password",
    type=str,
    default=default_password,
    help="Use token as API Key",
)
args = parser.parse_args()


def get_fingerprint(gpg_output):
    lines = gpg_output.splitlines()
    fpline = lines[1].strip()
    fpline = fpline.replace(" ", "")
    return fpline


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

    def normalize_keys(self, keys):
        out = {}
        for ik, iv in keys.items():
            out[ik] = []
            for el in iv:
                if isinstance(el, str):
                    el = el.lower()
                    print(el)
                    keyval = cct_root_signing.fetch_keyval_from_gpg(el)
                    res = {"fingerprint": el, "public": keyval}
                elif isinstance(el, dict):
                    res = {
                        "private": el["private"].lower(),
                        "public": el["public"].lower(),
                    }
                out[ik].append(res)

        return out

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

        console.print("[green]Root metadata signed & verified!")

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

        console.print(
            "[green]Success: key mgr metadata verified based on root metadata."
        )

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
        console.print(f"[green]Signed [bold]{final_fn}[/bold]")

    def __init__(self, in_folder=args.directory):

        self.keys["root"] = [
            get_fingerprint(os.environ["KEY1"]),
            get_fingerprint(os.environ["KEY2"]),
        ]

        self.in_folder = Path(in_folder).resolve()
        self.folder = self.in_folder.parent / (str(self.in_folder.name) + "_signed")

        if not self.folder.exists():
            os.mkdir(self.folder)

        self.keys = self.normalize_keys(self.keys)
        console.print("Using keys:", self.keys)

        console.print("Using folder:", self.folder)

        self.create_root(self.keys)
        self.create_key_mgr(self.keys)
        for f in glob.glob(str(self.in_folder / "**" / "repodata.json")):
            self.sign_repodata(Path(f), self.keys)


class BasicAuthHandler(SimpleHTTPRequestHandler):
    """Main class to present webpages and authentication."""

    user = args.user
    password = args.password
    key = base64.b64encode(bytes(f"{args.user}:{args.password}", "utf-8")).decode(
        "ascii"
    )

    def do_HEAD(self):
        self.send_response(200)
        self.send_header("Content-type", "text/html")
        self.end_headers()

    def do_AUTHHEAD(self):
        self.send_response(401)
        self.send_header("WWW-Authenticate", 'Basic realm="Test"')
        self.send_header("Content-type", "text/html")
        self.end_headers()

    def do_GET(self):
        """Present frontpage with user authentication."""
        auth_header = self.headers.get("Authorization", "")
        if not auth_header:
            self.do_AUTHHEAD()
            self.wfile.write(b"no auth header received")
            pass
        elif auth_header == "Basic " + self.key:
            SimpleHTTPRequestHandler.do_GET(self)
            pass
        else:
            self.do_AUTHHEAD()
            self.wfile.write(auth_header.encode("ascii"))
            self.wfile.write(b"not authenticated")
            pass


class CondaTokenHandler(SimpleHTTPRequestHandler):
    """Main class to present webpages and authentication."""

    api_key = args.token
    token_pattern = re.compile("^/t/([^/]+?)/")

    def do_GET(self):
        """Present frontpage with user authentication."""
        match = self.token_pattern.search(self.path)
        if match:
            prefix_length = len(match.group(0)) - 1
            new_path = self.path[prefix_length:]
            found_api_key = match.group(1)
            if found_api_key == self.api_key:
                self.path = new_path
                return SimpleHTTPRequestHandler.do_GET(self)

        self.send_response(403)
        self.send_header("Content-type", "text/html")
        self.end_headers()
        self.wfile.write(b"no valid api key received")


if args.sign:
    if not conda_content_trust_available:
        print("Conda content trust not installed!")
        exit(1)
    signer = RepoSigner()
    os.chdir(signer.folder)
else:
    os.chdir(args.directory)

if not args.auth or args.auth == "none":
    handler = SimpleHTTPRequestHandler
elif not args.auth or args.auth == "basic":
    handler = BasicAuthHandler
elif not args.auth or args.auth == "token":
    handler = CondaTokenHandler

PORT = args.port

server = HTTPServer(("", PORT), handler)
print("Server started at localhost:" + str(PORT))
try:
    server.serve_forever()
except:
    # Catch all sorts of interrupts
    print("Shutting server down")
    server.shutdown()
    print("Server shut down")
