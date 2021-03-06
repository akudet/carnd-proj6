/*
 * particle_filter.cpp
 *
 *  Created on: Dec 12, 2016
 *      Author: Tiffany Huang
 */

#include <random>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <math.h> 
#include <iostream>
#include <sstream>
#include <string>
#include <iterator>

#include "particle_filter.h"

using namespace std;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
	// TODO: Set the number of particles. Initialize all particles to first position (based on estimates of 
	//   x, y, theta and their uncertainties from GPS) and all weights to 1. 
	// Add random Gaussian noise to each particle.
	// NOTE: Consult particle_filter.h for more information about this method (and others in this file).

	if (this->is_initialized) {
	    return;
	}
	this->is_initialized = true;
	this->num_particles = 1000;

	default_random_engine engine;

	normal_distribution<double> rand_x(x, std[0]);
	normal_distribution<double> rand_y(y, std[1]);
	normal_distribution<double> rand_theta(theta, std[2]);

	for (int i = 0; i < this->num_particles; i++) {
	    Particle particle;
	    particle.id = i;
	    particle.x = rand_x(engine);
	    particle.y = rand_y(engine);
	    particle.theta = rand_theta(engine);
	    particle.weight = 1;
	    this->particles.push_back(particle);
	    this->weights.push_back(particle.weight);
	}
}

void ParticleFilter::prediction(double delta_t, double std_pos[], double velocity, double yaw_rate) {
	// TODO: Add measurements to each particle and add random Gaussian noise.
	// NOTE: When adding noise you may find std::normal_distribution and std::default_random_engine useful.
	//  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
	//  http://www.cplusplus.com/reference/random/default_random_engine/

	default_random_engine engine;

	normal_distribution<double> rand_x(0, std_pos[0] * delta_t);
	normal_distribution<double> rand_y(0, std_pos[1] * delta_t);
	normal_distribution<double> rand_theta(0, std_pos[2] * delta_t);

	for (int i = 0; i < num_particles; ++i) {
		Particle &particle = particles[i];
		if (yaw_rate == 0) {
			particle.x += velocity * delta_t * cos(particle.theta);
			particle.y += velocity * delta_t * sin(particle.theta);
			particle.theta= particle.theta;
		} else {
			particle.x += (velocity / yaw_rate) * (sin(particle.theta + yaw_rate * delta_t) - sin(particle.theta));
			particle.y -= (velocity / yaw_rate) * (cos(particle.theta + yaw_rate * delta_t) - cos(particle.theta));
			particle.theta += yaw_rate * delta_t;
		}
		particle.x += rand_x(engine);
		particle.y += rand_y(engine);
		particle.theta += rand_theta(engine);
	}
}

void ParticleFilter::dataAssociation(std::vector<LandmarkObs> predicted, std::vector<LandmarkObs>& observations) {
	// TODO: Find the predicted measurement that is closest to each observed measurement and assign the 
	//   observed measurement to this particular landmark.
	// NOTE: this method will NOT be called by the grading code. But you will probably find it useful to 
	//   implement this method and use it as a helper during the updateWeights phase.

}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
		const std::vector<LandmarkObs> &observations, const Map &map_landmarks) {
	// TODO: Update the weights of each particle using a mult-variate Gaussian distribution. You can read
	//   more about this distribution here: https://en.wikipedia.org/wiki/Multivariate_normal_distribution
	// NOTE: The observations are given in the VEHICLE'S coordinate system. Your particles are located
	//   according to the MAP'S coordinate system. You will need to transform between the two systems.
	//   Keep in mind that this transformation requires both rotation AND translation (but no scaling).
	//   The following is a good resource for the theory:
	//   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
	//   and the following is a good resource for the actual equation to implement (look at equation 
	//   3.33
	//   http://planning.cs.uiuc.edu/node99.html

	vector<LandmarkObs> predicted;
	predicted.reserve(observations.size());
	for (int i = 0; i < num_particles; i++) {
		Particle &particle = particles[i];

		// for each observation find best landmark with local coordinate system, aka, dataAssociation
		// but the api they provided seem not right, em, predicated should be a ref and observations be const ref
		transform(observations.cbegin(), observations.cend(), predicted.begin(),
				[particle, map_landmarks](const LandmarkObs &observation) {
			// local to world system, rotate -theta and translate t
			auto x_f = observation.x * cos(-particle.theta) + observation.y * sin(-particle.theta);
			x_f += particle.x;
			auto y_f = observation.y * cos(-particle.theta) - observation.x * sin(-particle.theta);
			y_f += particle.y;
			auto landmark = min_element(map_landmarks.landmark_list.cbegin(), map_landmarks.landmark_list.cend(),
					[x_f, y_f](const Map::single_landmark_s &a, const Map::single_landmark_s &b) {
				return ((a.x_f - x_f + b.x_f - x_f) * (a.x_f - b.x_f) + (a.y_f - y_f + b.y_f - y_f) * (a.y_f - b.y_f)) < 0;
			});
			// world to local coord, translate -t and rotate theta
			auto xt = landmark->x_f - particle.x;
			auto yt = landmark->y_f - particle.y;
			auto x = xt * cos(particle.theta) + yt * sin(particle.theta);
			auto y = yt * cos(particle.theta) - xt * sin(particle.theta);
			return LandmarkObs{landmark->id_i, x, y};
		});

		double term_x = 0;
		double term_y = 0;
		for(int j = 0; j< observations.size();j++) {
			const auto &y = observations[j];
			const auto &y_pred = predicted[j];

			term_x += pow(y.x - y_pred.x, 2);
			term_y += pow(y.y - y_pred.y, 2);
		}
		particle.weight = exp(-0.5 * (term_x / pow(std_landmark[0], 2) + term_y / pow(std_landmark[1], 2)));
		weights[i] = particle.weight;
	}
}

void ParticleFilter::resample() {
	// TODO: Resample particles with replacement with probability proportional to their weight. 
	// NOTE: You may find std::discrete_distribution helpful here.
	//   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution

	default_random_engine engine;
	discrete_distribution<int> sampler(this->weights.begin(), this->weights.end());

	vector<Particle> nparticles;
	nparticles.reserve(this->particles.size());
	for (int i = 0; i < this->num_particles; i++) {
		nparticles.push_back(this->particles[sampler(engine)]);
	}
	this->particles = nparticles;
}

Particle ParticleFilter::SetAssociations(Particle& particle, const std::vector<int>& associations, 
                                     const std::vector<double>& sense_x, const std::vector<double>& sense_y)
{
    //particle: the particle to assign each listed association, and association's (x,y) world coordinates mapping to
    // associations: The landmark id that goes along with each listed association
    // sense_x: the associations x mapping already converted to world coordinates
    // sense_y: the associations y mapping already converted to world coordinates

    particle.associations= associations;
    particle.sense_x = sense_x;
    particle.sense_y = sense_y;
}

string ParticleFilter::getAssociations(Particle best)
{
	vector<int> v = best.associations;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<int>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseX(Particle best)
{
	vector<double> v = best.sense_x;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseY(Particle best)
{
	vector<double> v = best.sense_y;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
