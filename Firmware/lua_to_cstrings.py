import sys

text = ""
with open(sys.argv[1] + '/globals.lua','r') as readfile:
    for line in readfile:
        text += "\"{line}\"\n".format(line = line.rstrip())

text = "const char* global_lua ="+text.rstrip()+";"
f = open("globals.lua.h", "w")
f.write(text)
f.close()
