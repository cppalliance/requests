
url = "https://publicsuffix.org/list/public_suffix_list.dat"

import requests
import urllib.parse
import sys

dat = requests.get(url).text

lines = ''
lnn = [ln for ln in  dat.splitlines() if ln and ln != '\n' and not ln.startswith("//")]

fm = [ln for ln in lnn if not ln.startswith("!") and ln != '*']
wl = [ln for ln in lnn if ln.startswith("!")]
wc = [ln for ln in lnn if ln.startswith('*.')]

with open(sys.argv[1], "w") as f:
    f.write(
        f"""
    // full matches
    {{
         {' ,'.join(f'"{f}"' for f in fm)}
    }},
    {{
        {' ,'.join(f'"{f}"' for f in wl)}
    }},
    {{
        {' ,'.join(f'"{f}"' for f in wc)}
    }}
""")
