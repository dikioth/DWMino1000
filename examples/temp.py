import os


for dirname in os.listdir('.'):
    path = os.path.join('.', dirname)
    for item in os.listdir(path):
        if item.endswith(".c"):
            os.rename(os.path.join(path, item), os.path.join(path, path + '.ino'))
