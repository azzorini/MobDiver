#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <list>
#include <cmath>
#include <random>
#include <utility>
#include <stdexcept>
#include <cfloat>

// Seed for the random number generator
const unsigned SEED = 124346;

// Enumerates for types and interactions
enum types : short unsigned {TYPE_A, TYPE_B, TYPE_C, EMPTY};
enum interactions : short unsigned {SELECTION, REPRODUCTION, EXCHANGE, ERROR};

// Winning table
const types winningTable[3][3] = {{EMPTY, TYPE_A, TYPE_C}, {TYPE_A, EMPTY, TYPE_B}, {TYPE_C, TYPE_B, EMPTY}};

class Game2D {
	// Rates (simulation constants)
	double sigma;
	double mu;
	double epsilon;

	// Square lattice length
	unsigned L;

	// Lattice
	std::vector<short unsigned> animals;

	// Neighbours
	std::vector< std::vector<unsigned> > neighbours;

	// Total escape rate
	double W;

	// Time
	double t;

	// Random numbers
	std::mt19937 gen;
	std::uniform_real_distribution<double> ran_u;

	// Internal methods
	
	// Utility that convert from 2D to 1D
	unsigned int pos_by_indeces(unsigned int i, unsigned int j) const {
		return i*L + j;
	}

	// Utility that calculates the neighbours
	void calculate_neighbours();

	// Utility that calculates the contribution to W of a certain index
	double W_ind(unsigned);
public:
	Game2D(unsigned, double, double, double, double, unsigned);

	Game2D(const Game2D& o) : sigma(o.sigma), mu(o.mu), epsilon(o.epsilon), L(o.L), animals(o.animals),
	neighbours(o.neighbours), W(o.W), t(o.t), gen(o.gen), ran_u(o.ran_u) {}

	// Calculates the total scape rate
	double calculate_W() const;

	// Setters
	void set_M(double M) {
		epsilon = .5*M*L*L;
		// After changin a rate we need to recompute W
		W = calculate_W();
	}
	void set_selec(double selec) {
		sigma = selec;
		// After changin a rate we need to recompute W
		W = calculate_W();
	}
	void set_repr(double repr) {
		mu = repr;
		// After changin a rate we need to recompute W
		W = calculate_W();
	}
	void set_t(double time) { t = time; }

	// Getters
	double get_t() const { return t; }
	double get_W() const { return W; }
	double get_M() const { return 2*epsilon/L/L; }
	unsigned get_L() const { return L; }
	double get_selec() const { return sigma; }
	double get_repr() const { return mu; }

	// Operators
	const short unsigned& operator[](unsigned i, unsigned j) const { return animals[pos_by_indeces(i, j)]; }
	// Prefix increment (it will be the only one implemented to avoid copies)
	// it implements one time step of the Gillespie algorithm
	Game2D& operator++();

	// Save to PPM image
	void save_to_ppm(const std::string&) const;
	// Save to txt
	void save_to_txt(const std::string&) const;

	// Load from txt
	void load_from_txt(const std::string&);

	// Set system in a random state
	void randomize();
};

Game2D::Game2D(unsigned length = 2, double M = 3e-6, double selec = 1, double repr = 1, double t0 = .0, unsigned seed = SEED)
: sigma(selec), mu(repr), epsilon(.5*M*length*length), L(length), animals(length*length),
  neighbours(4, std::vector<unsigned>(length*length)), t(t0), gen(seed), ran_u(DBL_TRUE_MIN, 1.) {
	  // Random number generator to fill the lattice
	  std::uniform_int_distribution<short unsigned> ran_t(TYPE_A, EMPTY);
	  // We fill the lattice randomly
	  for (unsigned i = 0; i < length*length; i++) animals[i] = ran_t(gen);
	  // We compute the neighbours
	  calculate_neighbours();
	  // We compute the total escape rate
	  W = calculate_W();
}

void Game2D::calculate_neighbours() {
	unsigned int i, j;
	for (i = 0; i < L; i++)
		for (j = 0; j < L; j++) {
			neighbours[0][pos_by_indeces(i, j)] = pos_by_indeces((i > 0 ? i-1 : L-1), j);
			neighbours[1][pos_by_indeces(i, j)] = pos_by_indeces(i, (j > 0 ? j-1 : L-1));
			neighbours[2][pos_by_indeces(i, j)] = pos_by_indeces((i < L-1 ? i+1 : 0), j);
			neighbours[3][pos_by_indeces(i, j)] = pos_by_indeces(i, (j < L-1 ? j+1 : 0));
		}
}

double Game2D::W_ind(unsigned i) {
	double W_r = .0;

	for (unsigned n = 0; n < 4; n++) {
		if (animals[i] != animals[neighbours[n][i]]) {
			// Independenly if this is a pair of animals
			// or an animal and an empty space they can swap positions
			W_r += epsilon;

			if (animals[i] == EMPTY || animals[neighbours[n][i]] == EMPTY) W_r += mu;
			else W_r += sigma;
		}
	}

	return W_r;
}

double Game2D::calculate_W() const {
	unsigned i, n, L2 = L*L;
	double W_calc = .0;

	for (i = 0; i < L2; i++) {
		for (n = 0; n < 2; n++) {
			if (animals[i] != animals[neighbours[n][i]]) {
				// Independenly if this is a pair of animals
				// or an animal and an empty space they can swap positions
				W_calc += epsilon;

				if (animals[i] == EMPTY || animals[neighbours[n][i]] == EMPTY) W_calc += mu;
				else W_calc += sigma;
			}
		}
	}

	return W_calc;
}

