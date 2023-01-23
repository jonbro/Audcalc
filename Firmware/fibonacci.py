last = 0
current = 1

def next():
    global last
    global current
    new = last + current
    last = current
    current = new

for x in range(10000000):
    next()
    # print(str(x+1) + ": " + str(current) + "\n")

print(current)