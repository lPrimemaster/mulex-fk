import site
import sys
from pathlib import Path
import shutil
import os
import json


# Write the package additional files for kpm to read at install time
def generate_output(cpath, install_loc, package_name):
    output = {}
    output['additional_files'] = []
    for dp, dn, files in os.walk(install_loc):
        for file in files:
            output['additional_files'].append(
                str(Path(os.path.join(dp, file)).resolve())
            )
    return output


def post_install(package_name, cache_path, install_prefix):
    cache_path = Path(cache_path).resolve()
    cwd = Path(install_prefix).resolve()
    install_loc = (Path(site.getusersitepackages()) / 'pymx').resolve()
    install_loc.mkdir(parents=True, exist_ok=True)

    shutil.copytree(cwd / 'pymx', install_loc, dirs_exist_ok=True)

    return generate_output(cache_path, install_loc, package_name)