Game2D& Game2D::operator++() {
	unsigned i, n, n_ind, N = L*L;
	short unsigned op = ERROR;
	double W_ini, W_fin, ran_W, sum = .0;

	// We increment the time
	t -= std::log(ran_u(gen))/W;

	// We get a random number between 0 and W
	ran_W = ran_u(gen)*W;

	// We select a neighbour and a interaction
	for (i = 0; i < N; i++) {
		// Loop for neighbours
		for (n = 0; n < 2; n++) {
			n_ind = neighbours[n][i];
			if (animals[i] != animals[n_ind]) {
				// We count the exchange possibility
				sum += epsilon;
				if (sum > ran_W) {
					op = EXCHANGE;
					goto change;
				}
				if (animals[i] == EMPTY || animals[n_ind] == EMPTY) {
					sum += mu;
					if (sum > ran_W) {
						op = REPRODUCTION;
						goto change;
					}
				} else { // They are two animals of different type
					sum += sigma;
					if (sum > ran_W) {
						op = SELECTION;
						goto change;
					}
				}
			}
		}
	}

change:
	switch (op) {
		case EXCHANGE:
			W_ini = W_ind(i) + W_ind(n_ind);
			std::swap(animals[i], animals[n_ind]);
			W_fin = W_ind(i) + W_ind(n_ind);
			break;
		case REPRODUCTION:
			if (animals[i] == EMPTY) {
				W_ini = W_ind(i);
				animals[i] = animals[n_ind];
				W_fin = W_ind(i);
			} else {
				W_ini = W_ind(n_ind);
				animals[n_ind] = animals[i];
				W_fin = W_ind(n_ind);
			}
			break;
		case SELECTION:
			if (animals[i] == winningTable[animals[i]][animals[n_ind]]) n = n_ind;
			else n = i;
			W_ini = W_ind(n);
			animals[n] = EMPTY;
			W_fin = W_ind(n);
			break;
		default:
			throw std::runtime_error("Invalid operation encountered. There are no operations available.\nAll the animals are the same.");
	}

	W += W_fin - W_ini;

	return *this;
}

// Operator in order to show the Game2D class
std::ostream& operator<<(std::ostream& os, const Game2D& g) {
	unsigned i, j, L = g.get_L();

	for (i = 0; i < L; i++) {
		os << g[i, 0];
		for (j = 1; j < L; j++) os << '\t' << g[i, j];
		os << '\n';
	}

	return os;
}

void Game2D::save_to_ppm(const std::string& filename = "screenshot.ppm") const {
	unsigned i, N = L*L;
	std::ofstream fout(filename);

	fout << "P3\n" << L << ' ' << L << "\n255\n";

	for (i = 0; i < N; i++) {
		switch (animals[i]) {
			case TYPE_A:
				fout << "255 0 0\n";
				break;
			case TYPE_B:
				fout << "0 0 255\n";
				break;
			case TYPE_C:
				fout << "255 255 0\n";
				break;
			default:
				fout << "0 0 0\n";
		}
	}

	fout.close();
}

void Game2D::save_to_txt(const std::string& filename = "saved_state.txt") const {
	unsigned i, j;
	std::ofstream fout(filename);

	for (i = 0; i < L; i++) {
		fout << animals[pos_by_indeces(i, 0)];
		for (j = 0; j < L; j++) fout << ' ' << animals[pos_by_indeces(i, j)];
		fout << '\n';
	}

	fout.close();
}

void Game2D::load_from_txt(const std::string& filename) {
	unsigned i, j;
	std::ifstream fin(filename);

	for (i = 0; i < L; i++) {
		for (j = 0; j < L; j++) {
			if (!(fin >> animals[pos_by_indeces(i, j)]))
				throw std::runtime_error("The file does not have enough data");
		}
	}
	fin.close();

	W = calculate_W();
}

void Game2D::randomize() {
	  // Random number generator to fill the lattice
	  std::uniform_int_distribution<short unsigned> ran_t(TYPE_A, EMPTY);

	  // We fill the lattice randomly
	  for (unsigned i = 0; i < L*L; i++) animals[i] = ran_t(gen);

	  W = calculate_W();
}

// This function will only work up to 99999 frames
inline std::string frame_to_string(unsigned n) {
	std::string result = std::to_string(n);
	return std::string(5 - result.size(), '0') + result;
}

int main() {
	const unsigned L = 100;
	const double TMAX = L*L;
	const double M = 3e-4;
	const double frame_gap = 1;

	Game2D sim(L, M);
	double t, frame_count = frame_gap;
	unsigned nframe = 1;

	sim.save_to_ppm("FinalWorkFrames/" + frame_to_string(0) + ".ppm");
	t = 0;
	while (t < TMAX) {
		try {
			++sim;
		} catch (std::runtime_error const& e) {
			std::cout << e.what() << '\n';
			break;
		} 

		t = sim.get_t();

		if (frame_count < t) {
			sim.save_to_ppm("FinalWorkFrames/" + frame_to_string(nframe) + ".ppm");
			nframe++;
			frame_count += frame_gap;

		}
	}

	std::cout << "Simulation ended at time t = " << sim.get_t() << ".\n";
	sim.save_to_txt("saved_state_L_" + std::to_string(L) + "_M_" + std::to_string(M*1e6) + "e-6.txt");

	return 0;
}
