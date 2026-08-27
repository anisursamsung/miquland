# biway
Wayland based tiling window manager build in C++.

**Theming**
Have own theme engine: Core cocept in Android way. Comes with build in dark and light mode with two default wallpapers and color set set. You can define wallpaper for each one and color scheme for each one. If you look at the directory ./config/biway.conf there we point to theme/theme_mode.conf which points to either light scheme or dark scheme. If you want same wallpaper set wallpaper variable at biway.conf or if you want different wallpaper for each then set wallpaper variable inside dark.conf or light.conf. Basically it is just smart pointing by source = ..... That gives you total freedom to make own theme swithcer script. It is up to user how they use source =... to make life simple. e.g. someone may like sharp vs rounded setup and put window_corner_radius = 0 for sharp file and 20 for rounded and point smartly to those and theme switcher app just changes the pointed file. 

**Customization**
There are many. Check the config file and you will get the idea. Keybidning, window bordering etc.

**Tiling layout**
Biway is highly opinionated in this segment. We let only two windows maximum visible (i.e. 2 windows per workspace). Third one auomatically goes to next workspace. If single window is opened, it takes full screen.  

***Important**
We live listen to configuration changes in ~/.config/biway/ and ~/.config/biway/theme/ only. Any other folder or subfolder are not live listened.

***What we use***
Wayland, wlroots major backbone.

***AI Use**
Completely vibe coded. But, yes 30% of the writings are by me and direction is by me. It is not like that you ask AI to make a Window Manager and it will make one for you.

***GTK and KDE Apps support**
GTK apps are having issues with popup for now. Lets see if I can fix it.

***App developement**
It has two native sampe apps working. Menu and Topbar. 

***Copy paste Issue***
Get wl-clipbard and wl-clip-persist-standard (get the name correct from web). let clip-perists run in background/another terminal/autostarted.

***Notification****
Works but you need to have one of mako, dunst, etc and make sure they are running in background/another terminal/autostarted.

***Shell***
Default companion parts?
A menu and a bar is given by default. You can hide bar by Super B as can be seen in the config file or simply run "togglebar".

***Reserved key bidning***
Super T for terminal, Super Shift Q to exit biway is reserved for system.


***Wish me luck***
Goal is to make a basic fully functional window manager which works out of box. So, many defaults will be set but will be changable. E.g. The menu will be set but will be changeable e.g. to Rofi easily. Make few native apps with the native toolkit we made. Ultimate goal is to make customizable disto.




