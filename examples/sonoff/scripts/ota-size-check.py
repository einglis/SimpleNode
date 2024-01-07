
import os
import SCons.Warnings
Import("env")

"""
This has been quite the voyage...  In principle, PlatformIO will check the final size for you,
but it appears to overwrite the 'board_upload.maximum_size' value - even if specified
in platform.ini - with a value extracted by regex magic from the 'board_build.ldscript'.
But that replacement happens post-build, when this script is already long gone.

The same code that does the magic also reads the true binary size using xtensa-lx106-elf-size
but that gave me confusing numbers, so I've opted to read the file size directly, which
gives a good, though slightly higher estimate.

And because it's late, I just hard-coded the limit to be half of what I know the actual
flash space reported by the ldscript to be.  I'm not sure I have clean access to it here.
"""

def post_program_action(source, target, env):

  file_size = os.path.getsize( target[0].get_abspath() )

  if file_size > 958448//2: # oh the humanity!
    raise SCons.Warnings.SConsWarning("WARNING: Image may be too large for OTA update")

env.AddPostAction( "$BUILD_DIR/${PROGNAME}.bin", post_program_action )
