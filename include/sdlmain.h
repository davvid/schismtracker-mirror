#ifndef __sdlmain_header
#  define __sdlmain_header

/* just a fancy way to get SDL headers */

#  ifdef USE_X11
#    undef DISABLE_X11
#    ifndef __unix__
#      define __unix__
#    endif
#  endif

#  include <SDL.h>
#  include <SDL_thread.h>
#  ifndef __cplusplus
#    include <SDL_syswm.h>
#    ifdef USE_OPENGL
#      include <SDL_opengl.h>
#    endif
#  endif

#endif /* ! __sdlmain_header */

