# gstrecsign

Simple Gstreamer REC sign for adding blinking red circle to a pipeline

Example (clickable):

<a href="http://www.youtube.com/watch?feature=player_embedded&v=WTQbfZoJDUQ
" target="_blank"><img src="http://img.youtube.com/vi/WTQbfZoJDUQ/0.jpg" 
alt="recsign" width="240" height="180" border="10" /></a>


Dependencies:
```
sudo apt-get update
sudo apt-get upgrade
sudo apt-get install build-essential gstreamer-1.0 gstreamer1.0-tools libgstreamer1.0-dev
```

Building:
```
make
```

Installing (needs sudo):
```
sudo make install
```

Example pipeline:
```
gst-launch-1.0 videotestsrc ! video/x-raw, width=1280, height=720 ! recsign ! autovideosink
```
