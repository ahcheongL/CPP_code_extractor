import os


def add_build_dir_to_path():
    build_dir = os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build"
    )
    if build_dir not in os.environ["PATH"]:
        os.environ["PATH"] += ":" + build_dir


def remove_file(path):
    try:
        os.remove(path)
    except OSError as e:
        print(f"Error removing file {path}: {e.strerror}")

    return
