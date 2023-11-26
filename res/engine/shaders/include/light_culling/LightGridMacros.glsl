#ifdef READ_ONLY_LIGHT_GRID // define to mark light grids and light index lists as `readonly`
#glsl #define LIGHT_GRID_QUALIFIER readonly
#else
#glsl #define LIGHT_GRID_QUALIFIER
#endif