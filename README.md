# gstrecsign

Simple Gstreamer REC sign for adding blinking red circle to a pipeline

Example (clickable):

<a href="http://www.youtube.com/watch?feature=player_embedded&v=WTQbfZoJDUQ
" target="_blank"><img src="http://img.youtube.com/vi/WTQbfZoJDUQ/0.jpg" 
alt="recsign" width="240" height="180" border="10" /></a>


Building with `GCC` under Ubuntu 16.04:
```
gcc -Wall -fPIC gstrecsign.c -o libgstrecsign.so --shared $(pkg-config --cflags --libs gstreamer-1.0)
```

Dependencies:
```
sudo apt-get update
sudo apt-get upgrade
sudo apt-get install gstreamer-1.0 gstreamer1.0-tools
```

Installing:

Copy `libgstrecsign.so` to your gstreamer `lib` dir, e.g.
```
cp libgstrecsign.so /usr/lib/x86_64-linux-gnu/gstreamer-1.0/
```
Example pipeline:
```
gst-launch-1.0 videotestsrc ! video/x-raw, width=1280, height=720 ! recsign ! autovideosink
```
