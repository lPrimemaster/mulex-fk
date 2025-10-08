# Brief  : Detect the current platform so we gather the correct shared object
# Author : César Godinho
# Date   : 26/09/2025

import platform
from pathlib import Path
from ._exceptions import UnsupportedSystemError
from ._logger import logger
from typing import Tuple, Union
import os


def _detect_platform() -> Tuple[str, str]:
    system = platform.system().lower()
    arch = platform.machine().lower()
    logger.debug(f"Detected platform: {system}.{arch}")
    return (system, arch)


def _detect_lib_filename() -> str:
    system, arch = _detect_platform()

    if system == "windows":
        return "MxCapi.dll"
    elif system == "linux":
        return "libMxCapi.so"

    # macOS and others not supported
    raise UnsupportedSystemError(f"Unsupported platform {system}.{arch}")


def _detect_dev_mode() -> bool:
    try:
        project_dir = Path(__file__).parents[1]
        return Path(project_dir / ".git").exists()
    except IndexError:
        return False


def get_lib_location() -> Union[str, None]:
    """ Find the location of the pymx C API. """

    try:
        filename = _detect_lib_filename()
    except UnsupportedSystemError:
        logger.error("Unsupported platform")
        return None

    # NOTE: (Cesar) First check if we are running in dev mode
    #               Just check if there is a git repository in the top level
    if _detect_dev_mode():
        candidates = [
            "build",
            "Build",
            "build/capi",
            "Build/capi"
        ]

        for candidate in candidates:
            path = Path(__file__).parents[1] / candidate / filename
            if path.exists():
                logger.debug(f"Candidate {path} - FOUND")
                return str(path)
            logger.debug(f"Candidate {path} - NOT FOUND")

        logger.error("Library file not found.")
        return None

    # NOTE: (César) We are not on dev mode try and find the lib on PATH
    system, _ = _detect_platform()
    try:
        if system == "linux":
            penv = os.environ['PATH'].split(':')
        elif system == "windows":
            penv = os.environ['PATH'].split(';')
    except KeyError:
        logger.error("Could not find the PATH environment variable.")
        return None

    candidates = [
        "",
        "lib"
    ]

    for dir in penv:
        for candidate in candidates:
            path = Path(dir) / candidate / filename
            logger.debug(f"Candidate {path} - ", end="")
            if path.exists():
                logger.debug("FOUND")
                return str(path)
            logger.debug("NOT FOUND")

    logger.error(f"Failed to find {filename}. Is it on PATH?")
    return None
