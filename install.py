from pathlib import Path


BIN_FOLDER = Path(__file__).parent / "bin"


def install():
    add_in_script_path = Path.home() / ".profile"
    path_to_add = Path(BIN_FOLDER).absolute()
    with open(add_in_script_path, "a") as file:
        file.write(f"\nexport PATH=$PATH:{path_to_add}\n")


if __name__ == "__main__":
    install()
