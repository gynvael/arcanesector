@echo off
make -f Makefile.win32
if not %errorlevel%  == 0 goto :eof

:: md5('alamakota').hexdigest()
set ARCANE_PASSWD=49aa66843380c377e93b198b966eb699
set ARCANE_HOST=127.0.0.1:5001
client.exe 2

::start framedump.png


