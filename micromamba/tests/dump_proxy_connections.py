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
