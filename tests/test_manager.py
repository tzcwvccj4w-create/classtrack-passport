import importlib.util
import pathlib
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("manager", ROOT / "tools" / "classtrack_manager.py")
manager = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(manager)


def main():
    with tempfile.TemporaryDirectory() as directory:
        path = pathlib.Path(directory) / "classes.csv"
        path.write_text("id,name,course\n0,26机电2班,高职英语\n", encoding="utf-8")
        raw = manager.validate_csv(path)
        assert raw.startswith(b"id,name,course")
        path.write_text("id,name,course\n12,A,B\n", encoding="utf-8")
        try:
            manager.validate_csv(path)
        except ValueError:
            pass
        else:
            raise AssertionError("out-of-range ID accepted")
    print("PASS: ClassTrack Manager CSV validation")


if __name__ == "__main__":
    main()
