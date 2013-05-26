#!/bin/bash
convert shadow.png -resize 48x48 src/main/res/drawable-mdpi/ic_launcher.png
convert shadow.png -resize 72x72 src/main/res/drawable-hdpi/ic_launcher.png
convert shadow.png -resize 96x96 src/main/res/drawable-xhdpi/ic_launcher.png
convert shadow.png -resize 144x144 src/main/res/drawable-xxhdpi/ic_launcher.png
