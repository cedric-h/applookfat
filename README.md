# you drop a WASM bundle on the screen and it parses it and visualizes the size of each section/function therein.

It also traces function calls, such that when you hover over one function, it also shows you what functions are called from it.

# example 

here's an example of what you get when you drop applookfat's own bundle into itself (kinky!)

![Screenshot_20230202_011058](https://github.com/cedric-h/applookfat/assets/25539554/ccd208f1-ffcc-49cf-bf36-d028b7dcfd70)

# philosophical meandering

i would argue that one of the greatest caveats of WASM is the killing blow it deals to the "open web."

hopefully this project "opens" your WASM for you a bit; although you do need debug symbols for it to be useful.


# methodology/improvements

it's all written in C and rendered with WebGL because your WASM bundle is fat as fuck and any overhead is too much overhead.

actually, i could be leveraging C and WASM far more efficiently; there are still WASM bundles for which this app stutters and chokes. progress to be made, iterations to be had and so forth.

