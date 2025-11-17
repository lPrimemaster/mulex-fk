import site
import sys
from pathlib import Path
import shutil
import os
import json


# Write the package additional files for kpm to read at install time
def write_json(cpath, install_loc, package_name):
    output = {}
    output['additional_files'] = []
    for dp, dn, files in os.walk(install_loc):
        for file in files:
            output['additional_files'].append(
                str(Path(os.path.join(dp, file)).resolve())
            )

    with open(f'{cpath}/{package_name}.user.json', 'w') as f:
        json.dump(output, f)


if __name__ == '__main__':
    metadata = json.load(sys.stdin)

    package_name = metadata['package_name']
    cache_path = Path(metadata['cache_path']).resolve()
    cwd = Path(metadata['install_prefix']).resolve()
    install_loc = (Path(site.getusersitepackages()) / 'pymx').resolve()
    install_loc.mkdir(parents=True, exist_ok=True)

    shutil.copytree(cwd / 'pymx', install_loc, dirs_exist_ok=True)

    write_json(cache_path, install_loc, package_name)
