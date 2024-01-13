
from datetime import datetime
import os
Import("env")

build_date = datetime.now().strftime('%a %-d %b %Y, %H:%M:%S')  # eg 'Fri 5 Jan 2024, 11:36:19'

git_describe = os.popen(f"cd {env['PROJECT_DIR']}; git describe --tags --always --dirty")
build_version = git_describe.read()
if git_describe.close() != None: # command failed
    build_version = datetime.now().strftime('%Y%m%d:%H%M%S') # eg 20240105:113619

with open( os.path.join( env['PROJECT_INCLUDE_DIR'],  "build.gen.h" ), "w" ) as fp:
  fp.write( "// WARNING: this file is auto-generated\n" )
  fp.write( f"#define XXX_BUILD_DATE \"{build_date}\"\n")
  fp.write( f"#define XXX_BUILD_REPO_VERSION \"{build_version.rstrip()}\"\n")
