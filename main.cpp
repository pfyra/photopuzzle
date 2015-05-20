#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_rect.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <vector>
#include <getopt.h>
#include <list>

#include "vec2.h"

/*
 * To do:
   - add parameters for 
          * vertical puzzle board or rotation of puzzle board (90, 180, 270 degrees)
          * disable rotation of pieces
          * sticky left mouse button, i.e. click a piece move and click to release it
   - randomize the start position for pieces
   - add less lame victory
   - background pic/colors
   - add sound when
      * a puzzle piece is placed correctly
      * a piece is "picked up" (i.e when piece_held_by_mouse is set)
      * victory

 * Known issues:
   - The pegs do not always look OK (looks like they are cut off)
      - this is likely because the code does not handle small angles very well, 
        e.g. a vertical peg in the right direction where the top of the peg is moving 
        like 3 pixels to the right and 1 up, 3 right 1 up etc. This will create holes in the piece_map.

 */


struct piece {
  // position in the original picture (static)
  SDL_Rect correct_pos;

  // current position (movable)
  SDL_Rect current_pos;

  // clickable part of the surface, i.e will cover most of the piece
  // so it should be good enough
  SDL_Rect piece_area;

  int current_rotation;

  int width;
  int height;
  
  int piece_idx_x;
  int piece_idx_y;

  SDL_Surface *surface;
  SDL_Texture *texture;
};


SDL_Window *sdlWindow;
SDL_Renderer *sdlRenderer;

bool running = true;
const char * photo_filename = 0;
SDL_Surface *photo = 0;

SDL_Point mouseposition;
piece *piece_held_by_mouse;
std::list<piece*> piece_render_order;

SDL_Surface *piece_hint_map;
SDL_Texture *piece_hint_map_txt;
SDL_Rect piece_hint_rect;

int piecewidth = -1; // width per piece
int pieceheight = -1;
SDL_Point puzzlearea[5];

SDL_Color bgcolor         = {.r=0x40, .g=0x00, .b=0x30, .a=0xff};
SDL_Color puzzleareacolor = {.r=0x30, .g=0x30, .b=0x70, .a=0xff};

float boardsize_percent = 0.5; // board size in percent of screen_width/height

/*
 * piece_map as an int will probably not suffice when more than 3*3 pieces are used, say 2*10 or so, the math breaks for getting the indices (I think)
 */

piece **pieces = 0;  // one struct per puzzle piece
int **piece_map = 0; // one int per pixel in the puzzle, indicating indices for the 'pieces' matrix (x*pieces_x + y)


// settable by parameters:
int fullscreen_flag = 0;
int show_hint_flag = 0;
int screen_width = 1280;
int screen_height = 1024;
int width = -1; // board size
int height = -1;
int pieces_x = 3;
int pieces_y = 3;
int auto_correct_distance = 5; // how close the piece need be to "jump" into correct position

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
const uint32_t rmask = 0xff000000;
const uint32_t gmask = 0x00ff0000;
const uint32_t bmask = 0x0000ff00;
const uint32_t amask = 0x000000ff;
#else
const uint32_t rmask = 0x000000ff;
const uint32_t gmask = 0x0000ff00;
const uint32_t bmask = 0x00ff0000;
const uint32_t amask = 0xff000000;
#endif


piece* xy_to_piece(int x, int y){
  return &pieces[ piece_map[x][y]%pieces_x ][ piece_map[x][y]/pieces_x ];
}




void handle_keypress( SDL_Event *e){
  if (e->key.keysym.sym == SDLK_ESCAPE){
    running = false;
  }
}

void handle_keyrelease( SDL_Event *e){
}

int distance(SDL_Rect *a, SDL_Rect *b){
  int dx = a->x - b->x;
  int dy = a->y - b->y;
  
  return sqrt(dx*dx + dy*dy);
}

bool pointInRect(const SDL_Point * const p, const SDL_Rect * const r){
  return (r->x        < p->x) &&
         (r->x + r->w > p->x) &&
         (r->y        < p->y) &&
         (r->y + r->h > p->y);

}
void handle_right_mousebuttonup(SDL_Event *e){

}

