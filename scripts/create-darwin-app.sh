#!/bin/sh

usage () {
	echo $(basename "$0") "<DESTDIR> <libSDL2.dylib>"
}

if ! test -d "$1" || ! test -f "$2"
then
	usage
	exit 1
fi

get_sdl2_rpath () {
	otool -L "$1" |
	grep libSDL2.dylib |
	cut -d' ' -f1 |
	sed -e 's,	,,'  # tab
}

app="$1/Schism Tracker.app"
libsdl2="$2"
macos="$app/Contents/MacOS"
macos_schism="$macos/schismtracker"
macos_libsdl2="$macos/libSDL2.dylib"
current_libsdl2=$(get_sdl2_rpath schismtracker) &&
rsync -vr --delete sys/macosx/Schism_Tracker.app/ "$app/" &&
mkdir -p "$macos" &&
cp schismtracker "$macos_schism" &&
cp "$libsdl2" "$macos_libsdl2" &&
install_name_tool -id libSDL2.dylib "$macos_libsdl2" &&
install_name_tool -change "$current_libsdl2" @executable_path/libSDL2.dylib "$macos_schism"
