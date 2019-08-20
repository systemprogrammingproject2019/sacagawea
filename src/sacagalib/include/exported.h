#ifndef EXPORTED_H
#define EXPORTED_H

/* This is needed in order to export functions to the DLL
 * (Linux doesn't need an equivalent for the .so file because)
 * all the functions are imported by default
 */
#ifdef _WIN32
# ifdef WIN_EXPORT
#   define EXPORTED  __declspec(dllexport)
# else
#   define EXPORTED  __declspec(dllimport)
# endif
#else
# define EXPORTED
#endif

#endif