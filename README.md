# Mist
Mist is a render project for learning purposes.

![img](/readme_image.png)

## Requirements
* Install vulkan sdk
* premake5
* visual studio 2022
## Build
Cmd in the root of repository:
```
premake5 vs2022
```
This will create a vs2022 sln with the projects required.
The shader compiled files (.spv) must be generated building SHADERS project on vs2022.
To run the project, use **,ost_test** as startup project and press F5.
