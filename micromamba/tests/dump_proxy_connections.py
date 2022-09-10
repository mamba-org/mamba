"""
mitmproxy connection dumper plugin

This script shouldn't be run or imported directly. Instead, it should be passed to mitmproxy as a script (-s).
It will then dump all request urls in the file specified by the outfile option.

We use this script instead of letting mitmdump do the dumping, because we only care about the urls, while mitmdump
also dumps all message content.
"""

from mitmproxy import ctx
from mitmproxy.addonmanager import Loader
from mitmproxy.http import HTTPFlow


class DumpAddon:
    def load(self, loader: Loader):
        loader.add_option(
            name="outfile",
            typespec=str,
            default="",
            help="Path for the file in which to dump the requests",
        )

    def request(self, flow: HTTPFlow):
        with open(ctx.options.outfile, "a+") as f:
            f.write(flow.request.url + "\n")


addons = [DumpAddon()]
