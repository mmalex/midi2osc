*Midi2Osc*

this is a simple Midi to OpenSoundControl (aka OSC) bridge.
it responds to midi messages on a midi input, and forwards them to OSC messages.
see http://opensoundcontrol.org/ for more information about OSC.

at the moment, it simply forwards all midi messages as a simple 'm' (midi) type OSC message to OSC path `/dreams`

Using
=====
when the program runs, you should see a small window that looks like this:

choose a midi input device from the first drop down. if you don't have a hardware midi input interface,
or you want to route midi from a DAW running on the same computer, you need a midi loopback device.
on mac, you can use OSX's built in IAC driver. 
see https://help.ableton.com/hc/en-us/articles/209774225-Using-virtual-MIDI-buses 
for an example for how to set it up with ableton, but it applies to any software that can output midi.

on windows, you can use a tool like midiox.

clicking on the keyboard also generates midi events. if the midi input is working, you'll see messages at the bottom of the window.

for the OSC output, type the IP address of the device you want to send the OSC messages to.
it should be listening for OSC messages of type 'm' (midi) on OSC path /dreams

I'll leave you to experiment with the other controls

Building
========
I've included pre-built binaries for mac and pc in the /binaries folder

at the time of writing, the code has been compiled & tested on mac & windows.
on mac, just run 'make'. you need to have portmidi and glfw3 installed.
I used https://brew.sh/ to install these dependencies.

on windows, the build is likely currently broken as I haven't had a chance to test it.
I've included a portmidi static lib in the repo as I found it hard to compile from source.

Dependencies
============

this little program uses the wonderful 'Dear Imgui' library by Omar Cornut
https://github.com/ocornut/imgui

it also uses the portmidi library for cross platform midi output
http://portmedia.sourceforge.net/portmidi/

Issues
======
This program is provided without any support by Alex Evans.
It's licensed under the MIT license.
Do what you want with it, but I do not have time to support it.
in other words, use at your own risk!
And to be clear, this is a personal project and is entirely unsupported by my employer. please don't hassle them.
Although it is designed to be compatible with Dreams for the PS4, and that is the reason I wrote it, it is not part of Dreams, not endorsed by Sony or MediaMolecule.
You also don't need Dreams to use it - it is a general 'midi to OSC' bridge, useful anywhere you want to send Midi messages to an OSC device or program. 
I just find it useful to use with dreams, YMMV.

