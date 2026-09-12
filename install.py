from pathlib import Path
import os


BIN_FOLDER = Path(__file__).parent / "bin"


def get_user_shell_name() -> str | None:
    if "SHELL" in os.environ:
        return Path(os.environ["SHELL"]).parts[-1]
    return None


def add_to_file(script_path: Path):
    if not script_path.exists():
        return
    path_to_add = Path(BIN_FOLDER).absolute()
    with open(script_path, "a") as file:
        file.write(f"\nexport PATH=$PATH:{path_to_add}\n")


def install():
    add_to_file(Path.home() / ".profile")

    shell = get_user_shell_name() or "bash"
    add_to_file(Path.home() / f".{shell}rc")


if __name__ == "__main__":
    install()
