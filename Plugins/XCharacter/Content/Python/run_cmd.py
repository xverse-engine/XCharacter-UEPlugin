import os
import shutil

print("Start running cmd...")

cwd = os.getcwd().replace("\\", "/")

print("Current working directory:")

shutil.rmtree(cwd + "/output", ignore_errors=True)
os.makedirs(cwd + "/output")

# generate mp4 throuth blender
for file in os.listdir("input"):
    if file.endswith("json"):
        json_path = cwd + "/input/" + file
        output_path = cwd + "/output/" + file[:-5] + "_to_arkit.json"

        cmd = f"python python_scripts/mhc_to_arkit.py {json_path} {output_path}"
        os.system(cmd)


exit(0)
