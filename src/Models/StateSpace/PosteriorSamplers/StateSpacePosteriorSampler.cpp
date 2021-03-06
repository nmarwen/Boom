/*
  Copyright (C) 2005-2011 Steven L. Scott

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/

#include <Models/StateSpace/PosteriorSamplers/StateSpacePosteriorSampler.hpp>
#include <cpputil/math_utils.hpp>
#include <numopt.hpp>
#include <TargetFun/TargetFun.hpp>

namespace BOOM{

  typedef StateSpacePosteriorSampler SSPS;
  typedef StateSpaceModelBase SSMB;

  SSPS::StateSpacePosteriorSampler(StateSpaceModelBase *model,
                                   RNG &seeding_rng)
      : PosteriorSampler(seeding_rng),
        model_(model),
        latent_data_initialized_(false)
  {}

  void SSPS::draw(){
    if (!latent_data_initialized_) {
      model_->impute_state();
      latent_data_initialized_ = true;
    }
    impute_nonstate_latent_data();
    model_->observation_model()->sample_posterior();
    for(int s = 0; s < model_->nstate(); ++s) {
      model_->state_model(s)->sample_posterior();
    }
    model_->impute_state();
    // End with a call to impute_state() so that the internal state of
    // the Kalman filter matches up with the parameter draws.
  }

  double SSPS::logpri()const{
    double ans = model_->observation_model()->logpri();
    for(int s = 0; s < model_->nstate(); ++s){
      ans += model_->state_model(s)->logpri();
    }
    return ans;
  }

  void SSPS::Mstep() {
    for (int i = 0; i < model_->nstate(); ++i) {
      model_->state_model(i)->find_posterior_mode();
    }
    model_->observation_model()->find_posterior_mode();
  }

  void SSPS::find_posterior_mode(double epsilon) {
    if (model_->check_that_em_is_legal()) {
      find_posterior_mode_using_em(epsilon, 500);
    }
    find_posterior_mode_numerically(epsilon);
  }

  void SSPS::find_posterior_mode_using_em(double epsilon, int max_steps) {
    model_->clear_client_data();
    double old_logp = model_->Estep(false) + logpri();
    double crit = 1 + epsilon;
    int em_steps = 0;
    while((crit > std::min(1.0, epsilon)) &&
          (em_steps++ < max_steps)) {
      Mstep();
      model_->clear_client_data();
      double logp = model_->Estep(false) + logpri();
      crit = logp - old_logp;
      old_logp = logp;
      if (crit < -.01) {
        // A small decrease in log posterior might be due to
        // numerical issues, but log posterior should never
        // decrease.
        report_error("EM iteration reduced the log posterior.");
      }
    }
  }

  namespace {
    class StateSpaceLogPosterior
        : public dTargetFun {
     public:
      StateSpaceLogPosterior(
          const StateSpaceModelBase *model,
          const StateSpacePosteriorSampler *prior)
          : model_(model),
            prior_(prior)
      {}

      double operator()(const Vector &parameters) const override {
        StateSpaceUtils::LogLikelihoodEvaluator evaluator(model_);
        return evaluator.evaluate_log_posterior(parameters);
      }

      double operator()(const Vector &parameters,
                        Vector &gradient) const override {
        double ans = model_->log_likelihood_derivatives(parameters, gradient);
        ans += prior_->increment_log_prior_gradient(parameters,
                                                    VectorView(gradient));
        return ans;
      }

     private:
      const StateSpaceModelBase *model_;
      const StateSpacePosteriorSampler *prior_;
    };
  }

  void SSPS::find_posterior_mode_numerically(double epsilon) {
    StateSpaceLogPosterior log_posterior(model_, this);
    Vector parameters = model_->vectorize_params(true);
    double log_posterior_value = log_posterior(parameters);
    std::string error_message;
    bool ok = max_nd1_careful(
        parameters,
        log_posterior_value,
        log_posterior,
        log_posterior,
        error_message,
        epsilon,
        500,
        ConjugateGradient);
    if (!ok) {
      ostringstream err;
      err << "Numerical search for posterior mode failed with error message: "
          << endl << error_message;
      report_error(err.str());
    } else {
      model_->unvectorize_params(parameters);
    }
  }

  double SSPS::log_prior_density(const ConstVectorView &parameters) const {
    double ans = model_->observation_model()->log_prior_density(
        model_->observation_parameter_component(parameters));
    for (int s = 0; s < model_->nstate(); ++s) {
      ans += model_->state_model(s)->log_prior_density(
          model_->state_parameter_component(parameters, s));
    }
    return ans;
  }

  double SSPS::increment_log_prior_gradient(const ConstVectorView &parameters,
                                            VectorView gradient) const {
    // The model expects a const Vector & for parameters and a Vector
    // & for gradient.
    Vector parameter_vector(parameters);
    Vector gradient_vector(gradient);
    double ans = model_->observation_model()->increment_log_prior_gradient(
        model_->observation_parameter_component(parameter_vector),
        model_->observation_parameter_component(gradient_vector));

    for (int s = 0; s < model_->nstate(); ++s) {
      ans += model_->state_model(s)->increment_log_prior_gradient(
          model_->state_parameter_component(parameter_vector, s),
          model_->state_parameter_component(gradient_vector, s));
    }
    gradient = gradient_vector;
    return ans;
  }

}  // namespace BOOM