void handle_right_mousebuttondown(SDL_Event *e){

  /* TODO: loop through piece_render_order instead and rotate only top one */
  for (int i=0; i<pieces_x; i++){
    for(int j=0; j<pieces_y; j++){
      if (pointInRect(&mouseposition, &pieces[i][j].piece_area)){
        pieces[i][j].current_rotation += 90;
        pieces[i][j].current_rotation %= 360;
      }
    }
  }
}

void handle_left_mousebuttonup(SDL_Event *e){
  piece_held_by_mouse = 0;
}

void handle_left_mousebuttondown(SDL_Event *e){
  
  for (int i=0; i<pieces_x; i++){
    for(int j=0; j<pieces_y; j++){
      if (pointInRect(&mouseposition, &pieces[i][j].piece_area)){
        piece_held_by_mouse = &pieces[i][j];
        // push 'piece_held_by_mouse' to front (drawn last)
        piece_render_order.remove(piece_held_by_mouse);
        piece_render_order.push_back(piece_held_by_mouse);
/*        printf("%d %d is in %d %d (x %d y %d  w %d h %d)   cor %d %d\n", 
               mouseposition.x,
               mouseposition.y,
               i, j,
               piece_held_by_mouse->piece_area.x,
               piece_held_by_mouse->piece_area.y,
               piece_held_by_mouse->piece_area.w,
               piece_held_by_mouse->piece_area.h,
               piece_held_by_mouse->correct_pos.x,
               piece_held_by_mouse->correct_pos.y);
*/
      }
    }
  }
}

void handle_mousemotion(SDL_Event *e){
//  printf("%d  %d\n", e->motion.x, e->motion.y);
  if(piece_held_by_mouse){
    piece_held_by_mouse->current_pos.x += e->motion.x - mouseposition.x;
    piece_held_by_mouse->current_pos.y += e->motion.y - mouseposition.y;
    piece_held_by_mouse->piece_area.x  += e->motion.x - mouseposition.x;
    piece_held_by_mouse->piece_area.y  += e->motion.y - mouseposition.y;

/*    printf("%d %d  (x %d y %d  w %d h %d)   cor %d %d   dist %d rot %d\n", 
           mouseposition.x,
           mouseposition.y,
           piece_held_by_mouse->piece_area.x,
           piece_held_by_mouse->piece_area.y,
           piece_held_by_mouse->piece_area.w,
           piece_held_by_mouse->piece_area.h,
           piece_held_by_mouse->correct_pos.x,
           piece_held_by_mouse->correct_pos.y,
           distance(&piece_held_by_mouse->current_pos, &piece_held_by_mouse->correct_pos),
           piece_held_by_mouse->current_rotation);
*/
    if (distance(&piece_held_by_mouse->current_pos, &piece_held_by_mouse->correct_pos) < auto_correct_distance &&
          piece_held_by_mouse->current_rotation == 0){
      piece_held_by_mouse->current_pos = piece_held_by_mouse->correct_pos;

      bool all_pieces_correct = true;
      for (int i=0; i<pieces_x; i++){
        for(int j=0; j<pieces_y; j++){
          if (distance(&pieces[i][j].current_pos, &pieces[i][j].correct_pos) > 1 ||
              pieces[i][j].current_rotation != 0){
            all_pieces_correct = false;
          }
        }
      }
      if (all_pieces_correct){
        printf("Congratulations, puzzle is finished!\n");
      }
    }

  }
  mouseposition.x = e->motion.x;
  mouseposition.y = e->motion.y;
}

void handle_event(SDL_Event *e){
  switch(e->type){
  case SDL_QUIT: 
    running = false; 
    break;

  case SDL_KEYDOWN: {
    handle_keypress(e);
    break;
  }
 
  case SDL_KEYUP: {
    handle_keyrelease(e);
//    handle_keyrelease(e->key.keysym.sym,
//                      e->key.keysym.mod/*,
//                      e->key.keysym.unicode*/);
    break;
  }

  case SDL_MOUSEMOTION:{
    handle_mousemotion(e);
    break;
  }

  case SDL_MOUSEBUTTONUP:{
    switch(e->button.button){
    case SDL_BUTTON_LEFT:   handle_left_mousebuttonup(e);  break;
    case SDL_BUTTON_RIGHT:  handle_right_mousebuttonup(e); break;
    default:                                               break;
    }
    break;
  }

  case SDL_MOUSEBUTTONDOWN:{
    switch(e->button.button){
    case SDL_BUTTON_LEFT:   handle_left_mousebuttondown(e);  break;
    case SDL_BUTTON_RIGHT:  handle_right_mousebuttondown(e); break;
    default:                                                 break;
    }
    break;
  }

  default:
    break;
  }  

}


