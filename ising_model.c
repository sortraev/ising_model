// #include <time.h>    // time(), clock()
#include <stdio.h>   // printf()
#include <stdlib.h>  // malloc(), rand()
#include <unistd.h>  // usleep()
#include <math.h>    // exp()
#include <pthread.h> // pthread stuff
#include <string.h>  // memset()
#include <signal.h>  // signal()
#include <sys/ioctl.h>
#include <termios.h>

#include <stdint.h>


// visualization stuff. ignore this.
#define MIN_FPS 4
#define MAX_FPS 60
#define DO_COLOR 1

#define COLOR_BLUE  "\x1b[33m"
#define COLOR_RED   "\x1b[31m"
#define RESET_COLOR "\x1b[0m"

#define RAND_UNIFORM() ((float) rand() / RAND_MAX)
#define MAX(x, y) ((x) >  (y) ? (x) : (y))
#define MIN(x, y) ((x) <= (y) ? (x) : (y))

typedef char spin;
typedef char energy;

// ferromagnet grid dimensions.
int H = 1;
int W = 1;
int FPS = 24;
int W0; // width of internal string representation.

void init_spins(spin *spins);
void compute_energies(spin *spins, energy *energies);
void update_spins(energy *energies, spin *spins);
void render(spin *spins, char *screen);

void *user_input_thread(void *_unused);
void sigint_handler(int sig);
void set_raw_mode();
void set_cooked_mode();

static inline char char_abs(char x) {
  char sign_bit = x >> 7;
  return (x ^ sign_bit) - sign_bit;
}

struct termios orig_termios;

// simulation parameters.
const double T_INC = 0.004;
const double P_INC = 0.002;
double T = 0.5;
double P = 0.1;
int RUNNING = 1;

int main() {

  if (signal(SIGINT, sigint_handler) != 0) {
    fprintf(stderr, "Error setting SIGINT handler.\n");
    exit(1);
  }
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  H = w.ws_row - 1;
  W = w.ws_col;

  W0 = W + 1;
  srand(43);

  pthread_t thd1;
  if (pthread_create(&thd1, NULL, user_input_thread, NULL) != 0) {
    fprintf(stderr, "pthread_create error\n");
    exit(1);
  }

  energy *energies = calloc(H * W0, sizeof(energy));
  spin *spins = malloc(H * W * sizeof(spin));

  init_spins(spins);

#if DO_COLOR
  printf(COLOR_RED); // red color output.
#endif

  set_raw_mode();
  while (RUNNING) {
    compute_energies(spins, energies);
    update_spins(energies, spins);

    render(spins, energies);
    fwrite(energies, sizeof(char), H * W0, stdout);
    printf("\n (T, P, FPS) = (%lf, %lf, %d)", T, P, FPS);
    printf("\r\033[%dA", H + 1);
    usleep((1000000 / FPS));
  }
  set_cooked_mode();


#if DO_COLOR
  printf("\033[%dB%s\n", H, RESET_COLOR);
#endif

  free(spins);
  free(energies);
}


void init_spins(spin *spins) {
  for (int i = 0; i < H * W; i++)
      spins[i] = (rand() & 2) - 1; // random zeroes and ones.
}


// optimized version of compute_energies(), which runs about 2-6x
// faster than its arguably prettier and better readable counterpart.
void compute_energies(spin *spins, energy *energies) {
  int x, y, z;
  y = 0;
  for (int i = 0; i < H; i++) {

    x = ((i + H - 1) % H) * W;
    z = ((i + 1) % H)     * W;

    spin ul = spins[x + W - 1];
    spin u  = spins[x];
    spin ur = spins[x + 1];
    spin l  = spins[y + W - 1];
    spin c  = spins[y];
    spin r  = spins[y + 1];
    spin dl = spins[z + W - 1];
    spin d  = spins[z];
    spin dr = spins[z + 1];

    energies[i + y] = (c * (ul + u + ur + l + r + dl + d + dr)) << 1;

    for (int j = 1; j < W - 1; j++) {

      ul = spins[x + j - 1];
      u  = spins[x + j];
      ur = spins[x + j + 1];
      l  = spins[y + j - 1];
      c  = spins[y + j];
      r  = spins[y + j + 1];
      dl = spins[z + j - 1];
      d  = spins[z + j];
      dr = spins[z + j + 1];

      energies[y + i + j] = (c * (ul + u + ur + l + r + dl + d + dr)) << 1;
    }

    x += W;
    y += W;
    z += W;

    ul = spins[x - 2];
    u  = spins[x - 1];
    ur = spins[x - W];
    l  = spins[y - 2];
    c  = spins[y - 1];
    r  = spins[y - W];
    dl = spins[z - 2];
    d  = spins[z - 1];
    dr = spins[z - W];

    energies[y + i - 1] = (c * (ul + u + ur + l + r + dl + d + dr)) << 1;

  }
}

