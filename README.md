# IdleMiner
Run a command whenever the computer is locked. My main use case for this is mining Bitcoin/Ethereum on my GPU when it's not in use. Works with Windows; [there are even easier methods for Linux](https://unix.stackexchange.com/questions/28181/run-script-on-screen-lock-unlock).

## Usage

Simply put the command you want to run after the IdleMiner executable on the command line. For instance, if you wanted to run `mine-bitcoins.exe --how-many=allofthem` whenever the screen is locked, run this command:

    IdleMiner.exe mine-bitcoins.exe --how-many=allofthem
    
IdleMiner starts the program 60 seconds after locking the computer so if you lock it for a very short time or by accident it won't immediately become less responsive. It sends a `SIGINT` (`Ctrl-C`) to the process as soon as the computer unlocks. If the process doesn't stop itself 5 seconds afterwards, it will force-terminate it.
