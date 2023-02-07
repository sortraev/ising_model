// #include <time.h>    // time(), clock()
#include <stdio.h>   // printf()
#include <stdlib.h>  // malloc(), rand()
#include <unistd.h>  // usleep()
#include <math.h>    // exp()
#include <pthread.h> // pthread stuff
#include <string.h>  // memset()
#include <signal.h>  // signal()


// visualization stuff. ignore this.
#define SHOW 1         // actually show visualization, or just compute simulation?
#define SECS 3600      // run simulation for this many seconds.
#define FPS  24       // frames per second of visualization.
#define N (SECS * FPS) // number of steps in simulation.
#define SLEEP_TIME (1000000 / FPS) /* time between renderings. */
#define COLOR 1

#define COLOR_BLUE  "\x1b[35m"
#define COLOR_RED   "\x1b[31m"
#define RESET_COLOR "\x1b[0m"

#define RAND_UNIFORM() ((float) rand() / RAND_MAX)

typedef char spin;
typedef char energy;

// ferromagnet grid dimensions.
int H = 79;
int W = 234;
int W0; // width of string representation.

void init_spins(spin *spins);
void scramble_spins(spin *spins);
void compute_energies(spin *spins, energy *energies);
void update_spins(energy *energies, spin *spins);
void render(spin *spins, char *screen);

int running = 1;
void sigint_handler(int sig); // sets running = 0 on SIGINT.

static inline char char_abs(char x) {
  char sign_bit = x >> 7;
  return (x ^ sign_bit) - sign_bit;
}

// simulation parameters.
#define MUTATE 0
#define T_INC  0.04f
#define P_INC  0.002f
double T = 0.137;
double P = 0.04;

int main(int argc, char **argv) {

  if (argc == 3) {
    H = atoi(argv[1]);
    W = atoi(argv[2]);
  }
  else if (argc != 1) {
    fprintf(stderr, "Unexpected number of arguments (expected 0 or 2).\n");
    exit(1);
  }

  if (signal(SIGINT, sigint_handler) != 0) {
    fprintf(stderr, "Error setting SIGINT handler.\n");
    exit(1);
  }

  W0 = W + 1;
  srand(43);


  // allocate spins and energies.
  spin   *spins    = malloc(H * W * sizeof(spin));
  energy *energies = calloc(H * W0, sizeof(energy));

  // memset(energies, ' ', H * W0 * sizeof(energy));
  // for (int i = 0; i < H; i++)
    // energies[i * W0 + W0] = '\n';

  init_spins(spins);

#if COLOR
  printf(COLOR_RED); // red color output.
#endif
  clock_t start = clock();

  int i = 1;
#if MUTATE
  while (running && P < 0.2 && T < 3.5) {
    T += !(i & 7) * T_INC;
    T += !(i & 7) * P_INC;
    i++;
#else
  while (running) {
#endif

    compute_energies(spins, energies);
    update_spins(energies, spins);

#if SHOW
    render(spins, energies);
    fwrite(energies, sizeof(char), H * W0, stdout);
    printf("\r\033[%dA", H);
    usleep(SLEEP_TIME);
#endif
// #if MUTATE
// #endif
  }


  for (int i = 0; i < N && running; i++) {

    compute_energies(spins, energies);
    update_spins(energies, spins);

#if SHOW
    render(spins, energies);
    fwrite(energies, sizeof(char), H * W0, stdout);
    printf("\033[%dA\n", H);
    usleep(SLEEP_TIME);
#endif
  }

  clock_t diff = clock() - start;

  int msec = diff * 1000 / CLOCKS_PER_SEC;
#if COLOR
  printf("\033[%dB%s\n", H, RESET_COLOR);
#endif

#if !SHOW
  render(spins, energies);
  fwrite(energies, sizeof(char), H * W0, stdout);
  printf("Time taken %d seconds %d milliseconds\n", msec / 1000, msec % 1000);
#endif


  free(spins);
  free(energies);
}


void init_spins(spin *spins) {
  for (int i = 0; i < H; i++)
    for (int j = 0; j < W; j++)
      spins[i * W + j] = (rand() & 2) - 1; // random zeroes and ones.
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
          RAND_UNIFORM() < exp(((float) -e) / T))) * 2 - 1;
    }
  }
}


// "render" the grid of spins by filling out a screen array with corresponding
// symbols.
void render(spin *spins, char *energies) {
  static const char symbols[6] = " .o*%#";
  for (int i = 0; i < H; i++)
    for (int j = 0; j < W; j++)
      energies[i * W0 + j] =
        symbols[(char_abs(energies[i * W0 + j]) / 4 + 1)
                 * (spins[i * W + j] > 0)];
}

void sigint_handler(int sig) {
  (void) sig;
  running = 0;
}


/*
 * a more readable energy function.
void compute_energies(spin *spins, energy *energies) {
  for (int i = 0; i < H; i++) {
    for (int j = 0; j < W; j++) {

      spin ul = spins[MOD(i - 1, H) * W + MOD(j - 1, W)];
      spin u  = spins[MOD(i - 1, H) * W + j];
      spin ur = spins[MOD(i - 1, H) * W + ((j + 1) % W)];
      spin l  = spins[i * W + MOD(j - 1, W)];
      spin c  = spins[i * W + j];
      spin r  = spins[i * W + ((j + 1) % W)];
      spin dl = spins[((i + 1) % H) * W + MOD(j - 1, W)];
      spin d  = spins[((i + 1) % H) * W + j];
      spin dr = spins[((i + 1) % H) * W + ((j + 1) % W)];

      energies[i * W0 + j] = 2 * c * (ul + u + ur + l + r + dl + d + dr);
    }
  }
}
*/