void events(){
  SDL_Event e;

  while(running && SDL_PollEvent(&e)){
    handle_event(&e);
  }
};

void loop(){
}


void render(){
  /* background color */
  SDL_SetRenderDrawColor(sdlRenderer, bgcolor.r, bgcolor.g, bgcolor.b, bgcolor.a);
  SDL_RenderClear(sdlRenderer);

  /* puzzle area, incl. hint if enabled */
  SDL_SetRenderDrawColor(sdlRenderer, puzzleareacolor.r, puzzleareacolor.g, puzzleareacolor.b, puzzleareacolor.a);
  SDL_RenderCopy(sdlRenderer, piece_hint_map_txt, 0, &piece_hint_rect);

  /* frame for puzzle area */
  SDL_SetRenderDrawColor(sdlRenderer, 255, 255, 255, 255);
  SDL_RenderDrawLines(sdlRenderer, puzzlearea, 5);

  /* pieces */ 
  for (std::list<piece*>::iterator it=piece_render_order.begin(); it!=piece_render_order.end(); ++it) {
    SDL_RenderCopyEx(sdlRenderer, 
                     (*it)->texture, 
                     0, 
                     &((*it)->current_pos),
                     (*it)->current_rotation,
                     0,
                     SDL_FLIP_NONE);
  }
  
  SDL_RenderPresent(sdlRenderer);
}
/*
bool pointInRect(int x, int y, SDL_Rect *r){
  return 
}
*/
void get_random_position( int *x, int *y){
  SDL_Rect screen = {.x = 0, .y = 0, .w = screen_width - piecewidth, .h = screen_height - pieceheight};
  SDL_Point p;
  do{
    *x = rand() % screen.w;
    *y = rand() % screen.h;
    p.x = *x;
    p.y = *y;
  }while(!(pointInRect(&p, &screen)));
}

