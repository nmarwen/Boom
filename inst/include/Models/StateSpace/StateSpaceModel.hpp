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

#ifndef BOOM_STATE_SPACE_MODEL_HPP_
#define BOOM_STATE_SPACE_MODEL_HPP_

#include <Models/DataTypes.hpp>
#include <Models/Policies/IID_DataPolicy.hpp>
#include <Models/Policies/PriorPolicy.hpp>
#include <Models/ZeroMeanGaussianModel.hpp>

#include <Models/StateSpace/StateSpaceModelBase.hpp>

namespace BOOM{

  namespace StateSpace {
    class MultiplexedDoubleData
        : public Data {
     public:
      MultiplexedDoubleData();
      MultiplexedDoubleData(double y);
      MultiplexedDoubleData * clone() const override;
      std::ostream & display(std::ostream &out) const override;
      void add_data(const Ptr<DoubleData> &data_point);

      double adjusted_observation() const;
      int sample_size() const;
      const DoubleData &double_data(int i) const;
      void set_value(double value, int i);

      // Returns true if data_ is empty, or if all elements of data_ are
      // missing.
      bool all_missing() const;

     private:
      std::vector<Ptr<DoubleData>> data_;
    };
  }  // namespace StateSpace

  class StateSpaceModel
      : public StateSpaceModelBase,
        public IID_DataPolicy<StateSpace::MultiplexedDoubleData>,
        public PriorPolicy {
   public:
    StateSpaceModel();
    StateSpaceModel(const Vector &y,
                    const std::vector<bool> &y_is_observed =
                       std::vector<bool>());
    StateSpaceModel(const StateSpaceModel &rhs);
    StateSpaceModel* clone()const override;

    int time_dimension() const override;
    double observation_variance(int t) const override;
    double adjusted_observation(int t) const override;
    bool is_missing_observation(int t) const override;
    ZeroMeanGaussianModel* observation_model() override;
    const ZeroMeanGaussianModel* observation_model() const override;
    void observe_data_given_state(int t) override;

    // Forecast the next nrow(newX) time steps given the current data,
    // using the Kalman filter.  The first column of the returned
    // Matrix is the mean of the forecast.  The second column is the
    // standard errors.
    Matrix forecast(int n);

    // Simulate the next n time periods, given current parameters and
    // state.
    Vector simulate_forecast(int n, const Vector &final_state);

    // Simulate the next n time periods given current parameters and a
    // specified set of observed data.  Uses negative_infinity() as a
    // signal for missing data.
    // Args:
    //   n:  The number of time periods to forecast.
    //   observed_data: A vector of observed data on which to base the
    //     forecast.  This might be different than the data used to
    //     fit the model.
    Vector simulate_forecast_given_observed_data(
        int n, const Vector &observed_data);

    // Run the Kalman filter over the set of observed data, using
    // negative_infinity() as a signal for missing data.  The .a and .P
    // elements in the returned ScalarKalmanStorage give the mean and
    // variance of the state one period after the last element in
    // observed_data.  t0 is notional time period for
    // observed_data[0], which will usually be 0.
    ScalarKalmanStorage filter_observed_data(const Vector &observed_data,
                                             int t0 = 0) const;

    // Return the vector of one-step-ahead predictions errors from a
    // holdout sample, following immediately after the training data.
    Vector one_step_holdout_prediction_errors(const Vector &holdout_y,
                                              const Vector &final_state) const;

    // Update the complete data sufficient statistics for the
    // observation model based on the posterior distribution of the
    // observation model error term at time t.
    //
    // Args:
    //   t: The time of the observation.
    //   observation_error_mean: Mean of the observation error given
    //     model parameters and all observed y's.
    //   observation_error_variance: Variance of the observation error given
    //     model parameters and all observed y's.
    void update_observation_model_complete_data_sufficient_statistics(
        int t,
        double observation_error_mean,
        double observation_error_variance) override;

    // Increment the portion of the log-likelihood gradient
    // pertaining to the parameters of the observation model.
    //
    // Args:
    //   gradient: The subset of the log likelihood gradient
    //     pertaining to the observation model.  The gradient will be
    //     incremented by the derivatives of log likelihood with
    //     respect to the observation model parameters.
    //   t:  The time index of the observation error.
    //   observation_error_mean: The posterior mean of the observation
    //     error at time t.
    //   observation_error_variance: The posterior variance of the
    //     observation error at time t.
    void update_observation_model_gradient(
        VectorView gradient,
        int t,
        double observation_error_mean,
        double observation_error_variance) override;

   private:
    Ptr<ZeroMeanGaussianModel> observation_model_;
    void setup();
  };
}  // namespace BOOM

#endif //BOOM_STATE_SPACE_MODEL_HPP_
