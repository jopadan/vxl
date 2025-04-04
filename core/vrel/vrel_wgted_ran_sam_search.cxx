// This is core/vrel/vrel_wgted_ran_sam_search.cxx
#include <iostream>
#include <vector>
#include <algorithm>
#include "vrel_wgted_ran_sam_search.h"
#include <vrel/vrel_objective.h>
#include <vrel/vrel_estimation_problem.h>
#include <vrel/vrel_util.h>

#include <cassert>
#ifdef _MSC_VER
#  include "vcl_msvc_warnings.h"
#endif

vrel_wgted_ran_sam_search::~vrel_wgted_ran_sam_search()
{
  if (own_generator_)
    delete generator_;
}


// ------------------------------------------------------------
bool
vrel_wgted_ran_sam_search::estimate(const vrel_estimation_problem * problem, const vrel_objective * obj_fcn)
{
  // assume the weights are already sorted.
  // get similarity weights
  const std::vector<double> & wgts = problem->similarity_weights();
  if (!wgts.empty())
  {
    is_sim_wgt_set_ = true;
    assert(wgts.size() == problem->num_samples());

    // sums up weights
    double sum_wgt = 0.0;
    for (double wgt : wgts)
      sum_wgt += wgt;

    // build probability interval
    double current_lower = 0.0;
    double next_lower;
    intervals_.resize(wgts.size());
    prob_interval one;
    for (unsigned i = 0; i < wgts.size(); ++i)
    {
      one.index_ = i;
      one.lower_ = current_lower;
      next_lower = current_lower + wgts[i] / sum_wgt;
      one.upper_ = next_lower;
      intervals_[i] = one;
      current_lower = next_lower;
    }
  }
  // call same function in base class
  return vrel_ran_sam_search::estimate(problem, obj_fcn);
}

// ------------------------------------------------------------
void
vrel_wgted_ran_sam_search::next_sample(unsigned int taken,
                                       unsigned int num_points,
                                       std::vector<int> & sample,
                                       unsigned int points_per_sample)
{
  typedef std::vector<prob_interval>::iterator interval_iter;

  if (generate_all_ || !is_sim_wgt_set_)
  {
    vrel_ran_sam_search::next_sample(taken, num_points, sample, points_per_sample);
    return;
  }

  if (num_points == 1)
  {
    sample[0] = 0;
  }
  else
  {
    unsigned int k = 0, counter = 0;
    prob_interval one;
    interval_iter iter;
    int id;
    while (k < points_per_sample) // This might be an infinite loop!
    {
      one.upper_ = generator_->drand32();
      iter = std::lower_bound(intervals_.begin(), intervals_.end(), one);
      // though this should not happen
      if (iter == intervals_.end())
        continue;

      // get index;
      id = iter->index_;
      ++counter;
      bool different = true;
      for (int i = k - 1; i >= 0 && different; --i)
        different = (id != sample[i]);
      if (different)
        sample[k++] = id, counter = 0;
      else if (counter > 1000)
      {
        std::cerr << "vrel_wgted_ran_sam_search::next_sample --- WARNING: "
                  << "drand32() generated 1000x the same value " << id << " from the range [0," << num_points - 1
                  << "]\n"
                  << " prob is " << one.lower_ << " in range [" << iter->lower_ << ", " << iter->upper_ << "]\n";
        sample[k++] = id + 1;
      }
    }
  }
}
