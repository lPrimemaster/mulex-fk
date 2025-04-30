## Summary

Plugins are standalone javascript/typescript modules that contain GUI's made by the user to control/display available data.

From these pages, the use as access to the typescript mulex API and can control:

- RDB variables
- PDB tables
- Subscribe to events
- Plot data via 'uplot' wrapper
- Create control interfaces with buttons/comboboxes/value selections/switches/etc...
- Make RPC calls to the server
- Make user RPC calls to the user's backends
- Write any compliant typescript code to perform whatever task necessary

A simple way to get started is to look under the demo plugin shipped with mxplug.
To do so, simply run `mxplug --new` on an empty directory and inspect the `src/entry/plugin.tsx` entry file.

The API specification for each topic above is explained under each section on the sidebar and will not be present here.
This page focus more on the layout of the plugin directory and how to build/create plugins.

## Project

A plugin is just a typical solid-js project. To learn more about the solid-js framework view [here](https://docs.solidjs.com).
Here is a brief description of all the files under the plugin project directory:

```bash
├── lib/                    # Contains lib files, typically not used
├── assets/                 # Contains asset files
├── src/
│   ├── lib/                # Contains mx lib files, typically not used
│   ├── api/                # Contains mx api files, typically not used
│   ├── components/         # Contains mx component files
│   └── entry/              # Contains the plugin entry point files
│       ├── plugin1.tsx     # These need to be here for
│       ├── plugin2.tsx     # mxplug to know how to build
│       └── ...
├── package.json            # node project package file
├── tsconfig.json           # typescript project specifications
├── vite.config.ts          # vite build specifications
└── globals.d.ts            # global variables that don't need 'import'
```

In reality this is simply a typescript project that gets compiled via vite to a working javascript file that gets imported by the mx server.
The only difference being the project tree is already cooked to work with our solid-js framework and mx ts API.

All the `.tsx` files under `src/entry/` will be treated as input files for building standalone plugins. So each file here represents one page/plugin
on the server project page.
The location of your other files is not mandatory. They can be placed anywhere. However, feel free to follow the already existing structure.
You should also modify the project metadata under the `package.json` file.

### Building
To build the project simply type `mxplug --build <expname>` on the root dir (where `package.json` is) with `<expname>` as you experiment running on the mx server.
Building uses built in yarn. This is currently not changable so if you wish to use your own manager refer to the [getting started page](getting_started.md#your-first-plugin).

To use "dev" mode (i.e. build when a source file is modified) you can run `mxplug` in hotswap mode via `mxplug --build <expname> --hotswap`.
This will make the plugin build binary run indefinitely and check for file changes under the specified working directory.

## API Elements

All the API components are displayed in the demo `plugin.tsx` file and are pretty simple to use. Head there to check out how to use them.
