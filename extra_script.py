import os
import json

if not os.path.exists("testmode.h"):
    with open("testmode.h", 'w') as f:
        f.write("/* TEST MODE parameters */\n")
        f.write("#define TESTMODE_SSID \"ostest\"\n")
        f.write("#define TESTMODE_PASS \"opendoor\"\n")

data = {}

if not os.path.exists("extra_env.json"):
    with open("extra_env.json", 'w') as f:
        data = {
            "upload": {
                "url": "",
                "pw": ""
            }
        }
        json.dump(data, f, indent=2)
else:
    with open("extra_env.json", "r") as f:
        data = json.load(f)

Import("env")

if data["upload"]["url"] != "" and data["upload"]["pw"] != "":
    env.Replace(
        UPLOAD_PROTOCOL="custom",
        UPLOAD_FLAGS=[],
        UPLOAD_COMMAND="curl -X POST {url} -F \"file=@$SOURCE\" -F \"pw={pw}\"".format(
            url=data["upload"]["url"],
            pw=data["upload"]["pw"]
        )
    )