// update the grid of spins based on the current grid of energies.
void update_spins(energy *energies, spin *spins) {
  for (int i = 0; i < H; i++) {
    for (int j = 0; j < W; j++) {
      energy e;
      spins[i * W + j] *=
        !(RAND_UNIFORM() < P &&
          ((e = energies[i * W0 + j]) < 0 ||
          RAND_UNIFORM() < exp((float) -e / T))) * 2 - 1;
    }
  }
}


// "render" the grid of spins by filling the energies array with symbols.
void render(spin *spins, char *energies) {
  static const char symbols[6] = " .o*%#";
  for (int i = 0; i < H; i++)
    for (int j = 0; j < W; j++)
      energies[i * W0 + j] =
        symbols[(char_abs(energies[i * W0 + j]) / 4 + 1)
                 * (spins[i * W + j] > 0)];
}

void sigint_handler(int _sig) {
  (void) _sig;
  RUNNING = 0;
}


void *user_input_thread(void *_unused) {
  (void) _unused;
  char c;
  while (1) {
    if (read(STDIN_FILENO, &c, 1) != 0) {
      if (c == 'q') RUNNING = 0;

      else if (c == 'z') P = MAX(P * 0.99, 0.0);
      else if (c == 'Z') P = MAX(P - 0.1, 0.0);

      else if (c == 'x') P = MIN(P * 1.01, 1.0);
      else if (c == 'X') P = MIN(P + 0.1, 1.0);

      else if (c == 'a') T = MAX(T * 0.99, 0.0);
      else if (c == 'A') T = MAX(T - 0.1, 0.0);

      else if (c == 's') T *= 1.01;
      else if (c == 'S') T += 0.1;

      else if (c == 'd') FPS = MAX(FPS - 1, MIN_FPS);
      else if (c == 'f') FPS = MIN(FPS + 1, MAX_FPS);
    }
  }
}



// lifted from the internet.
void set_cooked_mode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
  printf("\r(set cooked mode)\n");
  printf("\e[?25h");
}
void set_raw_mode() {
  atexit(set_cooked_mode);
  struct termios raw;

  raw = orig_termios;  /* copy original and then modify below */

  /* input modes - clear indicated ones giving: no break, no CR to NL,
     no parity check, no strip char, no start/stop output (sic) control */
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

  /* output modes - clear giving: no post processing such as NL to CR+NL */
  raw.c_oflag &= ~(OPOST);

  /* control modes - set 8 bit chars */
  raw.c_cflag |= (CS8);

  /* local modes - clear giving: echoing off, canonical off (no erase with
     backspace, ^U,...),  no extended functions, no signal chars (^Z,^C) */
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

  /* control chars - set return condition: min number of bytes and timer */
  raw.c_cc[VMIN] = 5; raw.c_cc[VTIME] = 8; /* after 5 bytes or .8 seconds
                                              after first byte seen      */
  raw.c_cc[VMIN] = 0; raw.c_cc[VTIME] = 0; /* immediate - anything       */
  raw.c_cc[VMIN] = 2; raw.c_cc[VTIME] = 0; /* after two bytes, no timer  */
  raw.c_cc[VMIN] = 0; raw.c_cc[VTIME] = 8; /* after a byte or .8 seconds */

  /* put terminal in raw mode after flushing */
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
    fprintf(stderr, "Failed to set raw mode\n");
    exit(1);
  }
}
