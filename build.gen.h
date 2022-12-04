
// This file will be auto-(re-)generated on build.
// But Arduino needs it to be there in the first place, hence its presence in the repo.

#define XXX_BUILD_DATE "Placeholder date"
#define XXX_BUILD_REPO_VERSION "Placeholder version"


// One-time setup instructions:

// Create `platform.local.txt` in the same folder as `platform.txt`,
//  for example, in `/home/edd/.arduino15/packages/esp8266/hardware/esp8266/2.7.4/`.
/*
    recipe.hooks.sketch.prebuild.1.pattern=make-build.gen.h.sh "{build.source.path}" "{build.path}"
*/

// Create `make-build-gen.h.sh` and place it somewhere on your PATH,
//  for example, in `/home/edd/bin/` or `/usr/local/bin/`.
/*
    #!/bin/bash

    echo Generating header \"build.gen.h\"

    # Go to the source directory.
    [ -n "$1" ] && cd "$1" || exit 1

    # Build a version string with git.
    version=$(git describe --tags --always --dirty 2> /dev/null)

    # If this is not a git repository, fallback to the compilation date.
    [ -n "$version" ] || version=$(date -I)

    # Save this in build.gen.h.
    echo "#define XXX_BUILD_DATE \"`date`\"" > $2/sketch/build.gen.h
    echo "#define XXX_BUILD_REPO_VERSION \"$version\"" >> $2/sketch/build.gen.h
*/
