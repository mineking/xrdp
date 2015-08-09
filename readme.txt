
Quick fixes for xrdp 0.9.0
- some code cleanup
- a lot of new badly written code
- RDP message parsing updated 
- a popup to resume or kill a previous session

Installation : 
change to the xrdp directory and run ./bootstrap ./configure make sudo make install

Make sure that these files are in /usr/local/sbin/  :
X11rdp  (can be a symlink X11rdp -> /opt/X11rdp/bin/X11rdp)
xrdp  
xrdp-chansrv  
xrdp-sesman  
xrdp-sessvc

G.
