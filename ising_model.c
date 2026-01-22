#include <time.h>    // time()
#include <stdio.h>   // printf()
#include <stdlib.h>  // malloc(), rand(), srand()
#include <unistd.h>  // usleep()
#include <math.h>    // exp()
#include <pthread.h> // pthread stuff
#include <string.h>  // memset()
#include <signal.h>  // signal()
#include <sys/ioctl.h>
#include <termios.h>
#include <errno.h>

#include <stdint.h>

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

typedef char spin;
typedef char energy;

void init_spins(spin *spins, size_t n);
void compute_energies(spin *spins, energy *energies);
void update_spins(energy *energies, spin *spins);
void render(spin *spins, char *screen);

void *user_input_handler_thread(void *_unused);
int init(void);
void window_resize(void);

// visualization stuff.
#define MIN_FPS 4
#define MAX_FPS 60
int FPS = 24;
int running = 1;

// screen (and simulation grid) dimensions.
int H = 0;
int W = 0;
int W0 = 0; // width of internal string representation.
int need_resize = 0;

energy *energies = NULL;
spin   *spins    = NULL;

// simulation parameters.
double T = M_PI;
double P = 0.42;

int do_pause = 0;

int main(void) {

  if (init() != 0) {
    return 1;
  }

  energies = calloc(H * W0, sizeof(energy));
  spins    = malloc(H * W * sizeof(spin));

  init_spins(spins, H * W);

  while (running) {
    compute_energies(spins, energies);
    update_spins(energies, spins);

    render(spins, energies);
    fwrite(energies, sizeof(char), H * W0, stdout);

    printf("(T, P, FPS) = (%lf, %lf, %2d)", T, P, FPS);
    printf("\r\x1b[%dA", H);
    usleep(1000000 / FPS);
    if (need_resize)
      window_resize();
    while (do_pause);
  }

  free(spins);
  free(energies);
}

void init_spins(spin *spins, size_t n) {

  size_t m = n / sizeof(int);
  for (size_t i = 0; i < m; i++) {
    union { int as_int; spin ss[sizeof(int)]; } r =
      { .as_int = rand() & 0x0202020202020202};

    #pragma GCC unroll sizeof(int)
    for (size_t k = 0; k < sizeof(int); k++)
      r.ss[k] -= 1;

    ((int*)spins)[i] = r.as_int;
  }

  for (size_t k = m * sizeof(int); k < n; k++)
    spins[k] = (rand() & 2) - 1;
}

// compute energies for each spin based on its 8 neighbors.
void compute_energies(spin *spins, energy *energies) {
  for (int i = 0; i < H; i++) {
    // offsets of this row, the previous, and the next (with wrap-around).
    int this_row = i                 * W;
    int prev_row = ((i + H - 1) % H) * W;
    int next_row = ((i + 1) % H)     * W;

    // inner loop is unrolled to avoid expensive mod operations.
    int j = 0;
    spin ul = spins[prev_row + W - 1]; // wrap around columns.
    spin u  = spins[prev_row + j    ];
    spin ur = spins[prev_row + j + 1];

    spin l  = spins[this_row + W - 1]; // wrap around columns.
    spin c  = spins[this_row + j    ];
    spin r  = spins[this_row + j + 1];

    spin dl = spins[next_row + W - 1]; // wrap around columns.
    spin d  = spins[next_row + j    ];
    spin dr = spins[next_row + j + 1];

    energies[i * W0 + j] = (c * (ul + u + ur + l + r + dl + d + dr)) << 1;
    for (j = 1; j < W - 1; j++) {

      ul = spins[prev_row + j - 1];
      u  = spins[prev_row + j    ];
      ur = spins[prev_row + j + 1];

      l  = spins[this_row + j - 1];
      c  = spins[this_row + j    ];
      r  = spins[this_row + j + 1];

      dl = spins[next_row + j - 1];
      d  = spins[next_row + j    ];
      dr = spins[next_row + j + 1];

      energies[i * W0 + j] = (c * (ul + u + ur + l + r + dl + d + dr)) << 1;
    }

    ul = spins[prev_row + j - 1];
    u  = spins[prev_row + j    ];
    ur = spins[prev_row + 0    ];

    l  = spins[this_row + j - 1];
    c  = spins[this_row + j    ];
    r  = spins[this_row + 0    ];

    dl = spins[next_row + j - 1];
    d  = spins[next_row + j    ];
    dr = spins[next_row        ];

    energies[i * W0 + j] = (c * (ul + u + ur + l + r + dl + d + dr)) << 1;
  }
}

float rand_uniform(void) {
  return (float) rand() / RAND_MAX;
}

char char_abs(char x) {
  char sign_bit = x >> 7;
  return (x ^ sign_bit) - sign_bit;
}

