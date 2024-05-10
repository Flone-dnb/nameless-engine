#ifdef READ_ONLY_LIGHT_GRID // define to mark light grids and light index lists as `readonly`
    #glsl #define LIGHT_GRID_QUALIFIER readonly
    // macros as types don't work in HLSL for some reason :(
#else
    #glsl #define LIGHT_GRID_QUALIFIER
#endif