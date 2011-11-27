// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file /protocols/moves/MetropolisHastingsMover.hh
/// @brief
/// @author  Oliver Lange ( oliver.lange@tum.de )

#ifndef INCLUDED_protocols_moves_SimulatedTempering_hh
#define INCLUDED_protocols_moves_SimulatedTempering_hh

// Unit Headers
#include <protocols/moves/SimulatedTempering.fwd.hh>
#include <protocols/moves/TemperingBase.hh>

#include <protocols/moves/Mover.hh>

// Project Headers
#include <protocols/moves/MonteCarlo.fwd.hh>
#include <core/pose/Pose.fwd.hh>
#include <numeric/random/WeightedSampler.hh>
#include <protocols/jd2/Job.fwd.hh>

// Utility Headers
#include <core/types.hh>
#include <utility/vector1.hh>

namespace protocols {
namespace moves {

///@details
class SimulatedTempering : public TemperingBase {
	typedef TemperingBase Parent;
public:

	SimulatedTempering();

	SimulatedTempering( SimulatedTempering const& );

	virtual
	void apply( core::pose::Pose& ) {};

	virtual
	std::string
	get_name() const;

	MoverOP
	clone() const;

	virtual
	MoverOP
	fresh_instance() const;

	virtual
	void
	parse_my_tag(
		utility::tag::TagPtr const tag,
		DataMap & data,
		protocols::filters::Filters_map const & filters,
		Movers_map const & movers,
		core::pose::Pose const & pose
	);

	/// @brief execute the temperatur move ( called by observer_after_metropolis )
	/// returns the current temperatur in kT.
	core::Real
	temperature_move( core::Real score);

	/// @brief callback executed before any Monte Carlo trials
	virtual void
	initialize_simulation();

	/// @brief callback executed after all Monte Carlo trials
	virtual
	void
	finalize_simulation(
		core::pose::Pose & pose,
		protocols::moves::MetropolisHastingsMover const & metropolis_hastings_mover
	);

	void
	finalize_simulation( std::string const& output_name );

protected:
	void set_defaults();

	/// @brief Assigns user specified values to primitive members using command line options
	virtual
	void init_from_options();

	/// @brief update weights based on current counts
	void reweight();

	/// @brief reset the raw counts per state (not the weighted ones) to 0
	void reset_raw_counter();

	/// @brief initialize temperatures and weights from file, return false if IO error occurrs
	virtual
	bool initialize_from_file( std::string const& filename );

	virtual
	void write_to_file( std::string const& file_in, std::string const& output_name, utility::vector1< core::Real > const& wcounts );

/// ------------------ register cmdline options ---------------------------

private:
	static bool options_registered_;

public:
	static void register_options();

/// ---------------- member variables --------------------------

private:
	utility::vector1< core::Real > weights_;
	utility::vector1< core::Size > counts_;
	utility::vector1< core::Real > weighted_counts_;
	core::Size total_count_;


	core::Real score_offset_;

	core::Real self_transition_;
	// allows jumps to any temperature in single step
	bool temperature_jumps_;
	// reweight after X steps -- 0 for now reweighting
	core::Size reweight_stride_;

}; //end SimulatedTempering

} //namespace moves
} //namespace protocols

#endif //INCLUDED_protocols_moves_SimulatedTempering_HH