// update the grid of spins based on the current grid of energies.
void update_spins(energy *energies, spin *spins) {
  for (int i = 0; i < H; i++) {
    for (int j = 0; j < W; j++) {
      energy e = energies[i * W0 + j];
      spins[i * W + j] *=
        (!(rand_uniform() < P &&
          (e < 0 ||
          rand_uniform() < exp((float) -e / T))) << 1) - 1;
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

void *user_input_handler_thread(void *_unused) {
  (void) _unused;
  char c;
  while (running) {
    if (read(STDIN_FILENO, &c, 1) != 0) {

      switch (c) {
        case 27: // escape key
        case 'q':
        case 'Q': running = 0;                 break;

        case 'z': P *= 0.99;                   break;
        case 'Z': P = MAX(P - 0.1, 0.0);       break;
        case 'x': P = MIN(P * 1.01, 1.0);      break;
        case 'X': P = MIN(P + 0.1, 1.0);       break;

        case 'a': T *= 0.99;                   break;
        case 'A': T = MAX(T - 0.1, 0.0);       break;
        case 's': T *= 1.01;                   break;
        case 'S': T += 0.1;                    break;

        case 'd': FPS = MAX(FPS - 1, MIN_FPS); break;
        case 'f': FPS = MIN(FPS + 1, MAX_FPS); break;

        case 'p': do_pause = !do_pause;        break;
      }
    }
  }
  return NULL;
}

int set_window_dims(void) {
  struct winsize _winsize;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &_winsize) == -1)
    return 1;
  H = _winsize.ws_row - 1;
  W = _winsize.ws_col;
  W0 = W + 1;
  return 0;
}

void window_resize(void) {
  // update window dims, but first store the previous values.
  int H_prev = H;
  int W_prev = W;
  int W0_prev = W_prev + 1;
  set_window_dims();

  energy *energies_new = calloc(H * W0, sizeof(energy));
  spin   *spins_new    = malloc(H * W * sizeof(spin));

  // number of new rows/columns (if any) we need to copy/initialize.
  int h = MIN(H, H_prev);
  int w = MIN(W, W_prev);

  // copy existing energies/spins.
  for (int i = 0; i < h; i++) {
    memcpy(energies_new + i * W0, energies + i * W0_prev, w);
    memcpy(spins_new    + i * W,  spins    + i * W_prev,  w);
  }

  // random-initialize the new spins.
  for (int i = 0; i < h; i++)
    init_spins(spins_new + i * W + w, W - w);
  init_spins(spins_new + h * W, (H - h) * W);

  // update energies and spins!
  free(energies);
  free(spins);
  energies = energies_new;
  spins    = spins_new;

  need_resize = 0;
}

void signal_handler(int sig) {
  if (sig == SIGINT)
    running = 0;
  else if (sig == SIGWINCH)
    need_resize = 1;
}

struct termios orig_termios;
int set_terminal_raw_mode(void) {
  struct termios raw = orig_termios;
  raw.c_lflag &= ~(ECHO | ICANON);
  return tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void reset_terminal_mode(void) {
  printf("\x1b[?1049l"); // switch back from alternate screen.
  printf("\x1b[?25h");   // show cursor.
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); // restore term attributes.
}

int init(void) {
  /*
   * check that we are writing to a tty; register signal and exit handlers; init
   * orig_termios (for proper resetting to cooked mode); and set raw mode.
   */
  if (!isatty(fileno(stdout))) {
    fprintf(stderr, "stdout is not a tty\n");
    return 1;
  }
  if (signal(SIGINT, signal_handler) != 0) {
    fprintf(stderr, "Failed to set SIGINT handler: %s\n", strerror(errno));
    return 1;
  }
  if (signal(SIGWINCH, signal_handler) != 0) {
    fprintf(stderr, "Failed to set SIGWINCH handler: %s\n", strerror(errno));
    return 1;
  }
  if (tcgetattr(0, &orig_termios) != 0) {
    fprintf(stderr, "Failed to get original termios: %s\n", strerror(errno));
    return 1;
  }
  if (atexit(reset_terminal_mode) != 0) {
    fprintf(stderr, "Failed to set atexit: %s\n", strerror(errno));
    return 1;
  }
  if (set_terminal_raw_mode() != 0) {
    fprintf(stderr, "Failed to set raw mode: %s\n", strerror(errno));
    return 1;
  }
  pthread_t thd1;
  int pthread_create_errno =
    pthread_create(&thd1, NULL, user_input_handler_thread, NULL);
  if (pthread_create_errno != 0) {
    fprintf(stderr, "Failed to start user input handler thread: %s\n",
                    strerror(pthread_create_errno));
    return 1;
  }

  // set window dims -- use defaults on failure.
  if (set_window_dims() != 0) {
    H = 10;
    W = 10;
    W0 = W + 1;
  }

  printf("\x1b[?25l");   // hide cursor.
  printf("\x1b[?1049h"); // switch to alternate screen.

  srand(time(NULL));
  return 0;
}
