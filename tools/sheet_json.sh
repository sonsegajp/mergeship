#!/bin/bash
# JSON list of a test's contact sheets (for sending them).
ls /c/Users/hyper/Mergeship2/build/tests/$1/sheets/*.jpg | sed 's|^/c/|C:/|' | python -c "import sys,json;print(json.dumps([l.strip() for l in sys.stdin]))"
