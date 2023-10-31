import pathlib
import pickle

from mitmproxy import ctx, http
from mitmproxy.tools.main import mitmdump


class ResponseCapture:
    def __init__(self):
        self.cache_file = None
        self.response_dict = {}

    def load(self, loader):
        loader.add_option(
            name="cache_dir",
            typespec=str,
            default="",
            help="Path to load/store request cache",
        )

    def configure(self, updates):
        if "cache_dir" in updates:
            self.cache_file = pathlib.Path(ctx.options.cache_dir) / "requests.pkl"
            if self.cache_file.exists():
                with open(self.cache_file, "rb") as f:
                    self.response_dict = pickle.load(f)

    def done(self):
        if self.cache_file is not None:
            self.cache_file.parent.mkdir(parents=True, exist_ok=True)
            with open(self.cache_file, "wb") as f:
                pickle.dump(self.response_dict, f)

    def request(self, flow: http.HTTPFlow) -> None:
        if (url := flow.request.url) in self.response_dict:
            flow.response = self.response_dict[url]

    def response(self, flow: http.HTTPFlow) -> None:
        self.response_dict[flow.request.url] = flow.response


addons = [ResponseCapture()]

if __name__ == "__main__":
    print("Starting mitmproxy...")
    mitmdump(["-s", __file__])
