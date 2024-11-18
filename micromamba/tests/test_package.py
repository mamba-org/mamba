import filecmp
import platform
import shutil
import subprocess
import tarfile
import zipfile
from pathlib import Path

import conda_package_handling.conda_fmt
import pytest
import zstandard
from conda_package_handling import api as cph

from .helpers import *


@pytest.fixture
def cph_test_file():
    return Path(__file__).parent / "data" / "cph_test_data-0.0.1-0.tar.bz2"


def print_diff_files(dcmp):
    for name in dcmp.diff_files:
        print(f"diff_file {name} found in {dcmp.left} and {dcmp.right}")
    for name in dcmp.left_only:
        print(f"file only found in LHS {dcmp.left} / {name}")
    for name in dcmp.right_only:
        print(f"file only found in RHS {dcmp.right} / {name}")

    for sub_dcmp in dcmp.subdirs.values():
        print_diff_files(sub_dcmp)


def test_extract(cph_test_file: Path, tmp_path: Path):
    (tmp_path / "cph").mkdir(parents=True)
    (tmp_path / "mm").mkdir(parents=True)

    shutil.copy(cph_test_file, tmp_path / "mm")
    shutil.copy(cph_test_file, tmp_path / "cph")

    mamba_exe = get_umamba()
    subprocess.call(
        [
            mamba_exe,
            "package",
            "extract",
            str(tmp_path / "mm" / cph_test_file.name),
            str(tmp_path / "mm" / "cph_test_data-0.0.1-0"),
        ]
    )
    cph.extract(
        str(tmp_path / "cph" / cph_test_file.name),
        dest_dir=str(tmp_path / "cph" / "cph_test_data-0.0.1-0"),
    )

    conda = set(
        (p.relative_to(tmp_path / "cph") for p in (tmp_path / "cph").rglob("**/*"))
    )
    mamba = set(
        (p.relative_to(tmp_path / "mm") for p in (tmp_path / "mm").rglob("**/*"))
    )
    assert conda == mamba

    extracted = cph_test_file.name.removesuffix(".tar.bz2")
    fcmp = filecmp.dircmp(tmp_path / "cph" / extracted, tmp_path / "mm" / extracted)
    assert (
        len(fcmp.left_only) == 0
        and len(fcmp.right_only) == 0
        and len(fcmp.diff_files) == 0
    )
    # fcmp.report_full_closure()


def compare_two_tarfiles(tar1, tar2):
    tar1_files = set(tar1.getnames())
    tar2_files = set(tar2.getnames())
    assert tar1_files == tar2_files

    for f in tar1_files:
        m1: tarfile.TarInfo = tar1.getmember(f)
        m2 = tar2.getmember(f)
        if platform.system() != "Windows":
            if not m1.issym():
                assert m1.mode == m2.mode
            else:
                if platform.system() == "Linux":
                    assert m2.mode == 0o777
                else:
                    assert m1.mode == m2.mode
            assert m1.mtime == m2.mtime

        assert m2.uid == 0
        assert m2.gid == 0
        assert m2.uname == ""
        assert m2.gname == ""
        assert m1.size == m2.size

        if m1.isfile():
            assert tar1.extractfile(f).read() == tar2.extractfile(f).read()

        if m1.islnk() or m1.issym():
            assert m1.linkname == m2.linkname


def assert_sorted(l):
    assert l == sorted(l)


def test_extract_compress(cph_test_file: Path, tmp_path: Path):
    (tmp_path / "mm").mkdir(parents=True)

    shutil.copy(cph_test_file, tmp_path / "mm")

    mamba_exe = get_umamba()
    out = tmp_path / "mm" / "out"
    subprocess.call(
        [
            mamba_exe,
            "package",
            "extract",
            str(tmp_path / "mm" / cph_test_file.name),
            str(out),
        ]
    )
    subprocess.call(
        [
            mamba_exe,
            "package",
            "compress",
            str(out),
            str(tmp_path / "mm" / "out.tar.bz2"),
        ]
    )

    compare_two_tarfiles(
        tarfile.open(cph_test_file), tarfile.open(tmp_path / "mm" / "out.tar.bz2")
    )

    fout = tarfile.open(tmp_path / "mm" / "out.tar.bz2")
    names = fout.getnames()
    assert "info/paths.json" in names

    info_files = [f for f in names if f.startswith("info/")]
    # check that info files are at the beginning
    assert names[: len(info_files)] == info_files

    # check that the rest is sorted
    assert_sorted(names[len(info_files) :])
    assert_sorted(names[: len(info_files)])


# Only run this test if zstandard is available.
@pytest.mark.skipif(
    not hasattr(conda_package_handling.conda_fmt, "ZSTD_COMPRESS_LEVEL"),
    reason="zstandard not available (required by conda_package_handling)",
)
def test_transmute(cph_test_file: Path, tmp_path: Path):
    (tmp_path / "cph").mkdir(parents=True)
    (tmp_path / "mm").mkdir(parents=True)

    shutil.copy(cph_test_file, tmp_path)
    shutil.copy(tmp_path / cph_test_file.name, tmp_path / "mm")

    mamba_exe = get_umamba()
    subprocess.call(
        [mamba_exe, "package", "transmute", str(tmp_path / "mm" / cph_test_file.name)]
    )
    failed_files = cph.transmute(
        str(tmp_path / cph_test_file.name), ".conda", out_folder=str(tmp_path / "cph")
    )
    assert len(failed_files) == 0

    as_conda = cph_test_file.name.removesuffix(".tar.bz2") + ".conda"

    cph.extract(str(tmp_path / "cph" / as_conda))
    cph.extract(str(tmp_path / "mm" / as_conda))

    conda = list((tmp_path / "cph").rglob("**/*"))
    mamba = list((tmp_path / "mm").rglob("**/*"))

    fcmp = filecmp.dircmp(
        tmp_path / "cph" / "cph_test_data-0.0.1-0",
        tmp_path / "mm" / "cph_test_data-0.0.1-0",
    )
    assert (
        len(fcmp.left_only) == 0
        and len(fcmp.right_only) == 0
        and len(fcmp.diff_files) == 0
    )
    # fcmp.report_full_closure()

    # extract zipfile
    with zipfile.ZipFile(tmp_path / "mm" / as_conda, "r") as zip_ref:
        l = zip_ref.namelist()

        assert l[2].startswith("info-")
        assert l[0] == "metadata.json"
        assert l[1].startswith("pkg-")

        zip_ref.extractall(tmp_path / "mm" / "zipcontents")

    files = list((tmp_path / "mm" / "zipcontents").glob("**/*"))
    for f in files:
        if f.suffix == ".zst":
            with open(f, mode="rb") as fi:
                dcf = zstandard.ZstdDecompressor().stream_reader(fi)

                with tarfile.open(fileobj=dcf, mode="r|") as z:
                    assert_sorted(z.getnames())
                    members = z.getmembers()
                    for m in members:
                        if f.name.startswith("info-"):
                            assert m.name.startswith("info/")
                        if not f.name.startswith("info-"):
                            assert not m.name.startswith("info/")
                        assert m.uid == 0
                        assert m.gid == 0
