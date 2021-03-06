/*
  Copyright (C) 2005-2017 Steven L. Scott

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
#include <Models/StateSpace/StateSpaceRegressionModel.hpp>
#include <Models/StateSpace/StateModels/StateModel.hpp>
#include <Models/StateSpace/Filters/SparseKalmanTools.hpp>
#include <Models/DataTypes.hpp>
#include <distributions.hpp>

namespace BOOM{
  namespace {
    typedef StateSpaceRegressionModel SSRM;
    typedef StateSpace::MultiplexedRegressionData MRD;
  }  // namespace

  MRD::MultiplexedRegressionData()
      : state_model_offset_(0)
  {}
  MRD::MultiplexedRegressionData(double y, const Vector &x)
      : state_model_offset_(0)
  {
    NEW(RegressionData, data_point)(y, x);
    add_data(data_point);
  }

  MRD::MultiplexedRegressionData(const std::vector<Ptr<RegressionData>> &data)
      : state_model_offset_(0)
  {
    for (const auto &d : data) {
      add_data(d);
    }
  }

  MRD * MRD::clone() const { return new MRD(*this); }

  std::ostream & MRD::display(std::ostream &out) const {
    out << "state model offset: " << state_model_offset_ << std::endl
        << std::setw(10) << " response "
        << " predictors " << std::endl;
    for (int i = 0; i < regression_data_.size(); ++i) {
      out << std::setw(10) << regression_data_[i]->y()
          << " " << regression_data_[i]->x() << std::endl;
    }
    return out;
  }

  double MRD::adjusted_observation(const GlmCoefs &coefficients) const {
    double ans = 0;
    for (int i = 0; i < regression_data_.size(); ++i) {
      const RegressionData &observation(regression_data(i));
      ans += observation.y() - coefficients.predict(observation.x());
    }
    return ans / sample_size();
  }

  const RegressionData &MRD::regression_data(int i) const {
    return *(regression_data_[i]);
  }

  Ptr<RegressionData> MRD::regression_data_ptr(int i) {
    return regression_data_[i];
  }

  //======================================================================
  void SSRM::setup() {
    observe(regression_->coef_prm());
    observe(regression_->Sigsq_prm());
    regression_->only_keep_sufstats(true);
  }

  SSRM::StateSpaceRegressionModel(int xdim)
      : regression_(new RegressionModel(xdim))
  {
    setup();
    // Note that in this constructor the regression model will still
    // need to have data added, so we can't call fix_xtx().  This
    // means that the xtx matrix will be re-computed with each trip
    // through the data.
  }

  SSRM::StateSpaceRegressionModel(const Vector &y, const Matrix &X,
                                  const std::vector<bool> &observed)
      : regression_(new RegressionModel(ncol(X)))
  {
    setup();
    int n = y.size();
    if (nrow(X) != n) {
      ostringstream msg;
      msg << "X and y are incompatible in constructor for "
          << "StateSpaceRegressionModel." << endl
          << "length(y) = " << n << endl
          << "nrow(X) = " << nrow(X) << endl;
      report_error(msg.str());
    }

    for (int i = 0; i < n; ++i) {
      NEW(RegressionData, dp)(y[i], X.row(i));
      if (!(observed.empty()) && !observed[i]) {
        dp->set_missing_status(Data::partly_missing);
      }
      add_data(dp);
    }

    // The cast is necessary because the regression model stores a Ptr
    // to a base class that does not supply fix_xtx();
    regression_->suf().dcast<NeRegSuf>()->fix_xtx();
  }

  SSRM::StateSpaceRegressionModel(const SSRM &rhs)
      : Model(rhs),
        StateSpaceModelBase(rhs),
        DataPolicy(rhs),
        PriorPolicy(rhs),
        regression_(rhs.regression_->clone())
  {
    setup();
  }

  SSRM * SSRM::clone() const {return new SSRM(*this);}

  void SSRM::add_data(Ptr<Data> dp) {
    Ptr<RegressionData> regression_data = dp.dcast<RegressionData>();
    if (!!regression_data) {
      add_data(regression_data);
      return;
    }

    Ptr<MRD> multiplexed_data = dp.dcast<MRD>();
    if (!!multiplexed_data) {
      add_data(multiplexed_data);
      return;
    }
    report_error("Could not cast to an appropriate data type.");
  }

  void SSRM::add_data(Ptr<RegressionData> dp) {
    NEW(MRD, multiplexed_data)();
    multiplexed_data->add_data(dp);
    multiplexed_data->set_missing_status(dp->missing());
    add_data(multiplexed_data);
  }

  void SSRM::add_data(Ptr<MRD> dp) {
    DataPolicy::add_data(dp);
    for (int i = 0; i < dp->sample_size(); ++i) {
      regression_model()->add_data(dp->regression_data_ptr(i));
    }
  }

  double SSRM::observation_variance(int t) const {
    const std::vector<Ptr<MRD>> &data(dat());
    double sigsq = regression_->sigsq();
    if (t >= data.size()) {
      return sigsq;
    } else {
      int n = data[t]->sample_size();
      if (n == 0) ++n;
      return sigsq / n;
    }
  }

  double SSRM::adjusted_observation(int t) const {
    return dat()[t]->adjusted_observation(regression_->coef());
  }

  bool SSRM::is_missing_observation(int t) const {
    return dat()[t]->missing() != Data::observed;
  }

  void SSRM::observe_data_given_state(int t) {
    if (!is_missing_observation(t)) {
      Ptr<MRD> dp(dat()[t]);
      double state_mean = observation_matrix(t).dot(state(t));
      for (int i = 0; i < dp->sample_size(); ++i) {
        const RegressionData &observation(dp->regression_data(i));
        regression_->suf()->add_mixture_data(
            observation.y() - state_mean, observation.x(), 1.0);
      }
    }
  }

  Matrix SSRM::forecast(const Matrix &newX) const {
    ScalarKalmanStorage ks = filter();
    Matrix ans(nrow(newX), 2);
    int t0 = time_dimension();
    for (int t = 0; t < nrow(ans); ++t) {
      ans(t,0) = regression_->predict(newX.row(t))
          + observation_matrix(t + t0).dot(ks.a);
      sparse_scalar_kalman_update(0,  // y is missing, so fill in a dummy value
                                  ks.a,
                                  ks.P,
                                  ks.K,
                                  ks.F,
                                  ks.v,
                                  true,  // forecasts are missing data
                                  observation_matrix(t + t0),
                                  observation_variance(t + t0),
                                  *state_transition_matrix(t + t0),
                                  *state_variance_matrix(t + t0));
      ans(t,1) = sqrt(ks.F);
    }
    return ans;
  }

  // TODO(stevescott):  test simulate_forecast
  Vector SSRM::simulate_forecast(const Matrix &newX,
                                 const Vector &final_state) {
    StateSpaceModelBase::set_state_model_behavior(StateModel::MARGINAL);
    Vector ans(nrow(newX));
    int t0 = time_dimension();
    Vector state = final_state;
    for (int t = 0; t < ans.size(); ++t) {
      state = simulate_next_state(state, t + t0);
      ans[t] = rnorm(observation_matrix(t + t0).dot(state),
                     sqrt(observation_variance(t + t0)));
      ans[t] += regression_->predict(newX.row(t));
    }
    return ans;
  }

  Vector SSRM::simulate_forecast(const Matrix &newX) {
    StateSpaceModelBase::set_state_model_behavior(StateModel::MARGINAL);
    ScalarKalmanStorage kalman_storage = filter();
    // The Kalman filter produces the forecast distribution for the
    // next time period.  Since the observed data goes from 0 to t-1,
    // kalman_storage contains the forecast distribution for time t.
    Vector final_state = rmvn_robust(kalman_storage.a, kalman_storage.P);
    return simulate_forecast(newX, final_state);
  }

  Vector SSRM::one_step_holdout_prediction_errors(
      const Matrix &newX,
      const Vector &newY,
      const Vector &final_state) const {
    if (nrow(newX) != length(newY)) {
      report_error("X and Y do not match in StateSpaceRegressionModel::"
                   "one_step_holdout_prediction_errors");
    }

    Vector ans(nrow(newX));
    int t0 = time_dimension();
    ScalarKalmanStorage ks(state_dimension());
    ks.a = *state_transition_matrix(t0-1) * final_state;
    ks.P = SpdMatrix(state_variance_matrix(t0-1)->dense());

    for (int t = 0; t < ans.size(); ++t) {
      bool missing = false;
      sparse_scalar_kalman_update(
          newY[t] - regression_model()->predict(newX.row(t)),
          ks.a,
          ks.P,
          ks.K,
          ks.F,
          ks.v,
          missing,
          observation_matrix(t + t0),
          observation_variance(t + t0),
          *state_transition_matrix(t + t0),
          *state_variance_matrix(t + t0));
      ans[t] = ks.v;
    }
    return ans;
  }

  Vector SSRM::regression_contribution() const {
    const std::vector<Ptr<MRD>> &data(dat());
    Vector ans(data.size());
    for (int time = 0; time < data.size(); ++time) {
      Ptr<MRD> dp = data[time];
      double average_contribution = 0;
      for (int j = 0; j < data[time]->sample_size(); ++j) {
        average_contribution +=
            regression_model()->predict(dp->regression_data(j).x());
      }
      ans[time] = average_contribution /= dp->sample_size();
    }
    return ans;
  }

}  // namespace BOOM
