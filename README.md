# Overview
This project is an alternative approach to interfacing with DALSA Cameras on python. The initial approach to use GigE-V api was to utilize Cython wrappers, the project that does this is found in [pygigev](https://pypi.org/project/pygigev/). Cython introduces complexities which can make debugging a challenge. The problem I faced was with saving a realtime stream to image files, doing this for extended periods led to segfaults and I couldn't seem to find the bug with the pygigev code.

The current approach is to use pipes in C++ to communicate with a python program for processing. The camera I used was Genie Nano. Instead of having python process the images, I had decided to use cpp to process the images and then pass the data to a pipe which a python program can then read the data from. To do so, stdio was used.

# Quickstart
1. Ensure that you have properly setup the [Linux GigE-V framework](https://www.teledynedalsa.com/en/products/imaging/vision-software/linux-gige-v-framework/). 

2. Once set up you should be able to run make in the `/cpp` folder
```
$ cd ./cpp
$ make
```

3. Execute the `./cpp/genicam` program to see if it runs. Double check on the camera for any lighting changes. On Genie Nano the light will turn from a constant blue light to a flashing green light. The program will create a file socket which you can use a python script to read from. If you would wish to view the display window as a sanity check to see if the camera is working go to `./cpp/genicam.cpp` and set `DISPLAY_WINDOW` to 1.
```
# if you are in the ./cpp folder

$ ./genicam
```

4. Close `./genicam` before running the python reader. You might need to change the `IMG_HEIGHT`, `IMG_WIDTH`, `IMG_DEPTH` values in the `reader.py` file depending on your camera. This sample python reader example provides a possible approach to interface with the camera's output in python by converting the data to a numpy array. With the numpy array you can then use the data to perform computer vision tasks (eg. opencv2, deep learning, etc.).
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
These settings can be found in `cpp/genicam.cpp` 

1. `TURBO_DRIVE` This refers to the Turbo Drive mode on DALSA cameras [more info](https://www.teledynedalsa.com/en/learn/knowledge-center/turbodrive/) 

*Value is set to 0 (ie. off) by default*

2. `DISPLAY_WINDOW` This refers to the window that is being spawned when `./genicam` is being run. When set to 1, the video window will popup. When set to 0, the video window will be hidden. 

*Value is set to 0 (ie. hidden) by default*

3. `PRINT_STATEMENTS` When set to 1, the program will print information regarding the program state. When set to 0, the program will not print anything. 

*Value is set to 0 by default*. Since we are using stdout to communicate, printing will contaminate the stdout stream. If having print statements is needed, a possible solution is to write the data to a named pipe.
