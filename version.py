import subprocess
from datetime import datetime

Import("env")

def git_cmd(args, default=""):
    try:
        out = subprocess.check_output(
            ["git"] + args,
            stderr=subprocess.DEVNULL
        )
        return out.decode().strip()
    except Exception:
        return default

tag = git_cmd(["describe", "--tags", "--abbrev=0"], default="v0.0.0")

commit = git_cmd(["rev-parse", "--short", "HEAD"], default="nogit")

status = git_cmd(["status", "--porcelain"], default="")
dirty = "-dirty" if status else ""

date = datetime.now().strftime("%Y-%m-%d")

version_full = f"{tag}-{date}-g{commit}{dirty}"

print("Firmware VERSION:", version_full)

try:
    stringify = env.StringifyMacro
except AttributeError:
    def stringify(v: str) -> str:
        return '"' + v.replace('"', '\\"') + '"'

env.Append(
    CPPDEFINES=[
        ("VERSION",        stringify(version_full)),
        ("VERSION_TAG",    stringify(tag)),
        ("VERSION_COMMIT", stringify(commit)),
        ("VERSION_DATE",   stringify(date)),
    ]
)