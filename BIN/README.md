# Server information

## PrivateMessenger.exe

A Window x64 cgi-bin program, the server component which manages client ids, saves and distributes pending messages on per requests from the clients.

This component must be placed in the cgi-bin directory of the http server

All source code for this component is provided in this repository.

## modify_guid_template.code

Template code for generating source code. 

This component must be placed in a directory which is relative to the cgi-bin directory:

..\PrivateMessenger\Generated

For instance, if the cgi bin directory is: 

C:\Apache2.4\cgi-bin

then the Generated directory will be:

C:\Apache2.4\PrivateMessenger\Generated

You must set the permissions on the PrivateMessenger directory and all the directories under it so that the CGI component has read and write permission.

## Compiler.exe

Windows x64 program which compiles source code to generate protected byte code which will be sent to the client.

This component must be placed in the Generated directory, see above.

Source code for this .exe is not provided.

## Backup directory

In order to provide a location where the server may back up the database where Client ID and keys are stored, please create a backup directory which is a sibling with the Generated directory:

e.g. 

C:\Apache2.4\PrivateMessenger\Backup

## Database files

The server manages two files:

..\PrivateMessenger\DB.bin - contains the hashed 32 byte ID for each client and a server generated 16 byte keys

..\PrivateMessenger\MSG.bin - contains pending text messages (sent but not yet received) 




