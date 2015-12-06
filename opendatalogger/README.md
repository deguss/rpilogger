(detailed version: [http://opendatalogger.com](http://opendatalogger.com))

Installation instructions 
===

### how to compile from source

1. `cd ~/opendatalogger`
2. Edit the Makefile according to your needs. Set up how many ADCs you are using and config the channels. 
3. `make` and check for warnings or errors.
3. Verify if the program behaves normally by executing `build/ads --debug`

So far the program is not installed. If you wish to run it automatically the system boots, install it.

### installation

1. `cd ~/opendatalogger`
2. `sudo make install`
3. Watch for possible errors, if something went wrong, use `sudo make uninstall` to undo it.
4. To start it immediatelly execute `sudo /etc/init.d/opendatalogger start` or it will be started the next time you reboot.

### start / stop

After installation you have to use the system.d interface to start/stop/restart the sampling program ads. Accordingly

- `sudo /etc/init.d/opendatalogger start` or
- `sudo /etc/init.d/opendatalogger stop` or
- `sudo /etc/init.d/opendatalogger restart`.


### check

You can verify the program is running by typing `pstree`. It produces a process tree, where usually at the very top `ads` with 1 thread (in brackets) can be seen. You can also use `ps aux`, `htop` or the most elegant way:

`sudo /etc/init.d/opendatalogger status`



### uninstalling (permanent remove)

1. cd `~/opendatalogger`
2. `sudo make uninstall`

That's it!
We appreciate any feedback, post to the [github issue pages](https://github.com/deguss/rpilogger/issues) please.

Detailed instructions and expanation is available on the project's website. 
[http://opendatalogger.com](http://opendatalogger.com)

