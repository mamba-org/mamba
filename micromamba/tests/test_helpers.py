from . import helpers


def test_target_platform_args(monkeypatch):
    monkeypatch.setattr(helpers, "test_target_platform", "win-64")

    assert helpers.package_target_args(("xtensor",)) == ["--platform", "win-64"]
    assert helpers.package_target_args(("xtensor",), enabled=False) == []
    assert helpers.package_target_args(("--platform", "win-arm64")) == []
    assert helpers.package_target_args(("--platform=win-arm64",)) == []
    assert helpers.package_target_args(("--clone", "source")) == [
        "--platform",
        "win-64",
    ]
    assert helpers.package_target_args(("--clone=source",)) == ["--platform", "win-64"]


def test_package_commands_apply_target_platform(monkeypatch):
    commands = []

    monkeypatch.setattr(helpers, "test_target_platform", "win-64")
    monkeypatch.setattr(helpers, "get_umamba", lambda *args, **kwargs: "mamba")
    monkeypatch.setattr(
        helpers,
        "subprocess_run",
        lambda *args, **kwargs: commands.append(args) or b"",
    )

    helpers.create("xtensor", always_yes=False, default_channel=False, no_rc=False)
    helpers.install("xtensor", default_channel=False, no_rc=False)
    helpers.update("xtensor", default_channel=False, no_rc=False)
    helpers.umamba_repoquery("search", "xtensor", no_rc=False)
    helpers.run_env("create", "--file", "environment.yml")
    helpers.run_env("update", "--file", "environment.yml")
    helpers.run_env("export")

    for command in commands[:-1]:
        platform_index = command.index("--platform")
        assert command[platform_index + 1] == "win-64"
    assert "--platform" not in commands[-1]
