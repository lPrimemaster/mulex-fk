# Brief  : Detect the current platform so we gather the correct shared object
# Author : César Godinho
# Date   : 26/09/2025

import platform
from pathlib import Path
from ._exceptions import UnsupportedSystemError
from ._logger import logger


def _detect_platform():
    system = platform.system().lower()
    arch = platform.machine().lower()
    logger.debug(f"Detected platform: {system}.{arch}")
    return (system, arch)


def _detect_lib_filename():
    system, arch = _detect_platform()

    if system == "windows":
        return "pymx.dll"
    elif system == "linux":
        return "libpymx.so"

    # macOS and others not supported
    raise UnsupportedSystemError(f"Unsupported platform {system}.{arch}")


def _detect_dev_mode() -> bool:
    try:
        project_dir = Path(__file__).parents[1]
        return Path(project_dir / ".git").exists()
    except IndexError:
        return False


def get_lib_location():
    """ Find the location of the pymx library. """

    try:
        filename = _detect_lib_filename()
    except UnsupportedSystemError:
        logger.error("Unsupported platform")
        return None

    # NOTE: (Cesar) First check if we are running in dev mode
    #               Just check if there is a git repository in the top level
    dev_mode = _detect_dev_mode()

    if dev_mode:
        pass
