Commander
=========

"Commander" is an XScreenSaver module which draws spaceships from the
1980s computer game "Elite".

Some assembly of these components may be required, but the `doit`
script should get you most of the way through the build process.
Implementation notes and to-do list are in notes.txt and a reasonably
comprehensive manual page is provided.

The ship data in `commander.h` was produced (via `shape_to_h`) from
the original Elite data. The source code is available from
http://www.iancgbell.clara.net/elite/bbc/ and contains
"(C)Bell/Braben1984" but no licence is given, and my emails to them
asking about licensing have sadly gone unanswered.

Many thanks to Jamie Zawinksi for XScreenSaver, parts of which are
(re)used here; also to Vasek Potocek, the author of cubicgrid.c which
was a huge help while I was cobbling this together.

Dave Holland <dave@biff.org.uk>