bool init(){
  width = screen_width*boardsize_percent; 
  height= screen_height*boardsize_percent;

  piecewidth = width/pieces_x; // width per piece
  pieceheight = height/pieces_y;
        

  if (SDL_Init(SDL_INIT_EVERYTHING) < 0){
    return false;
  }

  piece_map = new int*[width];
  for (int i=0; i<width; i++){
    piece_map[i] = new int[height];
    for(int j=0; j<height; j++){
      piece_map[i][j] = -1;
    }
  }

  SDL_ShowCursor( SDL_ENABLE );

  SDL_Surface *orig_photo;

  if ((orig_photo = IMG_Load(photo_filename)) == 0){
    fprintf(stderr, "Couldn't open image: %s\n", photo_filename);
    return false;
  }

  photo = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 32, rmask,gmask,bmask,amask);

  SDL_BlitScaled(orig_photo, 0, photo, 0);

  SDL_FreeSurface(orig_photo);

  int flags = 0;// SDL_HWSURFACE | SDL_DOUBLEBUF;
  if (fullscreen_flag){
    flags |= SDL_WINDOW_FULLSCREEN;
  }

  if((sdlWindow = SDL_CreateWindow("Photo Puzzle", 
                                   SDL_WINDOWPOS_UNDEFINED, 
                                   SDL_WINDOWPOS_UNDEFINED, 
                                   screen_width, 
                                   screen_height, 
                                   flags)) == NULL) {
    return false;
  }

  if ((sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, 0)) == NULL){
    printf("SDL Error: %s\n", SDL_GetError());
    return false;
  }
  SDL_ClearError();

  srand(time(NULL));

  pieces = new piece*[pieces_x];
  
  for (int x=0; x<pieces_x; x++){
    pieces[x] = new piece[pieces_y];
    for (int y=0; y<pieces_y; y++){

      /* initialize basics for the piece */
      pieces[x][y].width = piecewidth*2;
      pieces[x][y].height = pieceheight*2;

      pieces[x][y].correct_pos.x = screen_width /2 - width /2 + x*piecewidth - 0.5*piecewidth;
      pieces[x][y].correct_pos.y = screen_height /2 - height /2 + y*pieceheight - 0.5*pieceheight;
      pieces[x][y].correct_pos.w = pieces[x][y].width;
      pieces[x][y].correct_pos.h = pieces[x][y].height;

      

      get_random_position(
        &pieces[x][y].current_pos.x,
        &pieces[x][y].current_pos.y);

//      pieces[x][y].current_pos.x = //x*(piecewidth+50);
//      pieces[x][y].current_pos.y = //y*(pieceheight+50);
      pieces[x][y].current_pos.w = pieces[x][y].width;
      pieces[x][y].current_pos.h = pieces[x][y].height;

      pieces[x][y].piece_area.x = pieces[x][y].current_pos.x + 0.5*piecewidth;
      pieces[x][y].piece_area.y = pieces[x][y].current_pos.y + 0.5*pieceheight;
      pieces[x][y].piece_area.w = piecewidth;
      pieces[x][y].piece_area.h = pieceheight;

      pieces[x][y].current_rotation = (rand()%4)*90;

      pieces[x][y].piece_idx_x = x;
      pieces[x][y].piece_idx_y = y;
      pieces[x][y].surface = SDL_CreateRGBSurface(SDL_SWSURFACE, 
                                                  pieces[x][y].width,
                                                  pieces[x][y].height,
                                                  photo->format->BitsPerPixel,
                                                  photo->format->Rmask,
                                                  photo->format->Gmask,
                                                  photo->format->Bmask,
                                                  photo->format->Amask);

      /*
       * The surface for the piece is a bit bigger (twice the size..) than the piece so the peg will fit
       */

      // make the surroundings of the piece transparent
      SDL_FillRect(pieces[x][y].surface, 0, SDL_MapRGBA(pieces[x][y].surface->format, 0,0,0,0));

      pieces[x][y].texture = 0;

      /* create the hole and peg where the pieces connect */
      //horizontal split
      if (y > 0){
        int flip = 1;
        int flipback = -1;

        if (rand()%2==0){
          flip = -1;
          flipback = 1;
        }

        vec2 * points = new vec2[6];
        points[0].x = 0;                 points[0].y = 0;
        points[1].x = piecewidth;        points[1].y = (flipback*(pieceheight/4));
        points[2].x = -(piecewidth);     points[2].y = (flip*(pieceheight/2));
        points[3].x = (piecewidth)*2;    points[3].y = (flip*(pieceheight/2));
        points[4].x = -(piecewidth/2);   points[4].y = (flipback*(pieceheight/4));
        points[5].x = (piecewidth);      points[5].y = 0;

        std::vector<vec2> vectors;

        for(int i=0; i<6; i++){
          points[i].x += x*(piecewidth);
          points[i].y += y*(pieceheight);
        }

        for(int i=1; i<5; i++){
          if (points[i].x != 0){
            points[i].x += (rand() % (int)(piecewidth/20) ) - piecewidth/10;
          }
          if (points[i].y != 0){
            points[i].y += (rand() % (int)(pieceheight/20) ) - pieceheight/10;
          }
        }

        for (float f=0; f<=1; f += 0.0001){
          vec2 p = getBezierPoint(points, 6, f);
          vectors.push_back(p);
        }
        delete [] points; points = 0;

        int above_piece_idx = (y-1)*pieces_x + x;
        int piece_idx = (y)*pieces_x + x;
        
        for(unsigned int i=0; i<vectors.size()-1; i++){
          int pointx = vectors[i].x;
          int pointy = vectors[i].y;

          bool left         = (int)vectors[i].x >= (int)vectors[i+1].x;
          bool strict_left  = (int)vectors[i].x >  (int)vectors[i+1].x;
//          bool right        = (int)vectors[i].x <= (int)vectors[i+1].x;
          bool strict_right = (int)vectors[i].x <  (int)vectors[i+1].x;
          bool no_horiz_move= (int)vectors[i].x == (int)vectors[i+1].x;
                    
          bool up           = (int)vectors[i].y >= (int)vectors[i+1].y;
//          bool strict_up    = (int)vectors[i].y >  (int)vectors[i+1].y;
          bool down         = (int)vectors[i].y <= (int)vectors[i+1].y;
          bool strict_down  = (int)vectors[i].y <  (int)vectors[i+1].y;
//          bool no_vert_move = (int)vectors[i].y == (int)vectors[i+1].y;


          if (strict_right && up){
            /* spline is moving to the right (possibly up-right) */
            piece_map[ pointx] [ pointy -1 ] = above_piece_idx;
            piece_map[ pointx] [ pointy    ] = piece_idx;
            piece_map[ pointx] [ pointy +1 ] = piece_idx;
          }

          if (up && left){
            /* spline is moving up (possibly up left) */
/*            piece_map[ pointx -1] [ pointy ] = above_piece_idx ;
*/            piece_map[ pointx   ] [ pointy ] = piece_idx ;
//            piece_map[ pointx +1] [ pointy ] = piece_idx +5;

          }

          if (down && strict_left){
            /* spline is moving straight down or down left */
            piece_map[ pointx -1] [ pointy ] = above_piece_idx;
            piece_map[ pointx   ] [ pointy ] = piece_idx;
//            piece_map[ pointx +1] [ pointy ] = piece_idx;
          }

          if (strict_down && strict_left){
            /* spline is moving straight down or down left */
            piece_map[ pointx   ] [ pointy ] = above_piece_idx;
            piece_map[ pointx -1] [ pointy ] = piece_idx;
//            piece_map[ pointx +1] [ pointy ] = piece_idx;
          }

          if (strict_down && strict_right){
            /* spline is moving straight down or down left */

            piece_map[ pointx   ] [ pointy -1] = above_piece_idx;
            piece_map[ pointx +1] [ pointy   ] = above_piece_idx;
            piece_map[ pointx   ] [ pointy ] = piece_idx;
          }

          if (strict_down && no_horiz_move){
            piece_map[ pointx   ] [ pointy ] = piece_idx;
            piece_map[ pointx +1] [ pointy ] = above_piece_idx;
            
          }
        }
      }

      // vertical
      if (x>0){
        int flip = 1;
        int flipback = -1;

        if (rand()%2==0){
          flip = -1;
          flipback = 1;
        }

        vec2 * points = new vec2[6];
        points[0].x = 0;                         points[0].y = 0;
        points[1].x = (flipback*(piecewidth/8)); points[1].y = pieceheight + (pieceheight/4);
        points[2].x = (flip*(piecewidth/2));     points[2].y = -pieceheight;
        points[3].x = (flip*(piecewidth/2));     points[3].y = pieceheight*2;
        points[4].x = (flipback*(piecewidth/8)); points[4].y = 0;
        points[5].x = 0;                         points[5].y = pieceheight;
  
        for(int i=0; i<6; i++){
          points[i].x += x*(piecewidth);
          points[i].y += y*(pieceheight);
        }

        for(int i=1; i<5; i++){
          if (points[i].x != 0){
            points[i].x += (rand() % (int)(piecewidth/20) ) - piecewidth/10;
          }
          if (points[i].y != 0){
            points[i].y += (rand() % (int)(pieceheight/20) ) - pieceheight/10;
          }
        }

        std::vector<vec2> vectors;
        for (float f=0; f<=1; f += 0.0001){
          vec2 p = getBezierPoint(points, 6, f);
          vectors.push_back(p);
        }
        delete [] points; points = 0;

        int piece_idx = (x+1) + (y)*pieces_x - 1;
        int left_piece_idx = (x) + (y)*pieces_x - 1;
          
//        printf("x:y %d:%d  peice: %d   left: %d\n", x,y,piece_idx, left_piece_idx);

        for(unsigned int i=0; i<vectors.size()-1; i++){
          int pointx = vectors[i].x;
          int pointy = vectors[i].y;

          bool left         = (int)vectors[i].x >= (int)vectors[i+1].x;
          bool strict_left  = (int)vectors[i].x >  (int)vectors[i+1].x;
//          bool right        = (int)vectors[i].x <= (int)vectors[i+1].x;
          bool strict_right = (int)vectors[i].x <  (int)vectors[i+1].x;
          bool no_horiz_move= (int)vectors[i].x == (int)vectors[i+1].x;
                    
          bool up           = (int)vectors[i].y <= (int)vectors[i+1].y;
          bool strict_up    = (int)vectors[i].y <  (int)vectors[i+1].y;
          bool down         = (int)vectors[i].y >= (int)vectors[i+1].y;
          bool strict_down  = (int)vectors[i].y >  (int)vectors[i+1].y;
//          bool no_vert_move = (int)vectors[i].y == (int)vectors[i+1].y;

          if (strict_right && strict_up){
            /* spline is moving to the right (possibly up-right) */
            piece_map[ pointx-1] [ pointy  ] = left_piece_idx;
            piece_map[ pointx] [ pointy    ] = piece_idx;
            piece_map[ pointx] [ pointy +1 ] = piece_idx;
          }

          if (up && left){
            /* spline is moving up (possibly up left) */
/*            piece_map[ pointx -1] [ pointy ] = above_piece_idx ;
*/            piece_map[ pointx   ] [ pointy ] = piece_idx;
//            piece_map[ pointx +1] [ pointy ] = piece_idx +5;

          }

          if (up && strict_right){
            /* spline is moving straight down or down left */
            piece_map[ pointx   ] [ pointy ] = piece_idx;
            piece_map[ pointx +1] [ pointy ] = piece_idx;
          }


          if (down && strict_left){
            /* spline is moving straight down or down left */
            piece_map[ pointx -1] [ pointy ] = left_piece_idx;
            piece_map[ pointx   ] [ pointy ] = piece_idx;
            piece_map[ pointx +1] [ pointy ] = piece_idx;
          }

          if (strict_down && strict_right){
            /* spline is moving straight down or down left */

//            piece_map[ pointx   ] [ pointy -1] = left_piece_idx;
            piece_map[ pointx +1] [ pointy   ] = left_piece_idx;
            piece_map[ pointx   ] [ pointy ] = piece_idx;
            piece_map[ pointx -1] [ pointy ] = piece_idx;
          }

          if (strict_down && no_horiz_move){
            piece_map[ pointx   ] [ pointy ] = piece_idx;
            piece_map[ pointx -1] [ pointy ] = left_piece_idx;
            
          }
        }
      }
    }
  }


  /*
   * Create the hint map surface from the piece_map before it is completely filled (only the borders are in there now)
   */  
  piece_hint_rect.x = screen_width /2 - width /2;
  piece_hint_rect.y = screen_height/2 - height/2;
  piece_hint_rect.w = width;
  piece_hint_rect.h = height;

  piece_hint_map = SDL_CreateRGBSurface(SDL_SWSURFACE, 
                                        width,
                                        height,
                                        32,0,0,0,0);
  SDL_FillRect(piece_hint_map, 0, SDL_MapRGBA(piece_hint_map->format, 
                                              puzzleareacolor.r,
                                              puzzleareacolor.g,
                                              puzzleareacolor.b,
                                              puzzleareacolor.a));
  if (show_hint_flag){
    for(int j=0; j<height; j++){
      for (int i=0; i<width; i++){
        if (piece_map[i][j] != -1){
          uint8_t *pixel = (uint8_t*)piece_hint_map->pixels + j*piece_hint_map->pitch + i*piece_hint_map->format->BytesPerPixel;
          uint32_t *pixel32 = (uint32_t*)pixel;
          *pixel32 = 0xffffffff;
        }
      }
    }
  }
  piece_hint_map_txt = SDL_CreateTextureFromSurface(sdlRenderer, piece_hint_map);

  /*
   * fill the frame with piece_ids
   */

  // top line
  int cur_id = 0;
  piece_map[0][0] = 0;
  for (int i=0; i<width; i++){
    if (piece_map[i][0] != -1)
      cur_id = piece_map[i][0];
    else
      piece_map[i][0] = cur_id;
  }

  // leftmost column
  for (int i=0; i<height; i++){
    if (piece_map[0][i] != -1)
      cur_id = piece_map[0][i];
    else
      piece_map[0][i] = cur_id;
  }

  // fill the rest within the frame
  for(int j=0; j<height; j++){
    for (int i=0; i<width; i++){
      if (piece_map[i][j] != -1)
        cur_id = piece_map[i][j];
      else
        piece_map[i][j] = cur_id;
    }
  }

  /* debug: print the pixel to piece mapping (pipe it to a file) */
