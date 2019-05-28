# Overview
This project is an alternative approach to interfacing with DALSA Cameras on python. This is a working example of how to use pipes in C++ to communicate with a python program for processing. The camera I used was Genie Nano. The [pygigev](https://pypi.org/project/pygigev/) module is not very reliable when performing realtime imaging. Debugging the pygigev module has proven to be a considerable challenge. Instead of having python process the images, I had decided to use cpp to process the images and then pass the data to a pipe which a python program can then read the data from. To do so, stdio was used.

# Quicktart
1. Ensure that you have properly setup the [Linux GigE-V framework](https://www.teledynedalsa.com/en/products/imaging/vision-software/linux-gige-v-framework/). 

2. Once set up you should be able to run make in the `/cpp` folder
```
$ cd ./cpp
$ make
```

3. Execute the `./cpp/genicam` program. The program will create a file socket which you can use a python script to read from
```
# if you are in the ./cpp folder

$ ./genicam
```

4. To test the python reader
```
$ cd ..

# if you are back to the ./ folder
$ python reader.py
```

5. Once done you can clean up the files if necessary
```
# if you are in the ./cpp folder

$ make clean
```

# CPP Program Settings
1. `TURBO_DRIVE` This refers to the Turbo Drive mode on DALSA cameras [more info](https://www.teledynedalsa.com/en/learn/knowledge-center/turbodrive/) *Value is set to 0 (ie. off) by default*
2. `DISPLAY_WINDOW` This refers to the window that is being spawned when `./genicam` is being run. When set to 1, the video window will popup. When set to 0, the video window will be hidden. *Value is set to 0 (ie. hidden) by default*