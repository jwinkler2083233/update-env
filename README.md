# update-env
Live update to the environment variables of any Windows process

This project builds on a code snippet I found.  If you run this in Administrator mode, you can do live realtime changes to environment variables in other processes.

Usage:

`UpdateEnvironment.exe <process id> <exact variable name> <new value>`
  
  
That's it.  This is a console program, and will return with a nonzero value in ERRORLEVEL if it didn't succeed for any reason.  It's written to run on 64-bit x86 chipsets.  I wrote this for the VS 2022 compiler, running Windows 10.