/*
  for(int j=0; j<height; j++){
    for (int i=0; i<width; i++){
      if ( piece_map[i][j] == -1)
        printf(" ");
      else
        printf("%c", piece_map[i][j] + '0');
    }
    printf("\n");
  }
*/

  /*
   * Fill pieces with photo data
   */
  for (int i=2; i<width-1; i++){  //skip some pixels to make sure the puzzle area border is visible
    for(int j=2; j<height-2; j++){
      piece *p = xy_to_piece(i,j);
      int pixpos_x = i - p->piece_idx_x*piecewidth  + 0.5*piecewidth;
      int pixpos_y = j - p->piece_idx_y*pieceheight  + 0.5*pieceheight;

      uint8_t *src  = (uint8_t*)     photo->pixels +    j    *     photo->pitch +     i   *     photo->format->BytesPerPixel;
      uint8_t *dest = (uint8_t*)p->surface->pixels + pixpos_y*p->surface->pitch + pixpos_x*p->surface->format->BytesPerPixel;
      switch(photo->format->BytesPerPixel){
      case 4: *dest++ = *src++;  
      case 3: *dest++ = *src++;  
      case 2: *dest++ = *src++;  
      case 1: *dest++ = *src++; 
        break;
      default:
        exit(-1);
      } 
    }
  }

  for (int i=0; i<pieces_x; i++){
    for(int j=0; j<pieces_y; j++){
      piece *p = &pieces[i][j];
      p->texture = SDL_CreateTextureFromSurface(sdlRenderer, p->surface);
      piece_render_order.push_back(p);
    }
  }

  /*
   * Define square for puzzle area
   */
  puzzlearea[0].x = screen_width /2 - width /2;  //upper left corner
  puzzlearea[0].y = screen_height/2 - height/2;
  puzzlearea[1].x = screen_width /2 + width /2;  // upper right corner
  puzzlearea[1].y = screen_height/2 - height/2;
  puzzlearea[2].x = screen_width /2 + width /2;  // lower right corner
  puzzlearea[2].y = screen_height/2 + height/2;
  puzzlearea[3].x = screen_width /2 - width /2;  // lower left corner
  puzzlearea[3].y = screen_height/2 + height/2;
  puzzlearea[4] = puzzlearea[0];                 // and back to the upper left corner

  return true;
}

