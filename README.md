# CD-i Tile Scroller example

This is a vibe coded example of a "corkscrew scroller" which is typically
used in horizontally scrolling Amiga games.

All of the code is written by GPT-6.1 Terra with heavy steering into
the right direction. As the developer of `Tiny Little Slug`, I know how it should work
and the coding agent was instructed to be my typist, making this project work within only
2 hours of coding. The assembler routines, I haven't really looked at, but pure C was not possible
to get the performance right.

I just wanted to know if this scrolling technique would work as well on the CD-i.

The base requirement for making this happen, is the capability of the video controller
to move the framebuffer pointer during the horizontal blank.
The Amiga can do this via the Copper, the CD-i can do it via the DCA / LCT.

I do have to admit that the Amiga is more powerful here, because
the framebuffer is allowed to be larger than the view. This is a feature,
the MCD212 seems to lack, requiring to set the pointer manually and not automatic,
like Agnus can do with the modulo registers `BPL1MOD` and `BPL2MOD`.

One plane is scrolling and columns of hidden tiles are drawn.
To make it slightly more interesting, there are some objects jumping around.

To even spice it up more, there is some Level A ADPCM music from disc.
In this case, it is `Surasshu - The Penis (Eek!)`, a fitting composition for
a sample like this.

## How to build

    ./make_image.sh

## References

This is source code which explains the trick in more depth for the Amiga.
They even added illustrated documentations about every variant they could think off.

https://aminet.net/package/dev/src/ScrollingTrick

## Disclaimer

I'm not sure about the copyright of the song.
If this is problematic I will filter it from the Git History.


