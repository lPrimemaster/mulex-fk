# Getting Started

## Running the server
Mulex-fk manages all of the backends/frontends via the `mxmain` server service. You can start it with a new test experiment via:
```sh
mxmain -n testexp -l
```
This opens the server with an experiment named testexp and in loopback mode so we don't serve the page on the network.


## Web frontend
If the previous step is successful you can go to your browser and type
```
localhost:8080
```
to land on the frontend home page.

### Home page
This page displays most of the info on currently active backends and connected frontends.
There is a 'log panel' where you will be able to see messages from the connected backends (more on this later),
a status pane where you see the current 'run' and status,
as well as controlling start/stop.
It also has a small widget to display system resources.

### Project page
Next is the project page. This will be empty and is where all of the user pages (plugins as called in the API) will be to control the experiment in question.


### RDB page
The RDB page allows the connected user to view all of the keys currently present in the system. You can use the top search bar to look for a specific entry.
This page also allows for the user to create a new key. Try clicking on a table key entry, the window view will be scrolled down to a pane where all of the current
variable info is. This pane updates in realtime. You can for example look under `/system/metrics/cpu_usage`.
This page also allows to edit/remove existing keys (as long as they're not system keys).

More on the RDB [here]().

### History page
> :warning: This page is still a work in progress.

The history page allows you to look at one/multiple RDB entry/entries and log its/their value in a line plot.

## Creating Backends