void quit(){
  for (int i=0; i<pieces_x; i++){
    for(int j=0; j<pieces_y; j++){
      piece *p = &pieces[i][j];
      SDL_FreeSurface(p->surface);
      SDL_DestroyTexture(p->texture);
    }
  }
  for (int i=0; i<width; i++){
    delete [] piece_map[i];
  }
  delete [] piece_map;

  IMG_Quit();
  SDL_Quit();
}

void print_help(const char * const argv0){
  printf("Usage %s: [OPTION]... <FILE>\n" 
         "Make a basic puzzle game from a picture or photo of yours\n" 
         "\n" 
         "Mandatory arguments to long options are mandatory for short options too.\n" 
         " -s, --size         game screen size in pixels, XxY or X*Y\n"
         " -p, --pieces       number of pieces in puzzle, XxY or X*Y\n"
         " -a, --auto_correct_distance  max distance for pieces to auto correct the position\n"
         "     --hint         show hint for pieces\n"
         "     --fullscreen   show game in full screen (hit escape to quit)\n"
        ,
         argv0);
}


unsigned int diff(timespec start, timespec end)
{
  timespec temp;
  if ((end.tv_nsec-start.tv_nsec)<0) {
    temp.tv_sec = end.tv_sec-start.tv_sec-1;
    temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
  } else {
    temp.tv_sec = end.tv_sec-start.tv_sec;
    temp.tv_nsec = end.tv_nsec-start.tv_nsec;
  }
  return temp.tv_nsec+temp.tv_sec*1000000000;
}


