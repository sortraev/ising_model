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
#include <errno.h>

#include <stdint.h>

// visualization stuff. ignore this.
#define MIN_FPS 4
#define MAX_FPS 60
int FPS = 24;

// screen (and simulation grid) dimensions.
int H = 10;
int W = 10;
int W0; // width of internal string representation.

int RUNNING = 1;

#define MAX(x, y) ((x) >  (y) ? (x) : (y))
#define MIN(x, y) ((x) <= (y) ? (x) : (y))

typedef char spin;
typedef char energy;

void init_spins(spin *spins);
void compute_energies(spin *spins, energy *energies);
void update_spins(energy *energies, spin *spins);
void render(spin *spins, char *screen);

void *user_input_handler_thread(void *_unused);
int init();

// simulation parameters.
double T = 0.5;
double P = 0.1;

int main() {

  if (init() != 0) {
    return 1;
  }


  energy *energies = calloc(H * W0, sizeof(energy));
  spin   *spins    = malloc(H * W * sizeof(spin));

  init_spins(spins);

  while (RUNNING) {
    compute_energies(spins, energies);
    update_spins(energies, spins);

    render(spins, energies);
    fwrite(energies, sizeof(char), H * W0, stdout);

    printf("\n(T, P, FPS) = (%lf, %lf, %2d)", T, P, FPS);
    printf("\r\x1b[%dA", H + 1);
    usleep((1000000 / FPS));
  }

  free(spins);
  free(energies);
}

void init_spins(spin *spins) {
  for (int i = 0; i < H * W; i++)
    spins[i] = (rand() & 2) - 1; // -1 and 1.
}

// compute energies for each spin based on its 8 neighbors.
void compute_energies(spin *spins, energy *energies) {
  for (int i = 0; i < H; i++) {
    // offsets of this row, the previous, and the next (with wrap-around).
    int this_row = i                 * W;
    int prev_row = ((i + H - 1) % H) * W;
    int next_row = ((i + 1) % H)     * W;


    // inner loop is unrolled to avoid expensive mod operations.
    spin ul = spins[prev_row + W - 1]; // wrap around columns.
    spin u  = spins[prev_row        ];
    spin ur = spins[prev_row     + 1];

    spin l  = spins[this_row + W - 1]; // wrap around columns.
    spin c  = spins[this_row        ];
    spin r  = spins[this_row     + 1];

    spin dl = spins[next_row + W - 1]; // wrap around columns.
    spin d  = spins[next_row        ];
    spin dr = spins[next_row     + 1];

    int j = 0;
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

float rand_uniform() {
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

void sigint_handler(int _sig) {
  (void) _sig;
  RUNNING = 0;
}

struct termios orig_termios;
int set_terminal_raw_mode() {
  struct termios raw = orig_termios;
  raw.c_lflag &= ~(ECHO | ICANON);
  return tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void reset_terminal_mode() {
  printf("\x1b[?1049l"); // switch back from alternate screen.
  printf("\x1b[?25h");   // show cursor.
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); // restore term attributes.
}

int init() {
  /*
   * check that we are writing to a tty; register SIGINT and exit handlers; init
   * orig_termios (for proper resetting to cooked mode); and set raw mode.
   */
  if (!isatty(fileno(stdout))) {
    fprintf(stderr, "stdout is not a tty\n");
    return 1;
  }
  if (signal(SIGINT, sigint_handler) != 0) {
    fprintf(stderr, "Failed to set SIGINT handler: %s\n", strerror(errno));
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


  /*
   * init window dimensions. keep defaults on failure.
   */
  struct winsize w;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != -1) {
    H = w.ws_row - 1;
    W = w.ws_col;
  }
  W0 = W + 1;

  printf("\x1b[?25l");   // hide cursor.
  printf("\x1b[?1049h"); // switch to alternate screen.

  srand(43);
  return 0;
}
