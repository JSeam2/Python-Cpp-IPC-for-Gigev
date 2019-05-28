import subprocess

proc = subprocess.Popen(
    "./cpp/genicam",
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE)

x = 0

while x < 200:
    data = proc.stdout.read(150)
    if data != b'':
        print(data)
    x += 1
    proc.wait()