int main(int argc, char *argv[]){
  int c;

//  bool vertical_puzzle = false;

  static struct option long_options[] =
        {
          /* These options set a flag. */
          {"hint",                  no_argument,       &show_hint_flag, 1},
          {"fullscreen",            no_argument,       &fullscreen_flag, 1},
          /* These options don’t set a flag.
             We distinguish them by their indices. */
          {"size",                  required_argument,       0, 's'},
          {"pieces",                required_argument,       0, 'p'},
          {"auto_correct_distance",  required_argument,       0, 'a'},
          {0, 0, 0, 0}
        };

  opterr = 0;
  int option_index = 0;

  while ((c = getopt_long (argc, argv, "a:s:p:f", long_options, &option_index)) != -1){
    switch (c) {
    case 's': {
      if (sscanf(optarg, "%dx%d", &screen_width, &screen_height) != 2
          && sscanf(optarg, "%d*%d", &screen_width, &screen_height) != 2){
        printf("-s, --size takes parameter of the format '<width>x<height>'\n");
        //print_help(argv[0]);
        return -1;
      }
      break;
    }
    case 'p':
      if (sscanf(optarg, "%dx%d", &pieces_x, &pieces_y) != 2
          && sscanf(optarg, "%d*%d", &pieces_x, &pieces_y) != 2){
        printf("-p, --pieces takes parameter of the format '<pieces width>x<pieces height>'\n");
        //print_help(argv[0]);
        return -1;
      }
      break;

    case 'a':
      auto_correct_distance = atoi(optarg);
      break;

    case 'f':
      fullscreen_flag = 1;
      break;

    case '?':
      switch(optopt){
      case 's':
      case 'h':
        printf("Option -%c requires an argument.\n", optopt);
        break;
      default:
        if (isprint(optopt)){
          printf("Unknown short parameter '%c'\n", optopt);
          print_help(argv[0]);
          return -1;
        }else{
          printf("Unknown long parameter '%s'\n",  argv[optind-1]);
          print_help(argv[0]);
          return -1;
        }
      }
      break;

    case 0:
      if (long_options[option_index].flag != 0){
        break;
      }
      printf("Unknown parameter: %s\n", long_options[option_index].name); // will this ever be hit?
      print_help(argv[0]);
      return -1;
      break;

    case -1:
      break;

    default:
      printf("Unknown long parameter: '%c'\n", c); // will this ever be hit?
      print_help(argv[0]);
      return -1;
      break;
    }
  }

  if (optind >= argc){
    printf("Filename for the puzzle needed\n");
    return -1;
  }

  photo_filename = strdup(argv[optind++]);

  if (optind < argc) {
    printf ("Ignored arguments: ");
    while (optind < argc)
      printf ("%s ", argv[optind++]);
    printf("\n");
  }

  if (!init()){
    return 1;
  }

  struct timespec ts_start;
  struct timespec ts_events;
  struct timespec ts_loop;
  struct timespec ts_render;
  int loopcount = 0;

  while(running) {
    clock_gettime(CLOCK_REALTIME, &ts_start);
    events();
    clock_gettime(CLOCK_REALTIME, &ts_events);
    loop();
    clock_gettime(CLOCK_REALTIME, &ts_loop);
    render();
    clock_gettime(CLOCK_REALTIME, &ts_render);
/*
    if (loopcount%100==0){
      
    }
*/
/*
    printf("events %u loop %u  render %u total %u\n", 
      diff(ts_start, ts_events),
      diff(ts_events, ts_loop),
      diff(ts_loop, ts_render),
      diff(ts_start, ts_render));
*/
    usleep(30000);
    loopcount++;
  } 


  quit();

  return 0;
};
