import os
import sys

import libmambapy
import mypy.stubgen
import setuptools
import setuptools.command.build_py


class build_py(setuptools.command.build_py.build_py):
    def run(self):
        """Generate stub files."""
        options = mypy.stubgen.Options(
            pyversion=sys.version_info[:2],
            no_import=False,
            inspect=True,
            doc_dir="",
            search_path=[],
            interpreter=sys.executable,
            parse_only=False,
            ignore_errors=False,
            include_private=False,
            output_dir=self.build_lib,
            modules=[],
            packages=["libmambapy", "libmambapy.bindings"],
            files=[],
            verbose=False,
            quiet=False,
            export_less=False,
            include_docstrings=False,
        )
        mypy.stubgen.generate_stubs(options)
        os.rename(
            src=os.path.join(self.build_lib, "libmambapy"),
            dst=os.path.join(self.build_lib, "libmambapy-stubs"),
        )

        super().run()


setuptools.setup(
    name="libmambapy-stubs",
    version=libmambapy.__version__,
    install_requires=[f"libmambapy=={libmambapy.__version__}"],
    packages=["src"],
    package_data={"libmambapy-stubs": ["**/*.pyi"]},
    cmdclass={"build_py": build_py},
)
